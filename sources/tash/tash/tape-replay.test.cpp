#include "tash/tash/replay-command.hpp"

#include "tash/recorder/bundle.hpp"
#include "tash/recorder/encoder-settings.hpp"
#include "tash/report/render.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/channel.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tape/transitions.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/profile.hpp"
#include "tash/tash/scenario-runner.hpp"
#include "tash/tash/tape-recording.hpp"
#include "tash/trace/line.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <oxbox/serialization/io.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
  using tash::cli::OpenedRun;
  using tash::cli::ReplayCommand;
  using tash::cli::RunRequest;
  using tash::cli::TapeRecording;
  using tash::session::Session;

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  // The homebrew profile with an absolute rom, so the replay resolves it
  // from wherever ctest runs the test from.
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

  auto Wrote(std::filesystem::path const& file, std::string const& text)
    -> void
  {
    std::ofstream out{ file };
    out << text;
  }

  // One bundle, recorded the way `tash run --scenario --bundle` records it.
  auto Recorded(std::filesystem::path const& root,
                std::filesystem::path const& profile_path,
                std::filesystem::path const& scenario,
                std::filesystem::path const& checkpoints = { })
    -> std::filesystem::path
  {
    auto const profile{ tash::cli::ProfileFrom(profile_path) };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return { };

    RunRequest wanted;
    wanted.profile = *profile;
    wanted.profile_path = profile_path;
    wanted.bundle = root;
    wanted.checkpoints = checkpoints;
    wanted.producer = "tash-test";

    auto opened{ OpenedRun::Open(std::move(wanted)) };
    EXPECT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
    if (!opened)
      return { };

    tash::cli::ScenarioRunner runner;
    auto const played{ runner.Play((*opened)->ScenarioOf(), scenario) };
    EXPECT_TRUE(played.has_value()) << (played ? "" : played.error());

    std::filesystem::path const where{ (*opened)->BundleOf()->Root() };
    auto const closed{ (*opened)->Close() };
    EXPECT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
    if (closed)
      EXPECT_EQ(closed->problem, "");
    return where;
  }

  auto Replayed(std::filesystem::path const& bundle) -> oxbox::cli::CliResult
  {
    ReplayCommand replay;
    replay.bundle = bundle.string();
    return replay();
  }

  auto Replayed(std::filesystem::path const& bundle,
                std::filesystem::path const& into) -> oxbox::cli::CliResult
  {
    ReplayCommand replay;
    replay.bundle = bundle.string();
    replay.record = into.string();
    return replay();
  }

  struct Summary
  {
    std::uint64_t encoded{ 0 };
    std::uint64_t dropped{ 0 };
    std::uint64_t recorded{ 0 };
    std::uint32_t width{ 0 };
    std::uint32_t height{ 0 };
  };

  // The bundle line a recorded replay prints, read back as its numbers.
  auto SummaryIn(std::string const& said) -> Summary
  {
    std::regex const counted{
      R"RX(\((\d+) encoded, (\d+) dropped, (\d+) recorded, )RX"
      R"RX((\d+)x(\d+) film\))RX" };
    std::smatch found;
    Summary read{ };
    if (!std::regex_search(said, found, counted))
    {
      ADD_FAILURE() << said;
      return read;
    }
    read.encoded = std::stoull(found[1].str());
    read.dropped = std::stoull(found[2].str());
    read.recorded = std::stoull(found[3].str());
    read.width = static_cast<std::uint32_t>(std::stoul(found[4].str()));
    read.height = static_cast<std::uint32_t>(std::stoul(found[5].str()));
    return read;
  }

  // The one bundle `--record` made under an otherwise empty directory.
  auto TheOneIn(std::filesystem::path const& root) -> std::filesystem::path
  {
    std::vector<std::filesystem::path> made;
    for (auto const& entry : std::filesystem::directory_iterator{ root })
      if (entry.is_directory())
        made.push_back(entry.path());
    EXPECT_EQ(made.size(), 1u);
    return made.size() == 1 ? made.front() : std::filesystem::path{ };
  }

  struct Trace
  {
    std::uint64_t frames{ 0 };
    std::uint64_t hash{ 0 };
    std::size_t   restores{ 0 };
    std::size_t   marks{ 0 };
  };

  // What the trace of a run that never rewound says it came out as.
  auto TraceOf(std::filesystem::path const& file) -> Trace
  {
    Trace read{ };
    auto const line{ tash::trace::Line::Of(file) };
    EXPECT_TRUE(line.has_value()) << (line ? "" : line.error());
    if (!line)
      return read;
    read.frames = line->Frames();

    auto reader{ tash::trace::Reader::Open(file) };
    EXPECT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    if (!reader)
      return read;
    reader->ForEach([&read, &line](auto const& record) {
      using Held = std::decay_t<decltype(record)>;
      if constexpr (std::is_same_v<Held, tash::trace::RestoreRecord>)
        ++read.restores;
      else if constexpr (std::is_same_v<Held, tash::trace::MarkRecord>)
        ++read.marks;
      else if constexpr (std::is_same_v<Held, tash::trace::FrameRecord>)
      {
        if (line->Holds(record.frame))
          read.hash = record.hash_exact;
      }
    });
    return read;
  }

  auto Live(std::filesystem::path const& profile_path)
    -> std::unique_ptr<Session>
  {
    auto const profile{ tash::cli::ProfileFrom(profile_path) };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return nullptr;
    auto settings{ tash::cli::SettingsFrom(*profile) };
    EXPECT_TRUE(settings.has_value()) << (settings ? "" : settings.error());
    if (!settings)
      return nullptr;
    auto opened{ Session::Open(std::move(*settings)) };
    EXPECT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
    return opened ? std::move(*opened) : nullptr;
  }

  auto StartMask() -> std::uint32_t
  {
    auto const channel{ tash::tape::ChannelFrom("p1.start") };
    EXPECT_TRUE(channel.has_value()) << (channel ? "" : channel.error());
    return channel ? channel->Mask() : 0u;
  }

  // Thirty frames with start held for the middle ten, which is two changes.
  auto Played(Session& run) -> void
  {
    run.Step(10);
    run.HoldPad(0, StartMask());
    run.Step(10);
    run.HoldPad(0, 0);
    run.Step(10);
  }
}

