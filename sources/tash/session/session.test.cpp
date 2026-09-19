#include "tash/perception/frame-hash.hpp"
#include "tash/session/rom-file.hpp"
#include "tash/session/session.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <print>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
  using tash::perception::ExactHash;
  using tash::session::Session;
  using tash::session::SessionSettings;

  // roms/ is the machine's own cartridge library; examples/columns/profile.yaml
  // names the same file for `tash run`.
  constexpr auto COLUMNS = "roms/Genesis/Columns (USA, Europe).zip";

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml") &&
           here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Columns() -> std::filesystem::path
  {
    return Root() / COLUMNS;
  }

  auto CorePath() -> std::filesystem::path
  {
    auto const beside{ tash::utilities::ExecutableDirectory() };
    EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
    return *beside / "genesis_plus_gx_libretro.so";
  }

  auto Homebrew() -> std::filesystem::path
  {
    return Root() / "examples/homebrew/zsenilia.bin";
  }

  auto Opened(std::filesystem::path const& rom) -> std::unique_ptr<Session>
  {
    SessionSettings settings;
    settings.core = CorePath();
    settings.rom = rom;
    auto session{ Session::Open(std::move(settings)) };
    EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
    return session ? std::move(*session) : nullptr;
  }

  auto Digests(Session& session, std::uint64_t frames)
    -> std::vector<std::uint64_t>
  {
    std::vector<std::uint64_t> digests;
    digests.reserve(frames);
    for (std::uint64_t frame{ 0 }; frame < frames; ++frame)
    {
      session.Step(1);
      auto const latest{ session.Video().Latest() };
      if (!latest)
      {
        digests.push_back(0);
        continue;
      }
      auto const digest{ ExactHash(*latest) };
      EXPECT_TRUE(digest.has_value()) << (digest ? "" : digest.error());
      digests.push_back(digest.value_or(0));
    }
    return digests;
  }

  auto Report(std::string_view what, Session const& session) -> void
  {
    std::print("[   fps    ] {}: {} frames in {:.3f} s, {:.1f} fps, "
               "{:.2f}x real time\n",
               what, session.Frames(), session.WallSeconds(),
               session.Frames() / session.WallSeconds(),
               session.AchievedRate());
  }

  auto LatestOf(Session& session) -> std::uint64_t
  {
    auto const latest{ session.Video().Latest() };
    EXPECT_TRUE(latest.has_value());
    if (!latest)
      return 0;
    auto const digest{ ExactHash(*latest) };
    EXPECT_TRUE(digest.has_value()) << (digest ? "" : digest.error());
    return digest.value_or(0);
  }

  auto FirstFrameOf(Session& session) -> std::uint64_t
  {
    session.Step(1);
    return LatestOf(session);
  }

  // What the cartridge has written: a reset that is not a power cycle shows
  // up here long before it shows up in the picture.
  auto WorkRam(Session const& session) -> std::vector<std::byte>
  {
    for (tash::session::MemoryRegion const& region : session.Memory())
      if (region.name == "system")
        return std::vector<std::byte>{ region.bytes.begin(),
                                       region.bytes.end() };
    ADD_FAILURE() << "the core exposes no system ram";
    return { };
  }

  auto Differing(std::vector<std::byte> const& one,
                 std::vector<std::byte> const& other) -> std::size_t
  {
    if (one.size() != other.size())
      return std::max(one.size(), other.size());
    std::size_t count{ 0 };
    for (std::size_t at{ 0 }; at < one.size(); ++at)
      count += one[at] != other[at] ? 1u : 0u;
    return count;
  }

  // Every reset the observers were told of, and the frame each landed at.
  class Resets : public tash::session::FrameObserver
  {
  public:
    auto OnFrame(tash::bus::FrameView const&, std::int64_t) -> void override
    { }

    auto OnReset(tash::session::Reset const& done) -> void override
    { at.push_back(done.at); }

    std::vector<std::uint64_t> at{ };
  };

  constexpr std::uint64_t RUN_FRAMES{ 600 };
  constexpr std::uint64_t STATE_FRAME{ 300 };
  constexpr std::uint64_t OBSERVED_FRAMES{ 120 };
}

