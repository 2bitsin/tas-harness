#include "tash/tash/scenario-runner.hpp"

#include "tash/python/checkpoint-store.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/report/render.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/session/restore-probe.hpp"
#include "tash/session/session.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/profile.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <oxbox/serialization/io.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
  using tash::cli::ScenarioRunner;
  using tash::recorder::Bundle;
  using tash::session::Session;
  using tash::session::SessionSettings;

  // A probe restores twice, so the second checkpoint's first restore is
  // the third of a run that checkpoints twice.
  constexpr std::uint64_t SPOILED_RESTORE{ 3 };

  constexpr std::uint64_t SETTLE_FRAMES{ 30 };

  // Three candidates at a lookahead of one: a checkpoint at the top level
  // and one under each candidate.
  constexpr std::uint64_t SEARCH_CHECKPOINTS{ 4 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Opened() -> std::unique_ptr<Session>
  {
    auto const beside{ tash::utilities::ExecutableDirectory() };
    EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
    auto const profile{ tash::cli::ProfileFrom(
      Root() / "examples/homebrew/profile.yaml") };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    SessionSettings settings;
    settings.core = *beside / "genesis_plus_gx_libretro.so";
    settings.rom = Root() / profile->rom;
    auto session{ Session::Open(std::move(settings)) };
    EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
    return session ? std::move(*session) : nullptr;
  }

  // Every frame the core makes, which a restore does not take back: the
  // frames a probe costs are counted here and nowhere else.
  class CountsEveryFrame : public tash::session::FrameObserver
  {
  public:
    auto OnFrame(tash::bus::FrameView const&, std::int64_t) -> void override
    { ++_frames; }

    auto OnRewind(tash::session::Rewind const&) -> void override { }

    [[nodiscard]] auto Frames() const noexcept -> std::uint64_t
    { return _frames; }

  private:
    std::uint64_t _frames{ 0 };
  };

  auto Wrote(std::filesystem::path const& file, std::string const& text)
    -> void
  {
    std::ofstream out{ file };
    out << text;
  }

  // The homebrew profile with an absolute rom, so a run opened from it
  // finds the rom wherever ctest runs the test from.
  auto ProfileBeside(std::filesystem::path const& where)
    -> std::filesystem::path
  {
    auto profile{ tash::cli::ProfileFrom(Root()
                                         / "examples/homebrew/profile.yaml") };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return { };
    profile->rom = (Root() / profile->rom).string();
    std::filesystem::path const path{ where / "profile.yaml" };
    oxbox::serialization::SerializeTo(*profile, path);
    return path;
  }

  // A core whose save state loses a field, as Genesis Plus GX's did before
  // libretro-cores 2026.9.12.12: the restore counted here lands on a state
  // the run left long ago, so the probe's second line cannot follow the
  // first.
  class LosesAFieldOnRestore : public tash::session::FrameObserver
  {
  public:
    LosesAFieldOnRestore(Session& live, std::uint64_t spoil)
    : _live{ live }, _spoil{ spoil }
    {
      auto state{ live.SaveState() };
      EXPECT_TRUE(state.has_value()) << (state ? "" : state.error());
      if (state)
        _stale = std::move(*state);
    }

    auto OnFrame(tash::bus::FrameView const&, std::int64_t) -> void override
    { }

    auto OnRewind(tash::session::Rewind const&) -> void override
    {
      if (++_rewinds != _spoil || _stale.empty())
        return;
      EXPECT_TRUE(_live.LoadState(_stale).has_value());
    }

  private:
    Session&               _live;
    std::uint64_t          _spoil;
    std::uint64_t          _rewinds{ 0 };
    std::vector<std::byte> _stale{ };
  };
}

TEST(ScenarioRunner, TheHomebrewScenarioRunsToCompletion)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-runs") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("scenario.tash") };

  auto const bundle{ Bundle::Create(scratch->Path(), "scenario") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  parts.bundle = &*bundle;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven,
                                 Root() / "examples/homebrew/scenario.py") };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(writer->Flush().has_value());

  std::uint64_t verdicts{ 0 };
  std::uint64_t passed{ 0 };
  auto reader{ tash::trace::Reader::Open(trace) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&verdicts, &passed](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::VerdictRecord>)
    {
      ++verdicts;
      passed += record.passed ? 1u : 0u;
    }
  });
  EXPECT_EQ(verdicts, 7u);
  EXPECT_EQ(passed, verdicts);
  EXPECT_TRUE(std::filesystem::exists(bundle->Shots() / "shot-0001.png"));
}