TEST(TapeReplay, TheHomebrewScenariosBundleReplaysToItsOwnHash)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-replay") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile,
             Root() / "examples/homebrew/scenario.py") };
  ASSERT_FALSE(bundle.empty());

  auto const manifest{ tash::recorder::Bundle::At(bundle)->Read() };
  ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());
  EXPECT_EQ(manifest->tape, tash::recorder::TAPE_NAME);

  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 2u);
  EXPECT_EQ(written->segments[0].name, "tape");
  EXPECT_EQ(written->segments[1].name, "judged");
  EXPECT_EQ(written->segments[1].Waits().kind,
            tash::tape::AnchorKind::EXACT_HASH);

  // The two presses the demo tape made, recorded at the frames they landed.
  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  EXPECT_EQ(moves->size(), 4u);

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();
}

TEST(TapeReplay, AWrongInputInTheTapeFailsAtTheSegmentThatWandered)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-wandered") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  // zsenilia answers no single button; up and down at once is the one input
  // that moves its frames, and 200 of them is where the move shows.
  std::filesystem::path const scenario{ scratch->Path() / "marked.py" };
  Wrote(scenario, "import tash\n"
                  "tash.run.mark('intro')\n"
                  "tash.run.step(201)\n"
                  "tash.run.mark('later')\n"
                  "tash.run.step(30)\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile, scenario) };
  ASSERT_FALSE(bundle.empty());
  EXPECT_EQ(Replayed(bundle).Code(), 0);

  auto written{ tash::tape::TapeFrom(bundle / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 2u);
  written->segments[0].transitions
    = "0 down p1.up\n0 down p1.down\n";
  ASSERT_TRUE(tash::tape::WriteTape(*written,
                                    bundle / tash::recorder::TAPE_NAME)
                .has_value());

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 1);
  EXPECT_NE(replayed.Message().find("later"), std::string_view::npos)
    << replayed.Message();
}

