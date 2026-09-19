#include "tash/mcp/content.hpp"
#include "tash/mcp/dispatcher.hpp"
#include "tash/mcp/mcp-client.hpp"
#include "tash/mcp/mcp-server.hpp"
#include "tash/mcp/protocol.hpp"
#include "tash/mcp/tool-context.hpp"
#include "tash/python/checkpoint-note.hpp"
#include "tash/python/checkpoint-store.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/report/render.hpp"
#include "tash/session/restore-probe.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tash/replay-command.hpp"
#include "tash/tash/run-host.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
  using tash::mcp::McpClient;
  using tash::utilities::Result;
  using ScratchArea = oxbox::platform::ScratchArea;

  // 600 frames of stepping at six times real time: long enough that the
  // calls refused while the job runs are certainly made while it runs.
  constexpr double JOB_RATE{ 6.0 };
  constexpr int    JOB_FRAMES{ 600 };
  constexpr int    POLL_LIMIT{ 600 };
  constexpr auto   POLL_BEAT{ std::chrono::milliseconds{ 20 } };

  // A budget a whole multiple of the step a looping job takes, so the
  // frames it runs before the refusal are the budget exactly.
  constexpr int    BUDGET_FRAMES{ 120 };
  constexpr int    BUDGET_STEP{ 10 };

  // Big enough that a cancel is what ends the loop, small enough that a
  // cancel that never arrives still ends the test.
  constexpr int    CANCEL_LIMIT{ 3000 };

  // How often Ended looks, and how long it waits for a server that should
  // already have gone.
  constexpr std::chrono::milliseconds ENDED_BEAT{ 20 };
  constexpr std::chrono::milliseconds ENDED_WAIT{ 1000 };
  constexpr std::chrono::milliseconds STILL_THERE{ 250 };

  auto Looping(int frames) -> std::string
  {
    return std::format("while True:\n    tash.run.step({})\n", frames);
  }

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  // The core, the interpreter and the endpoint all live on one thread, the
  // way `tash serve` has them: the fixture is that thread.
  class Served
  {
  public:
    explicit Served(std::filesystem::path checkpoints,
                    std::filesystem::path profile
                      = Root() / "examples/homebrew/profile.yaml")
    {
      // A profile names its ROM the way `tash` is run from, and ctest runs
      // a test from its own module's directory.
      std::filesystem::current_path(Root());
      std::promise<std::uint16_t> listening;
      std::future<std::uint16_t> ready{ listening.get_future() };
      _serving = std::thread{
        [this, &listening, checkpoints = std::move(checkpoints),
         profile = std::move(profile)]
        {
          tash::cli::RunHost host;
          host.Remember(tash::mcp::LaunchAsked{ profile });
          host.KeepCheckpointsUnder(checkpoints);
          tash::mcp::ToolContext tools{ host };
          tash::mcp::Dispatcher answering{ tools };

          auto opened{ tash::mcp::McpServer::Open(
            answering, tash::mcp::McpOptions{ "127.0.0.1", 0 }) };
          if (!opened)
          {
            listening.set_value(0);
            return;
          }
          _server = opened->get();
          (*opened)->Start();
          listening.set_value((*opened)->Port());
          (*opened)->Run();
          _server = nullptr;
        } };
      _port = ready.get();
    }

    ~Served()
    {
      if (tash::mcp::McpServer* const serving{ _server.load() })
        serving->Stop();
      if (_serving.joinable())
        _serving.join();
    }

    Served(Served const&)                    = delete;
    auto operator = (Served const&) -> Served& = delete;

    [[nodiscard]] auto Port() const noexcept -> std::uint16_t
    { return _port; }

    [[nodiscard]] auto Talking() -> McpClient
    { return McpClient{ "127.0.0.1", _port }; }

    // Whether the endpoint's own thread has come back, which is the server
    // having stopped without anybody stopping it.
    [[nodiscard]] auto Ended(std::chrono::milliseconds within) -> bool
    {
      auto const until{ std::chrono::steady_clock::now() + within };
      while (_server.load() != nullptr
             && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(ENDED_BEAT);
      return _server.load() == nullptr;
    }

  private:
    std::thread                 _serving{ };
    std::atomic<tash::mcp::McpServer*> _server{ nullptr };
    std::uint16_t               _port{ 0 };
  };

  // The frame number observe() opens with, zero when it says none.
  auto FrameIn(std::string const& said) -> std::uint64_t
  {
    std::size_t const at{ said.find("frame ") };
    if (at == std::string::npos)
      return 0;
    return std::stoull(said.substr(at + std::string_view{ "frame " }.size()));
  }

  // The line observe() prints its hashes on, which a restore reproduces.
  auto HashesIn(std::string const& said) -> std::string
  {
    std::size_t const at{ said.find("exact ") };
    if (at == std::string::npos)
      return { };
    return said.substr(at, said.find('\n', at) - at);
  }

  // The homebrew profile with one watch added, so the observation a moving
  // tool appends has a `watch` line in it too.
  auto WatchedProfile(ScratchArea const& scratch) -> std::filesystem::path
  {
    std::filesystem::path const written{ scratch.Path() / "watched.yaml" };
    std::ofstream writing{ written };
    writing << "name: zsenilia\n"
               "core: genesis_plus_gx\n"
               "rom: examples/homebrew/zsenilia.bin\n"
               "system_dir: \"\"\n"
               "save_dir: \"\"\n"
               "ports:\n"
               "  - joypad\n"
               "  - none\n"
               "watches:\n"
               "  - name: first\n"
               "    region: system\n"
               "    address: \"0x0000\"\n"
               "    width: 2\n"
               "    endian: little\n"
               "    is_signed: false\n";
    return written;
  }

  auto Forgot(std::filesystem::path const& root, std::string_view suffix)
    -> void
  {
    for (auto const& entry :
         std::filesystem::recursive_directory_iterator{ root })
      if (entry.path().extension() == suffix)
        std::filesystem::remove(entry.path());
  }

  // What another process writing the same name looks like from in here.
  auto Overwrote(std::filesystem::path const& root, std::string_view from,
                 std::string_view onto) -> void
  {
    for (auto const& entry :
         std::filesystem::recursive_directory_iterator{ root })
    {
      std::filesystem::path const file{ entry.path() };
      if (file.stem() != from)
        continue;
      std::filesystem::path target{ file };
      target.replace_filename(std::string{ onto }
                              + file.extension().string());
      std::filesystem::copy_file(
        file, target, std::filesystem::copy_options::overwrite_existing);
      std::filesystem::last_write_time(
        target, std::filesystem::file_time_type::clock::now());
    }
  }

  auto ExpectObserved(std::string const& said) -> void
  {
    EXPECT_NE(said.find("\nframe "), std::string::npos) << said;
    EXPECT_NE(said.find("\nwatch "), std::string::npos) << said;
  }

  // The bundle the launch answer names, on its own line.
  auto BundleIn(std::string const& said) -> std::filesystem::path
  {
    std::size_t const at{ said.find("bundle ") };
    if (at == std::string::npos)
      return { };
    std::size_t const from{ at + std::string_view{ "bundle " }.size() };
    return said.substr(from, said.find('\n', from) - from);
  }

  auto Launched(McpClient& talking, ScratchArea const& scratch)
    -> std::string
  {
    // No profile: the one the server was started with (the brief's `serve
    // --profile`, so that `launch` after `shutdown` reopens it).
    // Relative on purpose: a path in an answer is the client's to open, so
    // it must be absolute whatever the server was asked.
    auto const said{ talking.CallTool(
      "launch", nlohmann::json{
        { "bundle",
          std::filesystem::relative(scratch.Path() / "bundles").string() }
      }.dump()) };
    EXPECT_TRUE(said.has_value()) << (said ? "" : said.error());
    return said ? *said : std::string{ };
  }
}