TEST(ScenarioRunner, AScenarioCanAskForTheReportBeforeTheRunEnds)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-report") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const bundle{ Bundle::Create(scratch->Path(), "scenario") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  auto writer{ tash::trace::Writer::Open(bundle->Trace(), "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const file{ scratch->File("reporting.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(2)\n"
              "tash.run.mark('halfway')\n"
              "print(tash.run.report())\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  parts.bundle = &*bundle;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

  auto const page{ bundle->Root() / tash::report::REPORT_NAME };
  ASSERT_TRUE(std::filesystem::exists(page));
  std::ifstream held{ page };
  std::string const html{ std::istreambuf_iterator<char>{ held },
                          std::istreambuf_iterator<char>{ } };
  EXPECT_NE(html.find("halfway"), std::string::npos);
  EXPECT_NE(html.find("no run.yaml yet"), std::string::npos);
}

TEST(ScenarioRunner, EveryCheckpointProbesThatARestoreIsExact)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-probe") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("probe.tash") };
  auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const file{ scratch->File("probing.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(60)\n"
              "tash.run.checkpoint('here')\n"
              "tash.run.checkpoint('again')\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  session->Record(&*writer);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(writer->Flush().has_value());

  std::vector<tash::trace::VerdictRecord> verdicts;
  std::uint64_t restores{ 0 };
  auto reader{ tash::trace::Reader::Open(trace) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&verdicts, &restores](auto const& record) {
    using Held = std::decay_t<decltype(record)>;
    if constexpr (std::is_same_v<Held, tash::trace::VerdictRecord>)
      verdicts.push_back(record);
    else if constexpr (std::is_same_v<Held, tash::trace::RestoreRecord>)
      ++restores;
  });

  // Both checkpoints are probed, and the one verdict is the first's.
  ASSERT_EQ(verdicts.size(), 1u);
  EXPECT_EQ(verdicts[0].name,
            "a restored state continues as the straight line");
  EXPECT_TRUE(verdicts[0].passed) << verdicts[0].text;
  EXPECT_EQ(verdicts[0].text,
            std::format("{} frames compared, hash for hash",
                        tash::session::PROBE_FRAMES));
  EXPECT_EQ(restores, 4u);
  EXPECT_EQ(driven.Probes(), (tash::session::ProbeCounts{ 2u, 0u }));
}

TEST(ScenarioRunner, ACheckpointThatPartsIsNamedAndCounted)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-parting") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile_path{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile_path.empty());
  auto const profile{ tash::cli::ProfileFrom(profile_path) };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());

  auto const file{ scratch->File("parting.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(30)\n"
              "tash.run.checkpoint('one')\n"
              "tash.run.step(30)\n"
              "tash.run.checkpoint('two')\n");

  tash::cli::RunRequest wanted;
  wanted.profile = *profile;
  wanted.profile_path = profile_path;
  wanted.bundle = scratch->Path();
  wanted.producer = "tash-test";

  auto opened{ tash::cli::OpenedRun::Open(std::move(wanted)) };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
  LosesAFieldOnRestore spoiling{ (*opened)->Live(), SPOILED_RESTORE };
  (*opened)->Live().Observe(spoiling);

  std::filesystem::path const bundle{ (*opened)->BundleOf()->Root() };
  ScenarioRunner runner;
  auto const played{ runner.Play((*opened)->ScenarioOf(), file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  EXPECT_EQ((*opened)->ProbesOf(),
            (tash::session::ProbeCounts{ 2u, 1u }));
  auto const closed{ (*opened)->Close() };
  ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
  opened->reset();

  std::vector<tash::trace::VerdictRecord> verdicts;
  auto reader{ tash::trace::Reader::Open(bundle
                                         / tash::recorder::TRACE_NAME) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&verdicts](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::VerdictRecord>)
      verdicts.push_back(record);
  });

  ASSERT_EQ(verdicts.size(), 2u);
  EXPECT_TRUE(verdicts[0].passed) << verdicts[0].text;
  EXPECT_EQ(verdicts[0].text,
            std::format("{} frames compared, hash for hash",
                        tash::session::PROBE_FRAMES));
  EXPECT_EQ(verdicts[1].name, verdicts[0].name);
  EXPECT_FALSE(verdicts[1].passed);
  EXPECT_NE(verdicts[1].text.find("checkpoint \"two\": of"),
            std::string::npos) << verdicts[1].text;
  EXPECT_NE(verdicts[1].text.find("the lines part at frame"),
            std::string::npos) << verdicts[1].text;

  auto const manifest{ Bundle::At(bundle)->Read() };
  ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());
  EXPECT_EQ(manifest->probed, 2u);
  EXPECT_EQ(manifest->parted, 1u);

  std::ifstream held{ bundle / tash::report::REPORT_NAME };
  std::string const page{ std::istreambuf_iterator<char>{ held },
                          std::istreambuf_iterator<char>{ } };
  EXPECT_NE(page.find("probed 2 checkpoints, 1 parted"), std::string::npos);
}