TEST(TapeReplay, TheSearchScenariosBundleReplaysTheLineItKept)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-search") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile,
             Root() / "examples/homebrew/search.py") };
  ASSERT_FALSE(bundle.empty());

  // Every trial was folded off the line again, so the tape is the settle
  // before the search and the mark after it.
  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 2u);
  EXPECT_EQ(written->segments[0].name, "start");
  EXPECT_EQ(written->segments[0].frames, 30u);
  EXPECT_EQ(written->segments[1].name, "searched");
  EXPECT_EQ(written->segments[1].Waits().kind,
            tash::tape::AnchorKind::EXACT_HASH);

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();
}

TEST(TapeReplay, ARewindDropsTheChangesMadeAfterTheCheckpoint)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-rewind") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  std::filesystem::path const scenario{ scratch->Path() / "rewound.py" };
  Wrote(scenario, "import tash\n"
                  "run = tash.run\n"
                  "run.step(10)\n"
                  "run.hold(1, 'start')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.checkpoint('here')\n"
                  "run.hold(1, 'a')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.restore('here')\n"
                  "run.step(10)\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile, scenario) };
  ASSERT_FALSE(bundle.empty());

  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames, 40u);

  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  ASSERT_EQ(moves->size(), 2u);
  EXPECT_EQ((*moves)[0].frame, 10u);
  EXPECT_EQ((*moves)[1].frame, 20u);

  EXPECT_EQ(Replayed(bundle).Code(), 0);
}

TEST(TapeReplay, ARunSeededFromDiskFoldsToItsCheckpointAndReplays)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-seeded-run") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const keep{ scratch->Path() / "keep" };

  // The seed is taken after a fold, so its line is shorter than the frames
  // the run had made: the run that restores it starts at the checkpoint's
  // frame number and thirty frames along its own line.
  std::filesystem::path const seeding{ scratch->Path() / "seeding.py" };
  Wrote(seeding, "import tash\n"
                 "run = tash.run\n"
                 "run.step(10)\n"
                 "run.hold(1, 'start')\n"
                 "run.step(10)\n"
                 "run.release(1)\n"
                 "run.step(10)\n"
                 "run.checkpoint('start')\n"
                 "run.step(40)\n"
                 "run.restore('start')\n"
                 "run.checkpoint('folded')\n"
                 "assert run.line() == 30, run.line()\n"
                 "assert run.frames() > run.line()\n");
  ASSERT_FALSE(Recorded(scratch->Path(), profile, seeding, keep).empty());

  std::filesystem::path const seeded{ scratch->Path() / "seeded.py" };
  Wrote(seeded, "import tash\n"
                "run = tash.run\n"
                "run.restore('folded')\n"
                "assert run.line() == 30, run.line()\n"
                "run.step(10)\n"
                "run.mark('seeded')\n"
                "run.hold(1, 'start')\n"
                "run.step(10)\n"
                "run.release(1)\n"
                "run.checkpoint('mid')\n"
                "assert run.line() == 50, run.line()\n"
                "run.step(25)\n"
                "run.restore('mid')\n"
                "assert run.line() == 50, run.line()\n"
                "run.step(5)\n"
                "run.mark('after')\n"
                "run.step(5)\n"
                "assert run.line() == 60, run.line()\n");

  std::filesystem::path bundle;
  {
    auto const read{ tash::cli::ProfileFrom(profile) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    RunRequest wanted;
    wanted.profile = *read;
    wanted.profile_path = profile;
    wanted.bundle = scratch->Path();
    wanted.checkpoints = keep;
    wanted.producer = "tash-test";
    auto opened{ OpenedRun::Open(std::move(wanted)) };
    ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());

    tash::cli::ScenarioRunner runner;
    auto const played{ runner.Play((*opened)->ScenarioOf(), seeded) };
    ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

    auto const taped{ (*opened)->WriteTape() };
    ASSERT_TRUE(taped.has_value()) << (taped ? "" : taped.error());
    EXPECT_EQ(taped->frames, 60u);

    bundle = (*opened)->BundleOf()->Root();
    auto const closed{ (*opened)->Close() };
    ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
    EXPECT_EQ(closed->problem, "");
  }

  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 3u);
  EXPECT_EQ(written->segments[0].frames, 40u);
  EXPECT_EQ(written->segments[1].name, "seeded");
  EXPECT_EQ(written->segments[1].frames, 15u);

  // The mark after the fold anchors on the frame the run really stood at,
  // which is only so when the fold cut the seeded stretch.
  EXPECT_EQ(written->segments[2].name, "after");
  EXPECT_EQ(written->segments[2].frames, 5u);
  EXPECT_EQ(written->segments[2].Waits().kind,
            tash::tape::AnchorKind::EXACT_HASH);

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();
}