TEST(McpSession, EveryToolAnswersOverTheHttpPath)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-session") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  auto const opened{ talking.Initialize() };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
  EXPECT_FALSE(talking.Session().empty());

  auto const listed{ talking.Call(tash::mcp::METHOD_TOOLS_LIST, "{}") };
  ASSERT_TRUE(listed.has_value()) << (listed ? "" : listed.error());
  EXPECT_GE(nlohmann::json::parse(*listed)["tools"].size(), 20u);

  std::string const launched{ Launched(talking, scratch) };
  EXPECT_NE(launched.find("bundle /"), std::string::npos) << launched;

  EXPECT_TRUE(talking.CallTool("step", R"({"frames":10})").has_value());
  EXPECT_TRUE(talking.CallTool("pace", R"({"rate":0.0})").has_value());

  auto const observed{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(observed.has_value()) << (observed ? "" : observed.error());
  EXPECT_NE(observed->find("frame"), std::string::npos);
  // The size a `region` crop has to sit inside is said before one is asked
  // for, so a crop is never guessed at and retried.
  EXPECT_NE(observed->find(" size "), std::string::npos) << *observed;

  auto const looked{ talking.CallTool("look", "{}") };
  ASSERT_TRUE(looked.has_value()) << (looked ? "" : looked.error());
  EXPECT_NE(looked->find(tash::mcp::PNG_MEDIA_TYPE), std::string::npos);

  EXPECT_TRUE(talking.CallTool(
    "look", R"({"region":"0,0,32,32"})").has_value());
  auto const shot{ talking.CallTool("shot", "{}") };
  ASSERT_TRUE(shot.has_value()) << (shot ? "" : shot.error());
  EXPECT_TRUE(std::filesystem::path{ *shot }.is_absolute()) << *shot;

  EXPECT_TRUE(talking.CallTool(
    "act", R"({"hold":["start"],"frames":2})").has_value());
  EXPECT_TRUE(talking.CallTool("act", R"({"release":["start"]})")
                .has_value());

  EXPECT_TRUE(talking.CallTool(
    "anchor", R"({"predicate":"none"})").has_value());
  EXPECT_TRUE(talking.CallTool(
    "run_until", R"({"predicate":"none","timeout_frames":30})")
                .has_value());

  EXPECT_TRUE(talking.CallTool("mark", R"({"name":"here"})").has_value());
  EXPECT_TRUE(talking.CallTool("step", R"({"frames":5})").has_value());
  EXPECT_TRUE(talking.CallTool(
    "clip", R"({"label":"window","from_mark":"here"})").has_value());

  EXPECT_TRUE(talking.CallTool(
    "expect", R"({"name":"still-there","predicate":"none"})").has_value());
  EXPECT_TRUE(talking.CallTool(
    "judge", R"({"name":"looks-right","passed":true})").has_value());

  auto const kept{ talking.CallTool("checkpoint", R"({"name":"early"})") };
  ASSERT_TRUE(kept.has_value()) << (kept ? "" : kept.error());
  EXPECT_NE(kept->find("cached in /"), std::string::npos) << *kept;
  EXPECT_TRUE(talking.CallTool("step", R"({"frames":5})").has_value());
  EXPECT_TRUE(talking.CallTool("restore", R"({"name":"early"})").has_value());
  EXPECT_TRUE(talking.CallTool(
    "restore_or_play",
    nlohmann::json{
      { "name", "early" },
      { "tape", (Root() / "examples/homebrew/tapes/demo.yaml").string() }
    }.dump()).has_value());

  auto const reported{ talking.CallTool("report", "{}") };
  ASSERT_TRUE(reported.has_value()) << (reported ? "" : reported.error());
  EXPECT_TRUE(std::filesystem::exists(*reported)) << *reported;
  EXPECT_TRUE(std::filesystem::path{ *reported }.is_absolute()) << *reported;
  EXPECT_TRUE(std::filesystem::path{ *reported }.filename()
              == tash::report::REPORT_NAME);

  std::filesystem::path const written{
    std::filesystem::path{ *reported }.parent_path()
    / tash::recorder::MANIFEST_NAME };
  ASSERT_TRUE(std::filesystem::exists(written)) << written.string();
  std::ifstream reading{ written };
  std::string const manifest{ std::istreambuf_iterator<char>{ reading },
                              std::istreambuf_iterator<char>{ } };
  EXPECT_NE(manifest.find("outcome: running"), std::string::npos) << manifest;
  EXPECT_NE(manifest.find("core_name: Genesis"), std::string::npos)
    << manifest;

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
  EXPECT_TRUE(talking.End().has_value());
}