TEST(Session, TheHomebrewRomRunsSixHundredFrames)
{
  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  session->Step(RUN_FRAMES);
  Report("homebrew", *session);

  EXPECT_EQ(session->Frames(), RUN_FRAMES);
  EXPECT_NEAR(session->HarnessSeconds(), RUN_FRAMES / session->Fps(), 1e-9);
  auto const latest{ session->Video().Latest() };
  ASSERT_TRUE(latest.has_value());
  EXPECT_EQ(latest->descriptor.width, 320u);
  EXPECT_GT(latest->descriptor.height, 0u);
  EXPECT_GT(session->Video().Written(), RUN_FRAMES - 2);
  EXPECT_GT(session->Audio().Written(), 0u);
  EXPECT_EQ(session->DeterminismLevel(), tash::clock::Determinism::D2);
}

TEST(Session, AnObserverSeesEveryFrameTheCoreProduces)
{
  class Counting : public tash::session::FrameObserver
  {
  public:
    auto OnFrame(tash::bus::FrameView const& frame, std::int64_t harness_time)
      -> void override
    {
      EXPECT_EQ(frame.descriptor.number, seen);
      EXPECT_GE(harness_time, last_time);
      last_time = harness_time;
      ++seen;
    }

    std::uint64_t seen{ 0 };
    std::int64_t last_time{ 0 };
  };

  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  Counting counting;
  session->Observe(counting);
  session->Step(OBSERVED_FRAMES);

  EXPECT_EQ(counting.seen, session->Frames());
  EXPECT_EQ(counting.last_time,
            static_cast<std::int64_t>(
              std::llround((session->Frames() - 1) / session->Fps()
                           * tash::session::NANOSECONDS_PER_SECOND)));
}

TEST(Session, AnAudioObserverSeesEverySampleTheCoreProduces)
{
  class Collecting : public tash::session::AudioObserver
  {
  public:
    auto OnAudio(std::span<std::int16_t const> interleaved) -> void override
    {
      EXPECT_EQ(interleaved.size() % tash::bus::AUDIO_CHANNELS, 0u);
      ++blocks;
      samples += interleaved.size();
    }

    std::uint64_t blocks{ 0 };
    std::size_t samples{ 0 };
  };

  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  Collecting collecting;
  session->Observe(collecting);
  session->Step(OBSERVED_FRAMES);

  EXPECT_GT(collecting.blocks, 0u);
  EXPECT_EQ(collecting.samples, session->Audio().Written());
}

TEST(Session, TwoRunsOfTheHomebrewRomHashTheSame)
{
  std::vector<std::uint64_t> first;
  {
    auto const session{ Opened(Homebrew()) };
    ASSERT_NE(session, nullptr);
    first = Digests(*session, RUN_FRAMES);
    Report("homebrew, run one", *session);
  }
  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  std::vector<std::uint64_t> const second{ Digests(*session, RUN_FRAMES) };

  ASSERT_EQ(first.size(), second.size());
  EXPECT_EQ(first, second);
  // A demo animates, so a constant picture would be the bug this hides.
  EXPECT_GT(std::set<std::uint64_t>(first.begin(), first.end()).size(), 1u);
}

TEST(Session, AStateTakenAtThreeHundredReplaysTheSameSixHundredth)
{
  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);

  session->Step(STATE_FRAME);
  auto const state{ session->SaveState() };
  ASSERT_TRUE(state.has_value()) << (state ? "" : state.error());

  std::vector<std::uint64_t> const straight{
    Digests(*session, RUN_FRAMES - STATE_FRAME) };

  ASSERT_TRUE(session->LoadState(*state).has_value());
  std::vector<std::uint64_t> const replayed{
    Digests(*session, RUN_FRAMES - STATE_FRAME) };

  EXPECT_EQ(straight, replayed);
}

TEST(Session, ColumnsRunsFromItsZip)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  auto const session{ Opened(Columns()) };
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->RomPath().extension(), ".md");
  EXPECT_TRUE(std::filesystem::is_regular_file(session->RomPath()));

  session->Step(RUN_FRAMES);
  Report("columns", *session);
  EXPECT_EQ(session->Frames(), RUN_FRAMES);
}