TEST(TapeReplay, ASearchRightAfterASeedKeepsTheWatchesOnTheTape)
{
  constexpr std::uint64_t SEED_END{ 39 };

  auto const scratch{ tash::utilities::ScratchAreaOf("tape-seed-fold") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const keep{ scratch->Path() / "keep" };

  std::filesystem::path const seeding{ scratch->Path() / "seeding.py" };
  Wrote(seeding, "import tash\n"
                 "run = tash.run\n"
                 "run.step(10)\n"
                 "run.hold(1, 'start')\n"
                 "run.step(10)\n"
                 "run.release(1)\n"
                 "run.step(20)\n"
                 "run.checkpoint('seed')\n"
                 "assert run.line() == 40, run.line()\n");
  ASSERT_FALSE(Recorded(scratch->Path(), profile, seeding, keep).empty());

  // The search folds the run back to the seed's own frame zero, so every
  // record the tape is checked against was written by the restore.
  std::filesystem::path const searched{ scratch->Path() / "searched.py" };
  Wrote(searched, "import tash\n"
                  "run = tash.run\n"
                  "def press(driven, candidate):\n"
                  "    driven.hold(1, candidate)\n"
                  "    driven.step(5)\n"
                  "    driven.release(1)\n"
                  "    driven.step(5)\n"
                  "def moved(driven):\n"
                  "    return driven.watch('beat')\n"
                  "run.restore('seed')\n"
                  "assert run.line() == 40, run.line()\n"
                  "assert run.watch('beat') >= 0\n"
                  "tash.search(('a', 'b'), press, moved)\n"
                  "assert run.line() == 40, run.line()\n");

  std::filesystem::path bundle;
  {
    auto const read{ tash::cli::ProfileFrom(profile) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    RunRequest wanted;
    wanted.profile = *read;
    wanted.profile_path = profile;
    wanted.bundle = scratch->Path();
    wanted.checkpoints = keep;
    wanted.producer = "tash-test";
    auto opened{ OpenedRun::Open(std::move(wanted)) };
    ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());

    tash::cli::ScenarioRunner runner;
    auto const played{ runner.Play((*opened)->ScenarioOf(), searched) };
    ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

    auto const taped{ (*opened)->WriteTape() };
    ASSERT_TRUE(taped.has_value()) << (taped ? "" : taped.error());
    EXPECT_EQ(taped->frames, 40u);

    bundle = (*opened)->BundleOf()->Root();
    auto const closed{ (*opened)->Close() };
    ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
    EXPECT_EQ(closed->problem, "");
  }

  // The line ends where the seed did, and the restore is what wrote both
  // the hash and the watch there.
  std::size_t watched{ 0 };
  std::size_t hashed{ 0 };
  {
    auto reader{ tash::trace::Reader::Open(bundle
                                           / tash::recorder::TRACE_NAME) };
    ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    reader->ForEach([&watched, &hashed](auto const& record) {
      using Held = std::decay_t<decltype(record)>;
      if constexpr (std::is_same_v<Held, tash::trace::WatchRecord>)
        watched += record.frame == SEED_END ? 1u : 0u;
      else if constexpr (std::is_same_v<Held, tash::trace::FrameRecord>)
        hashed += record.frame == SEED_END ? 1u : 0u;
    });
  }
  EXPECT_EQ(watched, 1u);
  EXPECT_EQ(hashed, 1u);

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();
}

TEST(TapeReplay, ALiveRunWritesTheTapeTheRestoreFoldedItTo)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-live") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  std::filesystem::path const scenario{ scratch->Path() / "live.py" };
  Wrote(scenario, "import tash\n"
                  "run = tash.run\n"
                  "run.step(10)\n"
                  "run.hold(1, 'start')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.checkpoint('here')\n"
                  "run.step(20)\n"
                  "run.restore('here')\n"
                  "assert run.tape()['frames'] == 30\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  auto const read{ tash::cli::ProfileFrom(profile) };
  ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());

  RunRequest wanted;
  wanted.profile = *read;
  wanted.profile_path = profile;
  wanted.bundle = scratch->Path();
  wanted.producer = "tash-test";
  auto opened{ OpenedRun::Open(std::move(wanted)) };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());

  tash::cli::ScenarioRunner runner;
  auto const played{ runner.Play((*opened)->ScenarioOf(), scenario) };
  ASSERT_TRUE(played.has_value()) << (played ? "" : played.error());

  // The run is still open: the tape on disk is the line the restore folded
  // back to, twenty frames short of where the run stands.
  auto const taped{ (*opened)->WriteTape() };
  ASSERT_TRUE(taped.has_value()) << (taped ? "" : taped.error());
  EXPECT_EQ(taped->frames, 30u);
  EXPECT_TRUE(taped->file.is_absolute()) << taped->file.string();
  EXPECT_EQ(taped->file.filename(), tash::recorder::TAPE_NAME);

  // A replay reads the profile out of run.yaml, so the live tape is only
  // any use beside a manifest that names it.
  auto const manifest{ tash::recorder::Bundle::At(taped->file.parent_path())
                         ->Read() };
  ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());
  EXPECT_EQ(manifest->tape, tash::recorder::TAPE_NAME);
  EXPECT_EQ(manifest->outcome, "running");

  auto const written{ tash::tape::TapeFrom(taped->file) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames, 30u);

  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  ASSERT_EQ(moves->size(), 2u);
  EXPECT_EQ((*moves)[0].frame, 10u);
  EXPECT_EQ((*moves)[1].frame, 20u);

  auto const closed{ (*opened)->Close() };
  EXPECT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
}