TEST(ScenarioRunner, ASearchTakesItsCheckpointsWithoutTheProbe)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-search") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const file{ scratch->File("searching.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(30)\n"
              "answer = tash.search(('a', 'b', 'c'),\n"
              "                     lambda run, candidate: None,\n"
              "                     lambda run: 1, depth=2)\n"
              "assert answer.trials == 12, answer.trials\n"
              "assert answer.frames == 0, answer.frames\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  CountsEveryFrame counting;
  session->Observe(counting);

  tash::python::RunParts parts;
  parts.session = session.get();
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

  EXPECT_EQ(driven.Probes(),
            (tash::session::ProbeCounts{ 0u, 0u, SEARCH_CHECKPOINTS }));
  EXPECT_EQ(counting.Frames(), SETTLE_FRAMES);
}

TEST(ScenarioRunner, ANamedCheckpointStillPaysTheProbe)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-named") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const file{ scratch->File("naming.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(30)\n"
              "tash.run.checkpoint('here')\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  CountsEveryFrame counting;
  session->Observe(counting);

  tash::python::RunParts parts;
  parts.session = session.get();
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

  EXPECT_EQ(driven.Probes(), (tash::session::ProbeCounts{ 1u, 0u, 0u }));
  EXPECT_EQ(counting.Frames(),
            SETTLE_FRAMES + 2u * tash::session::PROBE_FRAMES);
}

TEST(ScenarioRunner, AScratchCheckpointFromPythonPaysNoProbe)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-scratch") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const file{ scratch->File("holding.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(30)\n"
              "tash.run.checkpoint('held', scratch=True)\n"
              "tash.run.step(30)\n"
              "tash.run.restore('held')\n"
              "tash.run.forget('held')\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  CountsEveryFrame counting;
  session->Observe(counting);

  tash::python::RunParts parts;
  parts.session = session.get();
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

  EXPECT_EQ(driven.Probes(), (tash::session::ProbeCounts{ 0u, 0u, 1u }));
  EXPECT_EQ(counting.Frames(), 2u * SETTLE_FRAMES);
}

TEST(ScenarioRunner, ACheckpointOneRunTookAnotherRestoresFromTheStore)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-store") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile_path{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile_path.empty());
  auto const profile{ tash::cli::ProfileFrom(profile_path) };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
  std::filesystem::path const seen{ scratch->File("seen.txt") };

  auto const taking{ scratch->File("taking.py") };
  Wrote(taking, std::format(
    "import pathlib\n"
    "import tash\n"
    "tash.run.step(30)\n"
    "tash.run.checkpoint('crossing')\n"
    "pathlib.Path(r'{}').write_text(tash.run.observe()['exact'])\n",
    seen.string()));
  auto const back{ scratch->File("back.py") };
  Wrote(back, std::format(
    "import pathlib\n"
    "import tash\n"
    "tash.run.restore('crossing')\n"
    "shown = tash.run.observe()\n"
    "want = pathlib.Path(r'{}').read_text()\n"
    "assert shown['exact'] == want, f\"{{shown['exact']}} is not {{want}}\"\n"
    "assert shown['frame'] == 30, shown['frame']\n",
    seen.string()));

  ScenarioRunner runner;
  std::filesystem::path state;
  for (auto const& [name, file] : { std::pair{ "first", taking },
                                    std::pair{ "second", back } })
  {
    tash::cli::RunRequest wanted;
    wanted.profile = *profile;
    wanted.profile_path = profile_path;
    wanted.bundle = scratch->Path();
    wanted.checkpoints = scratch->Path() / tash::python::CHECKPOINT_ROOT;
    wanted.name = name;
    wanted.producer = "tash-test";

    auto opened{ tash::cli::OpenedRun::Open(std::move(wanted)) };
    ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
    auto const played{ runner.Play((*opened)->ScenarioOf(), file) };
    ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
    state = scratch->Path() / tash::python::CHECKPOINT_ROOT
            / (*opened)->RomHashOf() / "crossing.state";
    EXPECT_TRUE(std::filesystem::exists(state)) << state.string();
    EXPECT_TRUE((*opened)->Close().has_value());
  }
}