TEST(Session, TwoRunsOfColumnsHashTheSame)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  std::vector<std::uint64_t> first;
  {
    auto const session{ Opened(Columns()) };
    ASSERT_NE(session, nullptr);
    first = Digests(*session, RUN_FRAMES);
  }
  auto const session{ Opened(Columns()) };
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(first, Digests(*session, RUN_FRAMES));
}

TEST(Session, ColumnsReplaysFromAStateTakenAtThreeHundred)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  auto const session{ Opened(Columns()) };
  ASSERT_NE(session, nullptr);
  session->Step(STATE_FRAME);
  auto const state{ session->SaveState() };
  ASSERT_TRUE(state.has_value()) << (state ? "" : state.error());

  std::vector<std::uint64_t> const straight{
    Digests(*session, RUN_FRAMES - STATE_FRAME) };
  ASSERT_TRUE(session->LoadState(*state).has_value());
  EXPECT_EQ(straight, Digests(*session, RUN_FRAMES - STATE_FRAME));
}

TEST(Session, AResetTellsTheObserversAndWritesOneRecord)
{
  auto const area{ tash::utilities::ScratchAreaOf("session-reset") };
  ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
  auto const path{ area->File("trace.bin") };

  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  Resets told;
  session->Observe(told);

  auto writing{ tash::trace::Writer::Open(path, "session-test") };
  ASSERT_TRUE(writing.has_value()) << writing.error();
  session->Record(&*writing);

  session->Step(STATE_FRAME);
  session->HoldPad(0, 1u << RETRO_DEVICE_ID_JOYPAD_START);
  ASSERT_TRUE(session->Reset().has_value());

  EXPECT_EQ(session->Pad(0), 0u);
  EXPECT_EQ(session->Frames(), STATE_FRAME + tash::session::RESET_FRAMES);
  ASSERT_EQ(told.at.size(), 1u);
  EXPECT_EQ(told.at[0], STATE_FRAME);

  session->Step(1);
  EXPECT_EQ(session->Frames(),
            STATE_FRAME + tash::session::RESET_FRAMES + 1);
  ASSERT_TRUE(writing->Flush().has_value());

  auto reading{ tash::trace::Reader::Open(path) };
  ASSERT_TRUE(reading.has_value()) << reading.error();
  std::vector<std::uint64_t> resets;
  reading->ForEach([&resets](auto const& record) {
    using Held = std::decay_t<decltype(record)>;
    if constexpr (std::is_same_v<Held, tash::trace::ResetRecord>)
      resets.push_back(record.frame);
  });
  ASSERT_EQ(resets.size(), 1u);
  EXPECT_EQ(resets[0], STATE_FRAME);
}

TEST(Session, AResetIsTheBootAFreshLaunchGets)
{
  std::uint64_t fresh{ 0 };
  std::vector<std::uint64_t> ahead;
  std::vector<std::byte> ram;
  {
    auto const launched{ Opened(Homebrew()) };
    ASSERT_NE(launched, nullptr);
    fresh = FirstFrameOf(*launched);
    ahead = Digests(*launched, OBSERVED_FRAMES);
    ram = WorkRam(*launched);
  }

  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  session->Step(STATE_FRAME);
  ASSERT_TRUE(session->Reset().has_value());

  // The reset ran the boot's first frame itself, so it is already drawn:
  // nothing here steps before the picture is judged.
  EXPECT_EQ(LatestOf(*session), fresh);
  EXPECT_EQ(Digests(*session, OBSERVED_FRAMES), ahead);
  EXPECT_EQ(Differing(WorkRam(*session), ram), 0u)
    << "of " << ram.size() << " bytes of work ram";
}

TEST(Session, TheCartridgeRegionIsTheRomFileAsItLies)
{
  auto const session{ Opened(Homebrew()) };
  ASSERT_NE(session, nullptr);
  std::ifstream file{ Homebrew(), std::ios::binary };
  std::vector<char> const held{ std::istreambuf_iterator<char>{ file },
                                std::istreambuf_iterator<char>{} };

  std::span<std::byte const> cartridge;
  for (tash::session::MemoryRegion const& region : session->Memory())
    if (region.name == tash::session::CARTRIDGE)
      cartridge = region.bytes;

  ASSERT_EQ(cartridge.size(), held.size());
  EXPECT_TRUE(std::ranges::equal(cartridge, std::as_bytes(std::span{ held })));
}