TEST(TapeReplay, ACheckpointRightAfterARestoreKeepsTheRunOnOneLine)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-after-restore") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  // Twice: restore, then checkpoint with no frame produced in between, which
  // is where the run's frame counter and its line part company.
  std::filesystem::path const scenario{ scratch->Path() / "after.py" };
  Wrote(scenario, "import tash\n"
                  "run = tash.run\n"
                  "run.step(5)\n"
                  "run.hold(1, 'start')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(5)\n"
                  "run.checkpoint('floor')\n"
                  "run.hold(1, 'a')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.restore('floor')\n"
                  "run.checkpoint('burst')\n"
                  "run.hold(1, 'a')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.restore('burst')\n"
                  "run.checkpoint('walk')\n"
                  "run.step(10)\n"
                  "run.restore('walk')\n"
                  "run.step(10)\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile, scenario) };
  ASSERT_FALSE(bundle.empty());

  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames, 30u);

  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  ASSERT_EQ(moves->size(), 2u);
  EXPECT_EQ((*moves)[0].frame, 5u);
  EXPECT_TRUE((*moves)[0].down);
  EXPECT_EQ((*moves)[1].frame, 15u);

  // The trace folds in the same unit, so the manifest, the report and the
  // tape all count the frames the run kept the same way.
  Trace const traced{ TraceOf(bundle / tash::recorder::TRACE_NAME) };
  EXPECT_EQ(traced.frames, 30u);

  auto const manifest{ tash::recorder::Bundle::At(bundle)->Read() };
  ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());
  EXPECT_EQ(manifest->kept, 30u);
  EXPECT_GT(manifest->frames, 30u);

  EXPECT_EQ(Replayed(bundle).Code(), 0);
}

TEST(TapeReplay, AResetDropsTheLineAndTheTapeStartsFromPowerOn)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-reset") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  std::filesystem::path const scenario{ scratch->Path() / "reset.py" };
  Wrote(scenario, "import tash\n"
                  "run = tash.run\n"
                  "run.step(10)\n"
                  "run.hold(1, 'start')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.reset()\n"
                  "run.hold(1, 'a')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile, scenario) };
  ASSERT_FALSE(bundle.empty());

  auto const written{ tash::tape::TapeFrom(bundle
                                           / tash::recorder::TAPE_NAME) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames,
            tash::session::RESET_FRAMES + 20u);

  // Only what was played after the reset, and from the line's own frame 0,
  // which is the frame the reset itself ran.
  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  ASSERT_EQ(moves->size(), 2u);
  EXPECT_EQ((*moves)[0].frame, tash::session::RESET_FRAMES);
  EXPECT_TRUE((*moves)[0].down);
  EXPECT_EQ((*moves)[1].frame, tash::session::RESET_FRAMES + 10u);

  auto const replayed{ Replayed(bundle) };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();
}