TEST(McpSession, ARestoredCheckpointAnswersItsOwnFrame)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-checkpoint") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  // Nothing has stepped yet, so the refusal is the one the exit check met.
  auto const early{ talking.CallTool("observe", "{}") };
  ASSERT_FALSE(early.has_value());
  EXPECT_TRUE(early.error().starts_with("observe: ")) << early.error();

  ASSERT_TRUE(talking.CallTool("step", R"({"frames":30})").has_value());
  auto const seen{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
  std::string const hashes{ HashesIn(*seen) };
  ASSERT_FALSE(hashes.empty()) << *seen;

  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"here"})")
                .has_value());
  ASSERT_TRUE(talking.CallTool("step", R"({"frames":10})").has_value());
  ASSERT_TRUE(talking.CallTool("restore", R"({"name":"here"})").has_value());
  auto const back{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(back.has_value()) << (back ? "" : back.error());
  EXPECT_EQ(HashesIn(*back), hashes) << *back;

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  // A fresh run knows the checkpoint only from disk, frame included.
  ASSERT_TRUE(talking.CallTool("restore", R"({"name":"here"})").has_value());
  auto const again{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
  EXPECT_EQ(HashesIn(*again), hashes) << *again;
  EXPECT_NE(again->find("frame 30 "), std::string::npos) << *again;

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
  Forgot(scratch.Path() / "_checkpoints", tash::python::NOTE_SUFFIX);
  ASSERT_FALSE(Launched(talking, scratch).empty());

  // A state written before the note beside it restores as it always did.
  ASSERT_TRUE(talking.CallTool("restore", R"({"name":"here"})").has_value());
  auto const bare{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(bare.has_value()) << (bare ? "" : bare.error());
  EXPECT_EQ(HashesIn(*bare), hashes) << *bare;
  EXPECT_NE(bare->find("frame 0 "), std::string::npos) << *bare;

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, ARestoreServesThisSessionsCopyAndNamesTheNewerFile)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-newer") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  std::filesystem::path const keep{ scratch.Path() / "_checkpoints" };
  Served served{ keep };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  ASSERT_TRUE(talking.CallTool("step", R"({"frames":30})").has_value());
  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"plan"})")
                .has_value());
  auto const seen{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
  std::string const early{ HashesIn(*seen) };
  ASSERT_FALSE(early.empty()) << *seen;

  ASSERT_TRUE(talking.CallTool("step", R"({"frames":300})").has_value());
  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"later"})")
                .has_value());
  auto const moved{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(moved.has_value()) << (moved ? "" : moved.error());
  std::string const late{ HashesIn(*moved) };
  ASSERT_NE(late, early) << *moved;

  Overwrote(keep, "later", "plan");
  auto const back{ talking.CallTool("restore", R"({"name":"plan"})") };
  ASSERT_TRUE(back.has_value()) << (back ? "" : back.error());
  EXPECT_NE(back->find("restored plan from this run"), std::string::npos)
    << *back;
  EXPECT_NE(back->find("is newer and was not read"), std::string::npos)
    << *back;
  EXPECT_EQ(HashesIn(*back), early) << *back;

  // The next run over the same directory is the one the file is for.
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());
  auto const taken{ talking.CallTool("restore", R"({"name":"plan"})") };
  ASSERT_TRUE(taken.has_value()) << (taken ? "" : taken.error());
  EXPECT_EQ(HashesIn(*taken), late) << *taken;
  EXPECT_EQ(taken->find("is newer"), std::string::npos) << *taken;

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, LaunchingTheRunAlreadyOpenAnswersIt)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-relaunch") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  std::string const opening{ Launched(talking, scratch) };
  ASSERT_FALSE(opening.empty());

  auto const again{ talking.CallTool("launch", "{}") };
  ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
  EXPECT_EQ(*again, opening);

  auto const named{ talking.CallTool(
    "launch", nlohmann::json{
      { "profile", (Root() / "examples/homebrew/profile.yaml").string() }
    }.dump()) };
  ASSERT_TRUE(named.has_value()) << (named ? "" : named.error());
  EXPECT_EQ(*named, opening);

  auto const other{ talking.CallTool(
    "launch", nlohmann::json{
      { "profile", (Root() / "examples/columns/profile.yaml").string() }
    }.dump()) };
  ASSERT_FALSE(other.has_value());
  EXPECT_NE(other.error().find("shutdown first"), std::string::npos)
    << other.error();

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, AShutdownWithNoSessionLeftEndsTheServer)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-exit") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());

  // The session that shut the run down may launch another one.
  EXPECT_FALSE(served.Ended(STILL_THERE));
  ASSERT_TRUE(talking.End().has_value());
  EXPECT_TRUE(served.Ended(ENDED_WAIT)) << "the port is still held";
}