TEST(ScenarioRunner, AFailedExpectationComesBackAsATraceback)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-refusals") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const file{ scratch->File("failing.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(1)\n"
              "tash.run.expect('never', False, 'by construction')\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_FALSE(played.has_value());
  EXPECT_NE(played.error().find("Traceback"), std::string::npos)
    << played.error();
  EXPECT_NE(played.error().find("by construction"), std::string::npos)
    << played.error();
}

TEST(ScenarioRunner, AScenarioCutsAClipFromAMarkOrFromAFrame)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-clip") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("clip.tash") };
  auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const file{ scratch->File("clipping.py") };
  Wrote(file, "import tash\n"
              "tash.run.mark('here')\n"
              "tash.run.step(5)\n"
              "tash.run.clip('window', from_mark='here')\n"
              "tash.run.clip('from-frame', from_frame=2)\n"
              "for asked in ({'from_mark': 'nowhere'}, {}):\n"
              "    try:\n"
              "        tash.run.clip('refused', **asked)\n"
              "        raise AssertionError(f'{asked} was not refused')\n"
              "    except RuntimeError as refusal:\n"
              "        print(refusal)\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(writer->Flush().has_value());

  std::vector<tash::trace::TriggerRecord> windows;
  auto reader{ tash::trace::Reader::Open(trace) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&windows](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::TriggerRecord>)
      windows.push_back(record);
  });

  ASSERT_EQ(windows.size(), 2u);
  EXPECT_EQ(windows[0].name, "window");
  EXPECT_EQ(windows[0].text, "0 5");
  EXPECT_EQ(windows[1].name, "from-frame");
  EXPECT_EQ(windows[1].text, "2 5");
}

TEST(ScenarioRunner, ObserveSpellsItsHashesAsAPredicateReadsThem)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-observe") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("observe.tash") };
  auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const file{ scratch->File("observing.py") };
  Wrote(file, "import tash\n"
              "tash.run.step(2)\n"
              "seen = tash.run.observe()\n"
              "for name in ('exact', 'difference', 'perceptual'):\n"
              "    assert isinstance(seen[name], str), seen[name]\n"
              "    assert len(seen[name]) == 16, seen[name]\n"
              "tash.run.expect('the exact hash reads back as a predicate',\n"
              "                f\"exact_hash {seen['exact']}\")\n"
              "tash.run.expect('and so does the perceptual one',\n"
              "                f\"perceptual_hash {seen['perceptual']}\"\n"
              "                ' within 0')\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(writer->Flush().has_value());

  std::vector<tash::trace::VerdictRecord> verdicts;
  auto reader{ tash::trace::Reader::Open(trace) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&verdicts](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::VerdictRecord>)
      verdicts.push_back(record);
  });

  ASSERT_EQ(verdicts.size(), 2u);
  for (auto const& verdict : verdicts)
    EXPECT_TRUE(verdict.passed) << verdict.name << ": " << verdict.text;
}

TEST(ScenarioRunner, AnchorAnswersThePredicateWithoutRecordingAVerdict)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("scenario-anchor") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("anchor.tash") };
  auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
  ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

  auto const file{ scratch->File("anchoring.py") };
  Wrote(file, "import tash\n"
              "run = tash.run\n"
              "run.step(2)\n"
              "here = f\"exact_hash {run.observe()['exact']}\"\n"
              "run.expect('the frame answers its own hash', run.anchor(here))\n"
              "run.expect('and not another', not run.anchor(\n"
              "    'exact_hash 0000000000000000'))\n"
              "run.expect('not turns the answer over',\n"
              "           not run.anchor(f'not {here}'))\n"
              "try:\n"
              "    run.anchor('watch nowhere equal 1')\n"
              "    refused = ''\n"
              "except RuntimeError as raised:\n"
              "    refused = str(raised)\n"
              "run.expect('a predicate it cannot judge is a refusal',\n"
              "           bool(refused), refused)\n");

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  tash::python::RunParts parts;
  parts.session = session.get();
  parts.trace = &*writer;
  tash::python::ScenarioRun driven{ parts };

  ScenarioRunner runner;
  auto const played{ runner.Play(driven, file) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());
  ASSERT_TRUE(writer->Flush().has_value());

  std::vector<tash::trace::VerdictRecord> verdicts;
  auto reader{ tash::trace::Reader::Open(trace) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&verdicts](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::VerdictRecord>)
      verdicts.push_back(record);
  });

  // Four expectations and nothing the four anchor calls wrote themselves.
  ASSERT_EQ(verdicts.size(), 4u);
  for (auto const& verdict : verdicts)
    EXPECT_TRUE(verdict.passed) << verdict.name << ": " << verdict.text;
  EXPECT_NE(verdicts[3].text.find("nowhere"), std::string::npos)
    << verdicts[3].text;
}