TEST(TapeReplay, ARecordedReplayIsTheFoldedLineWithNoRewindInIt)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-recorded") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

  // Three trials from one checkpoint, then the line the run kept: the
  // shape of a search, and the tape holds only what survived it.
  std::filesystem::path const scenario{ scratch->Path() / "searched.py" };
  Wrote(scenario, "import tash\n"
                  "run = tash.run\n"
                  "run.step(10)\n"
                  "run.mark('start')\n"
                  "run.checkpoint('here')\n"
                  "for trial in range(3):\n"
                  "    run.hold(1, 'a')\n"
                  "    run.step(10)\n"
                  "    run.release(1)\n"
                  "    run.step(10)\n"
                  "    run.restore('here')\n"
                  "run.hold(1, 'start')\n"
                  "run.step(10)\n"
                  "run.release(1)\n"
                  "run.step(10)\n"
                  "run.mark('chosen')\n");

  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());
  std::filesystem::path const bundle{
    Recorded(scratch->Path(), profile, scenario) };
  ASSERT_FALSE(bundle.empty());

  Trace const searched{ TraceOf(bundle / tash::recorder::TRACE_NAME) };
  // The three trials, and the two the restore probe made for its verdict.
  EXPECT_EQ(searched.restores, 5u);
  EXPECT_EQ(searched.frames, 30u);

  std::filesystem::path const into{ scratch->Path() / "replayed" };
  std::filesystem::create_directories(into);
  testing::internal::CaptureStdout();
  auto const replayed{ Replayed(bundle, into) };
  std::string const said{ testing::internal::GetCapturedStdout() };
  EXPECT_EQ(replayed.Code(), 0) << replayed.Message();

  // A replay waits on its encoder, so the film holds every frame the tape
  // played, and it is upscaled to a size worth watching.
  Summary const counts{ SummaryIn(said) };
  EXPECT_EQ(counts.dropped, 0u);
  EXPECT_EQ(counts.encoded, searched.frames);
  EXPECT_EQ(counts.recorded, searched.frames);
  EXPECT_GE(counts.width, tash::recorder::FILM_WIDTH_AT_LEAST);
  EXPECT_GT(counts.height, 0u);

  std::filesystem::path const clean{ TheOneIn(into) };
  ASSERT_FALSE(clean.empty());
  EXPECT_TRUE(std::filesystem::is_regular_file(clean
                                               / tash::recorder::VIDEO_NAME));
  EXPECT_TRUE(std::filesystem::is_regular_file(clean
                                               / tash::report::REPORT_NAME));

  Trace const film{ TraceOf(clean / tash::recorder::TRACE_NAME) };
  EXPECT_EQ(film.restores, 0u);
  EXPECT_EQ(film.frames, searched.frames);
  EXPECT_EQ(film.hash, searched.hash);
  EXPECT_GT(film.marks, 0u);

  auto const manifest{ tash::recorder::Bundle::At(clean)->Read() };
  ASSERT_TRUE(manifest.has_value()) << (manifest ? "" : manifest.error());
  EXPECT_EQ(manifest->replay_of, bundle.string());
  EXPECT_EQ(manifest->outcome, "completed");
  EXPECT_EQ(manifest->frames, searched.frames);
}

TEST(TapeRecording, AResetFoldsTheLineBackToNothing)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-folded") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::unique_ptr<Session> const run{ Live(profile) };
  ASSERT_NE(run, nullptr);
  TapeRecording folded{ *run };
  run->Observe(folded);
  Played(*run);
  auto const before{ folded.LineUpTo(run->Frames()) };
  ASSERT_TRUE(before.has_value());
  EXPECT_EQ(before->frames, 30u);

  ASSERT_TRUE(run->Reset().has_value());
  run->Step(10);

  auto const line{ folded.LineUpTo(run->Frames()) };
  ASSERT_TRUE(line.has_value());
  EXPECT_EQ(line->frames, tash::session::RESET_FRAMES + 10u);
  EXPECT_TRUE(line->marks.empty());
}