TEST(McpSession, AServerThatNeverOpenedARunOutlivesItsSession)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-no-run") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_TRUE(talking.End().has_value());
  EXPECT_FALSE(served.Ended(STILL_THERE)) << "there was no run to close";
}

TEST(McpSession, ADetachedJobRunsUnderNoBudgetUnlessOneIsNamed)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-detached-budget") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  auto const started{ talking.CallTool(
    "python", nlohmann::json{ { "source",
                                std::format("tash.run.step({})",
                                            BUDGET_STEP) },
                              { "detach", true } }.dump()) };
  ASSERT_TRUE(started.has_value()) << (started ? "" : started.error());
  EXPECT_NE(started->find("under no frame budget"), std::string::npos)
    << *started;

  // Waited for, the same source keeps the default the budget exists for.
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());
  auto const waited{ talking.CallTool(
    "python", nlohmann::json{ { "source", "1 + 1" } }.dump()) };
  ASSERT_TRUE(waited.has_value()) << (waited ? "" : waited.error());
  EXPECT_NE(waited->find(std::format("of its {}-frame budget",
                                     tash::mcp::DEFAULT_FRAME_BUDGET)),
            std::string::npos) << *waited;

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, PythonKeepsWhatAnEarlierCallDefined)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-python") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  std::string const launched{ Launched(talking, scratch) };
  EXPECT_NE(launched.find("bundle /"), std::string::npos) << launched;

  auto const defined{ talking.CallTool(
    "python",
    nlohmann::json{ { "source", "def twice(n):\n  return n * 2\n" } }
      .dump()) };
  ASSERT_TRUE(defined.has_value()) << (defined ? "" : defined.error());

  auto const used{ talking.CallTool(
    "python", nlohmann::json{ { "source", "twice(21)" } }.dump()) };
  ASSERT_TRUE(used.has_value()) << (used ? "" : used.error());
  EXPECT_NE(used->find("42"), std::string::npos);

  EXPECT_TRUE(talking.CallTool("step", R"({"frames":2})").has_value());
  auto const driven{ talking.CallTool(
    "python",
    nlohmann::json{ { "source", "tash.run.observe()['frame']" } }.dump()) };
  EXPECT_TRUE(driven.has_value()) << (driven ? "" : driven.error());

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, AToolThatMovesTheRunAnswersTheObservation)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-observed") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints", WatchedProfile(scratch) };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  // Nothing has stepped yet: the tool says what it did and stops there.
  auto const early{ talking.CallTool(
    "python", nlohmann::json{ { "source", "21 + 21" } }.dump()) };
  ASSERT_TRUE(early.has_value()) << (early ? "" : early.error());
  EXPECT_NE(early->find("42"), std::string::npos) << *early;
  EXPECT_EQ(early->find("\nframe "), std::string::npos) << *early;

  auto const played{ talking.CallTool(
    "restore_or_play",
    nlohmann::json{
      { "name", "watched" },
      { "tape", (Root() / "examples/homebrew/tapes/demo.yaml").string() }
    }.dump()) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ExpectObserved(*played);

  auto const stepped{ talking.CallTool("step", R"({"frames":10})") };
  ASSERT_TRUE(stepped.has_value()) << (stepped ? "" : stepped.error());
  EXPECT_NE(stepped->find("stepped 10 frames"), std::string::npos)
    << *stepped;
  ExpectObserved(*stepped);

  auto const acted{ talking.CallTool(
    "act", R"({"tap":["start"],"frames":2})") };
  ASSERT_TRUE(acted.has_value()) << (acted ? "" : acted.error());
  ExpectObserved(*acted);

  auto const driven{ talking.CallTool(
    "python", nlohmann::json{ { "source", "tash.run.step(3)" } }.dump()) };
  ASSERT_TRUE(driven.has_value()) << (driven ? "" : driven.error());
  ExpectObserved(*driven);

  EXPECT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, TheLifecycleIsEnforcedOverHttp)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-lifecycle") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  Served served{ made->Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };

  auto const early{ talking.Post(
    R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})") };
  ASSERT_TRUE(early.has_value()) << (early ? "" : early.error());
  auto const refused = nlohmann::json::parse(tash::mcp::BodyOf(*early));
  EXPECT_EQ(refused["error"]["code"], tash::mcp::INVALID_REQUEST);

  ASSERT_TRUE(talking.Initialize().has_value());

  auto const unknown{ talking.Post(
    R"({"jsonrpc":"2.0","id":2,"method":"tools/dance"})") };
  ASSERT_TRUE(unknown.has_value()) << (unknown ? "" : unknown.error());
  EXPECT_EQ(nlohmann::json::parse(tash::mcp::BodyOf(*unknown))["error"]
              ["code"], tash::mcp::METHOD_NOT_FOUND);

  auto const notified{ talking.Post(
    R"({"jsonrpc":"2.0","method":"notifications/initialized"})") };
  ASSERT_TRUE(notified.has_value()) << (notified ? "" : notified.error());
  EXPECT_EQ(notified->status, 202);
  EXPECT_TRUE(notified->body.empty());

  auto const broken{ talking.Post("{ not json") };
  ASSERT_TRUE(broken.has_value()) << (broken ? "" : broken.error());
  EXPECT_EQ(broken->status, 400);
  EXPECT_EQ(nlohmann::json::parse(tash::mcp::BodyOf(*broken))["error"]
              ["code"], tash::mcp::PARSE_ERROR);

  EXPECT_TRUE(talking.End().has_value());

  // An ended session is uninitialised: the next caller starts over.
  auto const after{ talking.Post(
    R"({"jsonrpc":"2.0","id":3,"method":"tools/list"})") };
  ASSERT_TRUE(after.has_value()) << (after ? "" : after.error());
  EXPECT_EQ(nlohmann::json::parse(tash::mcp::BodyOf(*after))["error"]
              ["code"], tash::mcp::INVALID_REQUEST);
}

