#include "tash/perception/frame-hash.hpp"
#include "tash/python/sampler-watches.hpp"
#include "tash/session/rom-file.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/player.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tash/profile.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/sampler.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace
{
  // roms/ is the machine's own cartridge library, which the profile names too.
  constexpr auto COLUMNS = "roms/Genesis/Columns (USA, Europe).zip";

  // The playfield segment and the one that checks the column landed.
  constexpr std::size_t LAST_SEGMENT_FRAMES{ 151 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Columns() -> std::filesystem::path
  {
    return Root() / COLUMNS;
  }

  auto TapePath() -> std::filesystem::path
  {
    return Root() / "examples/columns/tapes/title-to-game.yaml";
  }

  auto ProfilePath() -> std::filesystem::path
  {
    return Root() / "examples/columns/profile.yaml";
  }

  class Digests : public tash::session::FrameObserver
  {
  public:
    auto OnFrame(tash::bus::FrameView const& frame, std::int64_t)
      -> void override
    {
      auto const digest{ tash::perception::ExactHash(frame) };
      EXPECT_TRUE(digest.has_value()) << (digest ? "" : digest.error());
      _taken.push_back(digest.value_or(0));
    }

    [[nodiscard]] auto Taken() const -> std::vector<std::uint64_t> const&
    { return _taken; }

  private:
    std::vector<std::uint64_t> _taken{ };
  };

  struct Played
  {
    bool                       opened{ false };
    tash::tape::PlayCounts     counts{ };
    std::vector<std::uint64_t> digests{ };
    std::int64_t               level{ -1 };
    std::int64_t               score{ -1 };
  };

  // The whole exit check in one place: the profile's session, its watches
  // sampled so the tape's watch anchor has somebody to ask, and the tape.
  // Everything it opened is gone by the time it returns, because a second
  // libretro core cannot live in this process beside the first.
  auto Play() -> Played
  {
    Played done{ };
    Digests digests;

    auto const profile{ tash::cli::ProfileFrom(ProfilePath()) };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return done;

    auto settings{ tash::cli::SettingsFrom(*profile) };
    EXPECT_TRUE(settings.has_value()) << (settings ? "" : settings.error());
    if (!settings)
      return done;
    // The profile's rom is relative to where `tash run` is invoked, which
    // is the repository root; a test binary runs from its build tree.
    settings->rom = Columns();

    auto opened{ tash::session::Session::Open(std::move(*settings)) };
    EXPECT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
    if (!opened)
      return done;
    std::unique_ptr<tash::session::Session> const session{
      std::move(*opened) };

    auto const watched{ tash::cli::WatchesFrom(*profile) };
    EXPECT_TRUE(watched.has_value()) << (watched ? "" : watched.error());
    if (!watched)
      return done;

    auto opened_sampler{ tash::watches::Sampler::Open(
      *watched, tash::watches::MemoryMap::Of(*session)) };
    EXPECT_TRUE(opened_sampler.has_value())
      << (opened_sampler ? "" : opened_sampler.error());
    if (!opened_sampler)
      return done;
    std::unique_ptr<tash::watches::Sampler> const sampler{
      std::move(*opened_sampler) };

    session->Observe(*sampler);
    session->Observe(digests);

    auto player{ tash::tape::Player::Of(TapePath()) };
    EXPECT_TRUE(player.has_value()) << (player ? "" : player.error());
    if (!player)
      return done;

    tash::python::SamplerWatches const asked{ *sampler };
    tash::tape::PlayOptions options;
    options.watches = &asked;

    auto const counts{ player->Play(*session, options) };
    EXPECT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
    if (counts)
      done.counts = *counts;
    done.digests = digests.Taken();
    done.level = sampler->Value("level").value_or(-1);
    done.score = sampler->Value("score").value_or(-1);
    done.opened = true;
    return done;
  }
}

TEST(ColumnsTape, TheTapeChecksWithoutARom)
{
  auto const written{ tash::tape::TapeFrom(TapePath()) };
  ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
  EXPECT_EQ(written->header.name, "title-to-game");
  EXPECT_EQ(written->segments.size(), 5u);

  auto const sound{ tash::tape::Checked(*written) };
  EXPECT_TRUE(sound.has_value()) << (sound ? "" : sound.error());

  // Player::For is what loads the anchor crops beside the tape.
  auto const player{ tash::tape::Player::For(*written, TapePath()) };
  ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
  EXPECT_EQ(player->Segments(), written->segments.size());
}

TEST(ColumnsTape, PlaysFromPowerOnToTheFirstColumn)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  Played const done{ Play() };
  ASSERT_TRUE(done.opened);

  EXPECT_EQ(done.counts.segments, 5u);
  EXPECT_EQ(done.counts.transitions, 10u);
  EXPECT_EQ(done.counts.retries, 0u);
  EXPECT_EQ(done.counts.frames, done.digests.size());

  EXPECT_EQ(done.level, 0);

  // The tape's own exit check: the first column is on the floor, which
  // Columns pays for.
  EXPECT_GT(done.score, 0);
}

TEST(ColumnsTape, TwoPlaysOfTheTapeHashTheSame)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  Played const first{ Play() };
  Played const second{ Play() };
  ASSERT_TRUE(first.opened);
  ASSERT_TRUE(second.opened);

  ASSERT_GE(first.digests.size(), LAST_SEGMENT_FRAMES);
  EXPECT_EQ(first.counts.frames, second.counts.frames);

  std::vector<std::uint64_t> const first_tail{
    first.digests.end() - LAST_SEGMENT_FRAMES, first.digests.end() };
  std::vector<std::uint64_t> const second_tail{
    second.digests.end() - LAST_SEGMENT_FRAMES, second.digests.end() };
  EXPECT_EQ(first_tail, second_tail);
  EXPECT_EQ(first.digests, second.digests);
}