TEST(TapeRecording, AResetMakesAnAdriftRunWritableAgain)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-repowered") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::unique_ptr<Session> const run{ Live(profile) };
  ASSERT_NE(run, nullptr);
  TapeRecording again{ *run };
  run->Observe(again);
  Played(*run);

  again.OnRewind(tash::session::Rewind{ 0, std::nullopt, "elsewhere",
                                        nullptr });
  ASSERT_FALSE(again.LineUpTo(run->Frames()).has_value());

  ASSERT_TRUE(run->Reset().has_value());
  run->Step(10);
  auto const line{ again.LineUpTo(run->Frames()) };
  ASSERT_TRUE(line.has_value());
  EXPECT_EQ(line->frames, tash::session::RESET_FRAMES + 10u);

  auto const bundle{ tash::recorder::Bundle::Create(scratch->Path(),
                                                   "repowered") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  EXPECT_TRUE(again.Write(*bundle, tash::tape::TapeHeader{ }).has_value());
}

TEST(TapeRecording, ACheckpointsLineSeedsTheRunThatRestoresIt)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-seeded") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  // One core at a time in a process, so the first run is gone by the second.
  std::optional<tash::session::Line> line{ };
  {
    std::unique_ptr<Session> const first{ Live(profile) };
    ASSERT_NE(first, nullptr);
    TapeRecording kept{ *first };
    first->Observe(kept);
    Played(*first);
    line = kept.LineUpTo(first->Frames());
  }
  ASSERT_TRUE(line.has_value());
  EXPECT_EQ(line->frames, 30u);
  EXPECT_TRUE(line->marks.empty());

  std::unique_ptr<Session> const second{ Live(profile) };
  ASSERT_NE(second, nullptr);
  TapeRecording again{ *second };
  second->Observe(again);
  again.OnRewind(tash::session::Rewind{ 0, std::nullopt, "seeded", &*line });
  second->Step(10);

  auto const bundle{ tash::recorder::Bundle::Create(scratch->Path(),
                                                   "seeded") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  ASSERT_TRUE(again.Write(*bundle, tash::tape::TapeHeader{ }).has_value());

  auto const written{ tash::tape::TapeFrom(bundle->Tape()) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  ASSERT_EQ(written->segments.size(), 1u);
  EXPECT_EQ(written->segments[0].frames, 40u);

  // The line the second run never played, then the frames it did.
  auto const moves{ tash::tape::TransitionsFrom(written->segments[0].Lines()) };
  ASSERT_TRUE(moves.has_value()) << (moves ? "" : moves.error());
  ASSERT_EQ(moves->size(), 2u);
  EXPECT_EQ((*moves)[0].frame, 10u);
  EXPECT_TRUE((*moves)[0].down);
  EXPECT_EQ((*moves)[1].frame, 20u);
  EXPECT_FALSE((*moves)[1].down);
}

TEST(TapeRecording, AFoldAfterASeedCutsTheLineWhereTheCheckpointWas)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-seed-fold") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::optional<tash::session::Line> line{ };
  {
    std::unique_ptr<Session> const first{ Live(profile) };
    ASSERT_NE(first, nullptr);
    TapeRecording kept{ *first };
    first->Observe(kept);
    Played(*first);
    line = kept.LineUpTo(first->Frames());
  }
  ASSERT_TRUE(line.has_value());
  ASSERT_EQ(line->frames, 30u);

  std::unique_ptr<Session> const second{ Live(profile) };
  ASSERT_NE(second, nullptr);
  TapeRecording again{ *second };
  second->Observe(again);
  again.OnRewind(tash::session::Rewind{ 0, std::nullopt, "seeded", &*line });

  second->Step(10);
  tash::session::LinePlace const mid{ again.Place() };
  EXPECT_EQ(mid.frames, 40u);
  EXPECT_EQ(mid.token, second->Frames());

  second->Step(20);
  again.OnRewind(tash::session::Rewind{ second->Frames(), mid, "mid",
                                        nullptr });
  EXPECT_EQ(again.Place(), mid);

  // The seed's frames sit on the line before this run's first, so the fold
  // above has to cut the stretch, not run off its end and keep it whole.
  second->Step(5);
  EXPECT_EQ(again.Place().frames, 45u);
  EXPECT_EQ(again.Place().token, second->Frames());

  auto const held{ again.LineUpTo(second->Frames()) };
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->frames, 45u);
}