TEST(McpSession, ASecondSessionSeededFromDiskWritesOneTape)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-lineage") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "bundles" / tash::python::CHECKPOINT_ROOT };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_FALSE(Launched(talking, scratch).empty());

  // A press and its release, so the note has a line worth carrying.
  ASSERT_TRUE(talking.CallTool(
    "act", R"({"tap":["start"],"frames":20})").has_value());
  ASSERT_TRUE(talking.CallTool("step", R"({"frames":40})").has_value());
  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"seeded"})")
                .has_value());
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());

  std::string const second{ Launched(talking, scratch) };
  ASSERT_FALSE(second.empty());
  ASSERT_TRUE(talking.CallTool("restore", R"({"name":"seeded"})")
                .has_value());
  ASSERT_TRUE(talking.CallTool(
    "act", R"({"tap":["a"],"frames":30})").has_value());
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());

  std::filesystem::path const bundle{ BundleIn(second) };
  ASSERT_FALSE(bundle.empty()) << second;
  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames, 90u);

  tash::cli::ReplayCommand replay;
  replay.bundle = bundle.string();
  testing::internal::CaptureStdout();
  auto const answered{ replay() };
  std::string const said{ testing::internal::GetCapturedStdout() };
  ASSERT_EQ(answered.Code(), 0) << answered.Message() << said;
  EXPECT_NE(said.find("replayed 90 frames"), std::string::npos) << said;
  EXPECT_NE(said.find("watches match"), std::string::npos) << said;
}