TEST(TapeRecording, APlaceOnTheSeedsOwnFramesFoldsBackToItEveryTime)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-seed-place") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::optional<tash::session::Line> line{ };
  {
    std::unique_ptr<Session> const first{ Live(profile) };
    ASSERT_NE(first, nullptr);
    TapeRecording kept{ *first };
    first->Observe(kept);
    Played(*first);
    line = kept.LineUpTo(first->Frames());
  }
  ASSERT_TRUE(line.has_value());
  ASSERT_EQ(line->frames, 30u);

  std::unique_ptr<Session> const second{ Live(profile) };
  ASSERT_NE(second, nullptr);
  TapeRecording again{ *second };
  second->Observe(again);
  again.OnRewind(tash::session::Rewind{ 0, std::nullopt, "seeded", &*line });

  // What a search takes at its seed: this run has made no frame of its own,
  // so the place names one of the seed's and nothing else can.
  tash::session::LinePlace const seed{ again.Place() };
  EXPECT_EQ(seed.frames, 30u);
  EXPECT_NE(seed.token, 0u);

  for (int trial{ 0 }; trial < 3; ++trial)
  {
    second->Step(20);
    again.OnRewind(tash::session::Rewind{ second->Frames(), seed, "seed",
                                          nullptr });
    EXPECT_EQ(again.Place(), seed) << trial;
    auto const held{ again.LineUpTo(second->Frames()) };
    ASSERT_TRUE(held.has_value()) << trial;
    EXPECT_EQ(held->frames, 30u) << trial;
    EXPECT_EQ(held->transitions, line->transitions) << trial;
  }

  second->Step(10);
  auto const after{ again.LineUpTo(second->Frames()) };
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->frames, 40u);
}

TEST(TapeRecording, AFoldPastWhereTheLineWasLostPutsTheRunBackOnOne)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-refound") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::unique_ptr<Session> const run{ Live(profile) };
  ASSERT_NE(run, nullptr);
  TapeRecording again{ *run };
  run->Observe(again);
  run->Step(10);
  tash::session::LinePlace const early{ again.Place() };
  Played(*run);

  again.OnRewind(tash::session::Rewind{ run->Frames(), std::nullopt,
                                        "elsewhere", nullptr });
  ASSERT_FALSE(again.LineUpTo(run->Frames()).has_value());

  again.OnRewind(tash::session::Rewind{ run->Frames(), early, "early",
                                        nullptr });
  auto const line{ again.LineUpTo(run->Frames()) };
  ASSERT_TRUE(line.has_value()) << "a fold back past the stray restore";
  EXPECT_EQ(line->frames, 10u);
}

TEST(TapeRecording, ACheckpointWithNoLineLeavesTheRunAdrift)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("tape-adrift") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::filesystem::path const profile{ ProfileBeside(scratch->Path()) };
  ASSERT_FALSE(profile.empty());

  std::unique_ptr<Session> const run{ Live(profile) };
  ASSERT_NE(run, nullptr);
  TapeRecording adrift{ *run };
  run->Observe(adrift);
  Played(*run);

  adrift.OnRewind(tash::session::Rewind{ 0, std::nullopt, "elsewhere",
                                         nullptr });
  run->Step(10);
  EXPECT_FALSE(adrift.LineUpTo(run->Frames()).has_value());

  auto const bundle{ tash::recorder::Bundle::Create(scratch->Path(),
                                                   "adrift") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  auto const refused{ adrift.Write(*bundle, tash::tape::TapeHeader{ }) };
  ASSERT_FALSE(refused.has_value());
  EXPECT_NE(refused.error().find("came from outside this run"),
            std::string::npos) << refused.error();
}