TEST(McpSession, APythonLoopPastItsBudgetSaysTheFramesItRan)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-budget") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_TRUE(Launched(talking, scratch).find("core ") != std::string::npos);

  auto const over{ talking.CallTool(
    "python", nlohmann::json{ { "source", Looping(BUDGET_STEP) },
                              { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_FALSE(over.has_value()) << *over;
  EXPECT_NE(over.error().find("BudgetExceeded"), std::string::npos)
    << over.error();
  EXPECT_NE(over.error().find(std::format("ran {} frames", BUDGET_FRAMES)),
            std::string::npos) << over.error();

  // The run is the caller's again, and the budget went with the job.
  auto const after{ talking.CallTool(
    "python", nlohmann::json{ { "source", "tash.run.frames()" } }.dump()) };
  ASSERT_TRUE(after.has_value()) << (after ? "" : after.error());
  EXPECT_NE(after->find(std::to_string(BUDGET_FRAMES)), std::string::npos)
    << *after;

  auto const caught{ talking.CallTool(
    "python",
    nlohmann::json{
      { "source", std::format("try:\n    while True:\n"
                              "        tash.run.step({})\n"
                              "except tash.BudgetExceeded:\n"
                              "    print('caught at', tash.run.frames())\n",
                              BUDGET_STEP) },
      { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_TRUE(caught.has_value()) << (caught ? "" : caught.error());
  EXPECT_NE(caught->find(std::format("caught at {}", 2 * BUDGET_FRAMES)),
            std::string::npos) << *caught;

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, AJobUnderItsBudgetRunsUntouched)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-budget-under") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_TRUE(Launched(talking, scratch).find("core ") != std::string::npos);

  auto const under{ talking.CallTool(
    "python",
    nlohmann::json{
      { "source",
        std::format("tash.run.step({})", BUDGET_FRAMES - BUDGET_STEP) },
      { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_TRUE(under.has_value()) << (under ? "" : under.error());
  EXPECT_NE(under->find(std::format("frame {}", BUDGET_FRAMES - BUDGET_STEP)),
            std::string::npos) << *under;

  // Each call has the budget to itself, counting from where it starts.
  auto const again{ talking.CallTool(
    "python",
    nlohmann::json{
      { "source",
        std::format("tash.run.step({})", BUDGET_FRAMES - BUDGET_STEP) },
      { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
  EXPECT_NE(again->find(std::format("frame {}",
                                    2 * (BUDGET_FRAMES - BUDGET_STEP))),
            std::string::npos) << *again;

  // Nothing asked, so the server's own budget stands and is far away.
  auto const asked{ talking.CallTool(
    "python",
    nlohmann::json{ { "source", std::format("tash.run.step({})",
                                            BUDGET_FRAMES) } }.dump()) };
  ASSERT_TRUE(asked.has_value()) << (asked ? "" : asked.error());

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, ASynchronousJobIsStillThereWhenTheClientStoppedWaiting)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-sync-job") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  ASSERT_TRUE(Launched(talking, scratch).find("core ") != std::string::npos);

  auto const said{ talking.CallTool(
    "python",
    nlohmann::json{
      { "source", std::format("import sys\n"
                              "print('on stdout')\n"
                              "print('on stderr', file=sys.stderr)\n"
                              "tash.run.step({})\n", BUDGET_STEP) },
      { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_TRUE(said.has_value()) << (said ? "" : said.error());
  EXPECT_NE(said->find("on stdout"), std::string::npos) << *said;
  EXPECT_NE(said->find("stderr:\non stderr"), std::string::npos) << *said;
  EXPECT_NE(said->find(std::format("python-1 ran {} frames of its {}-frame"
                                   " budget", BUDGET_STEP, BUDGET_FRAMES)),
            std::string::npos) << *said;

  // The answer a client that stopped waiting would have lost is a job's,
  // and the two tools that read a job read it.
  auto const status{ talking.CallTool("python_status", "{}") };
  ASSERT_TRUE(status.has_value()) << (status ? "" : status.error());
  EXPECT_NE(status->find(std::format("python-1 finished after {} frames of"
                                     " its {}-frame budget", BUDGET_STEP,
                                     BUDGET_FRAMES)),
            std::string::npos) << *status;
  EXPECT_NE(status->find("on stdout"), std::string::npos) << *status;
  EXPECT_NE(status->find("stderr:\non stderr"), std::string::npos) << *status;

  auto const output{ talking.CallTool("python_output", "{}") };
  ASSERT_TRUE(output.has_value()) << (output ? "" : output.error());
  EXPECT_NE(output->find("on stdout"), std::string::npos) << *output;
  EXPECT_NE(output->find("stderr:\non stderr"), std::string::npos) << *output;

  // Nothing was named, so the answer says the budget that was in force.
  auto const bare{ talking.CallTool(
    "python", nlohmann::json{ { "source", "1 + 1" } }.dump()) };
  ASSERT_TRUE(bare.has_value()) << (bare ? "" : bare.error());
  EXPECT_NE(bare->find(std::format("of its {}-frame budget",
                                   tash::mcp::DEFAULT_FRAME_BUDGET)),
            std::string::npos) << *bare;

  auto const lifted{ talking.CallTool(
    "python", nlohmann::json{ { "source", "2 + 2" },
                              { "frame_budget", 0 } }.dump()) };
  ASSERT_TRUE(lifted.has_value()) << (lifted ? "" : lifted.error());
  EXPECT_NE(lifted->find("under no budget"), std::string::npos) << *lifted;

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, ADetachedJobKeepsTheSameBudgetAndACancelStopsIt)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-budget-detached") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  auto const opened{ talking.CallTool(
    "launch", nlohmann::json{
      { "bundle",
        std::filesystem::relative(scratch.Path() / "bundles").string() },
      { "rate", JOB_RATE }
    }.dump()) };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());

  auto const started{ talking.CallTool(
    "python", nlohmann::json{ { "source", Looping(BUDGET_STEP) },
                              { "detach", true },
                              { "frame_budget", BUDGET_FRAMES } }.dump()) };
  ASSERT_TRUE(started.has_value()) << (started ? "" : started.error());
  EXPECT_NE(started->find(std::format("under a {}-frame budget",
                                      BUDGET_FRAMES)),
            std::string::npos) << *started;
  EXPECT_NE(started->find("python_cancel"), std::string::npos) << *started;

  std::string status;
  for (int poll{ 0 }; poll != POLL_LIMIT; ++poll)
  {
    auto const said{ talking.CallTool("python_status", "{}") };
    ASSERT_TRUE(said.has_value()) << (said ? "" : said.error());
    status = *said;
    if (status.find("python-1 failed") != std::string::npos)
      break;
    std::this_thread::sleep_for(POLL_BEAT);
  }
  EXPECT_NE(status.find(std::format("python-1 failed after {} frames",
                                    BUDGET_FRAMES)),
            std::string::npos) << status;

  auto const said{ talking.CallTool("python_output", "{}") };
  ASSERT_FALSE(said.has_value()) << *said;
  EXPECT_NE(said.error().find("BudgetExceeded"), std::string::npos)
    << said.error();
  // The refusal names the budget and what was left of it, so the death
  // does not read as the source's own fault.
  EXPECT_NE(said.error().find(std::format("of {} frames run, 0 left",
                                          BUDGET_FRAMES)),
            std::string::npos) << said.error();

  // Nothing is running, so there is nothing to cancel.
  auto const idle{ talking.CallTool("python_cancel", "{}") };
  ASSERT_FALSE(idle.has_value()) << *idle;
  EXPECT_NE(idle.error().find("no python job is running"), std::string::npos)
    << idle.error();

  auto const looping{ talking.CallTool(
    "python", nlohmann::json{ { "source", Looping(BUDGET_STEP) },
                              { "detach", true },
                              { "frame_budget", CANCEL_LIMIT } }.dump()) };
  ASSERT_TRUE(looping.has_value()) << (looping ? "" : looping.error());

  auto const stopping{ talking.CallTool("python_cancel", "{}") };
  ASSERT_TRUE(stopping.has_value()) << (stopping ? "" : stopping.error());
  EXPECT_NE(stopping->find("python-2 stops at its next"), std::string::npos)
    << *stopping;

  for (int poll{ 0 }; poll != POLL_LIMIT; ++poll)
  {
    auto const answered{ talking.CallTool("python_status", "{}") };
    ASSERT_TRUE(answered.has_value())
      << (answered ? "" : answered.error());
    status = *answered;
    if (status.find("python-2 failed") != std::string::npos)
      break;
    std::this_thread::sleep_for(POLL_BEAT);
  }
  EXPECT_NE(status.find("python-2 failed after"), std::string::npos)
    << status;

  auto const cancelled{ talking.CallTool("python_output", "{}") };
  ASSERT_FALSE(cancelled.has_value()) << *cancelled;
  EXPECT_NE(cancelled.error().find("cancelled"), std::string::npos)
    << cancelled.error();

  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());
}

TEST(McpSession, ADetachedPythonJobRunsWhileTheLoopAnswers)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-detached") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "_checkpoints" };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());

  // Paced, so the job is certainly still running when the calls below are
  // refused: unpaced, 600 frames of this core are gone in a moment.
  auto const opened{ talking.CallTool(
    "launch", nlohmann::json{
      { "bundle",
        std::filesystem::relative(scratch.Path() / "bundles").string() },
      { "rate", JOB_RATE }
    }.dump()) };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());

  // A launch renders nothing: the frame the two reading tools answer with
  // below is the first this makes.
  EXPECT_TRUE(talking.CallTool("step", R"({"frames":10})").has_value());

  // Nothing detached yet: a status has nothing to answer about.
  auto const early{ talking.CallTool("python_status", "{}") };
  ASSERT_FALSE(early.has_value());
  EXPECT_NE(early.error().find("no python job"), std::string::npos)
    << early.error();

  auto const started{ talking.CallTool(
    "python_file",
    nlohmann::json{
      { "path", (Root() / "examples/homebrew/stepping.py").string() },
      { "detach", true }
    }.dump()) };
  ASSERT_TRUE(started.has_value()) << (started ? "" : started.error());
  EXPECT_NE(started->find("python-1 is running"), std::string::npos)
    << *started;

  // Every tool that would touch the run refuses while the job has it, and
  // names the job rather than interleaving with it.
  std::vector<std::pair<std::string, std::string>> const refused{
    { "act", R"({"tap":["start"]})" },
    { "step", R"({"frames":1})" },
    { "peek", R"({"address":"0x0000","count":4})" },
    { "checkpoint", R"({"name":"meanwhile"})" },
    { "python", R"({"source":"1"})" },
    { "python", R"({"source":"1","detach":true})" }
  };
  for (auto const& [tool, arguments] : refused)
  {
    auto const said{ talking.CallTool(tool, arguments) };
    ASSERT_FALSE(said.has_value()) << tool;
    EXPECT_NE(said.error().find("python-1 is running; python_status"),
              std::string::npos) << tool << ": " << said.error();
  }

  // The report is rendered from what is on disk, so it answers anyway.
  EXPECT_TRUE(talking.CallTool("report", "{}").has_value());

  // The step thread publishes each finished frame under its own lock, so
  // these two read it while the job goes on making them.
  auto const seen{ talking.CallTool("observe", "{}") };
  ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
  std::uint64_t const first{ FrameIn(*seen) };
  EXPECT_NE(first, 0u) << *seen;
  EXPECT_NE(HashesIn(*seen), "") << *seen;

  auto const looked{ talking.CallTool("look", "{}") };
  ASSERT_TRUE(looked.has_value()) << (looked ? "" : looked.error());
  EXPECT_NE(looked->find(tash::mcp::PNG_MEDIA_TYPE), std::string::npos)
    << *looked;

  std::uint64_t moved{ first };
  for (int poll{ 0 }; poll != POLL_LIMIT && moved <= first; ++poll)
  {
    std::this_thread::sleep_for(POLL_BEAT);
    auto const again{ talking.CallTool("observe", "{}") };
    ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
    moved = FrameIn(*again);
  }
  EXPECT_GT(moved, first) << "the published frame stood still";

  std::string status;
  bool        progressed{ false };
  for (int poll{ 0 }; poll != POLL_LIMIT; ++poll)
  {
    auto const said{ talking.CallTool("python_status", "{}") };
    ASSERT_TRUE(said.has_value()) << (said ? "" : said.error());
    status = *said;
    progressed = progressed
                 || status.find("stepped 100 frames") != std::string::npos;
    if (status.find("python-1 finished") != std::string::npos)
      break;
    EXPECT_NE(status.find("python-1 is running after "), std::string::npos)
      << status;
    std::this_thread::sleep_for(POLL_BEAT);
  }
  EXPECT_NE(status.find(std::format("python-1 finished after {} frames",
                                     JOB_FRAMES)),
            std::string::npos) << status;
  EXPECT_TRUE(progressed) << status;

  auto const output{ talking.CallTool("python_output", "{}") };
  ASSERT_TRUE(output.has_value()) << (output ? "" : output.error());
  EXPECT_NE(output->find("stepped 100 frames"), std::string::npos) << *output;
  EXPECT_NE(output->find("stepped 600 frames"), std::string::npos) << *output;
  EXPECT_NE(output->find("stepping is done"), std::string::npos) << *output;

  // Finished, so the run is the caller's again, observation and all.
  EXPECT_NE(output->find("\nframe "), std::string::npos) << *output;
  EXPECT_TRUE(talking.CallTool("step", R"({"frames":1})").has_value());

  auto const closed{ talking.CallTool("shutdown", "{}") };
  ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
  EXPECT_EQ(closed->find("waited for"), std::string::npos) << *closed;
}

TEST(McpSession, AResetPowersOnAgainAndTheTapeAfterItReplays)
{
  auto const made{ tash::utilities::ScratchAreaOf("mcp-reset") };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  ScratchArea const& scratch{ *made };
  Served served{ scratch.Path() / "bundles" / tash::python::CHECKPOINT_ROOT };
  ASSERT_NE(served.Port(), 0);

  McpClient talking{ served.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  std::string const launched{ Launched(talking, scratch) };
  ASSERT_FALSE(launched.empty());

  ASSERT_TRUE(talking.CallTool("step", R"({"frames":300})").has_value());
  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"before"})")
                .has_value());
  ASSERT_TRUE(talking.CallTool("checkpoint", R"({"name":"again"})")
                .has_value());

  auto const powered{ talking.CallTool("reset", "{}") };
  ASSERT_TRUE(powered.has_value()) << (powered ? "" : powered.error());
  EXPECT_NE(powered->find("reset to power on"), std::string::npos)
    << *powered;
  // The tool goes through the run's own checkpoint, so each of the two paid
  // for a restore probe, whose frames and restores are on the line like any
  // other.
  EXPECT_NE(powered->find(std::format(
              "frame {}", 300u + 4u * tash::session::PROBE_FRAMES
                          + tash::session::RESET_FRAMES)),
            std::string::npos) << *powered;

  // The checkpoints the run took are its own and a reset does not drop them.
  ASSERT_TRUE(talking.CallTool("restore", R"({"name":"before"})")
                .has_value());
  ASSERT_TRUE(talking.CallTool("reset", "{}").has_value());

  auto const played{ talking.CallTool(
    "python",
    nlohmann::json{ { "source", std::format(
      "tash.run.play(r'{}')",
      (Root() / "examples/homebrew/tapes/demo.yaml").string()) } }
      .dump()) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(talking.CallTool("shutdown", "{}").has_value());

  std::filesystem::path const bundle{ BundleIn(launched) };
  ASSERT_FALSE(bundle.empty()) << launched;
  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_LT(written->segments[0].frames, 300u);
  EXPECT_GE(written->segments[0].frames, 90u);

  tash::cli::ReplayCommand replay;
  replay.bundle = bundle.string();
  testing::internal::CaptureStdout();
  auto const answered{ replay() };
  std::string const said{ testing::internal::GetCapturedStdout() };
  ASSERT_EQ(answered.Code(), 0) << answered.Message() << said;
  EXPECT_NE(said.find("watches match"), std::string::npos) << said;
}
