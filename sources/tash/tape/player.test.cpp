#include "tash/tape/player.hpp"

#include "tash/perception/frame-hash.hpp"
#include "tash/tape/tape.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tash::tape
{
  using utilities::Result;

  namespace
  {
    constexpr std::uint64_t SHORT_TIMEOUT{ 10 };
    constexpr std::uint32_t START{ 1u << 3 };

    auto Root() -> std::filesystem::path
    {
      std::filesystem::path here{ std::filesystem::current_path() };
      while (!std::filesystem::exists(here / "buildutil.toml")
             && here.has_relative_path())
        here = here.parent_path();
      return here;
    }

    auto DemoTape() -> std::filesystem::path
    {
      return Root() / "examples/homebrew/tapes/demo.yaml";
    }

    auto Opened() -> std::unique_ptr<session::Session>
    {
      auto const beside{ utilities::ExecutableDirectory() };
      EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());

      session::SessionSettings settings;
      settings.core = *beside / "genesis_plus_gx_libretro.so";
      settings.rom  = Root() / "examples/homebrew/zsenilia.bin";
      auto session{ session::Session::Open(std::move(settings)) };
      EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
      return session ? std::move(*session) : nullptr;
    }

    // Every frame the player steps, digested where the tape cannot reach it.
    class Digests : public session::FrameObserver
    {
    public:
      auto OnFrame(bus::FrameView const& frame, std::int64_t) -> void override
      {
        auto const digest{ perception::ExactHash(frame) };
        EXPECT_TRUE(digest.has_value()) << (digest ? "" : digest.error());
        _digests.push_back(digest.value_or(0));
      }

      [[nodiscard]] auto Taken() const -> std::vector<std::uint64_t> const&
      { return _digests; }

    private:
      std::vector<std::uint64_t> _digests{ };
    };

    [[nodiscard]] auto Played(Digests& digests) -> Result<PlayCounts>
    {
      auto player{ Player::Of(DemoTape()) };
      EXPECT_TRUE(player.has_value()) << (player ? "" : player.error());
      if (!player)
        return utilities::Forwarded(player);

      std::unique_ptr<session::Session> const session{ Opened() };
      if (session == nullptr)
        return utilities::Refused("the session did not open");
      session->Observe(digests);
      return player->Play(*session);
    }

    // What the cartridge has written, which is where a reset that is not a
    // power cycle shows up long before the picture does.
    [[nodiscard]] auto WorkRam(session::Session const& session)
      -> std::vector<std::byte>
    {
      for (session::MemoryRegion const& region : session.Memory())
        if (region.name == "system")
          return std::vector<std::byte>{ region.bytes.begin(),
                                         region.bytes.end() };
      ADD_FAILURE() << "the core exposes no system ram";
      return { };
    }

    [[nodiscard]] auto Differing(std::vector<std::byte> const& one,
                                 std::vector<std::byte> const& other)
      -> std::size_t
    {
      if (one.size() != other.size())
        return std::max(one.size(), other.size());
      std::size_t count{ 0 };
      for (std::size_t at{ 0 }; at < one.size(); ++at)
        count += one[at] != other[at] ? 1u : 0u;
      return count;
    }

    [[nodiscard]] auto Waiting(std::string hash, Recovery recovery) -> Tape
    {
      TapeHeader header{ };
      header.name           = "never";
      header.timeout_frames = SHORT_TIMEOUT;
      header.on_timeout     = recovery;

      Segment segment{ };
      segment.name         = "the-screen-that-never-comes";
      segment.anchor       = Anchor{ };
      segment.anchor->kind = AnchorKind::EXACT_HASH;
      segment.anchor->hash = std::move(hash);
      return Tape{ header, { segment } };
    }
  }

  TEST(TapePlayer, TheDemoTapePlaysToTheSameFramesTwice)
  {
    Digests first{ };
    auto const once{ Played(first) };
    ASSERT_TRUE(once.has_value()) << (once ? "" : once.error());
    EXPECT_EQ(once->segments, 2u);
    EXPECT_EQ(once->transitions, 4u);
    EXPECT_GT(once->waited, 0u);
    EXPECT_EQ(once->retries, 0u);
    EXPECT_EQ(first.Taken().size(), once->frames);

    Digests again{ };
    auto const twice{ Played(again) };
    ASSERT_TRUE(twice.has_value()) << (twice ? "" : twice.error());
    EXPECT_EQ(*twice, *once);
    EXPECT_EQ(again.Taken(), first.Taken());
  }

  TEST(TapePlayer, TheDemoTapePlaysTheSameAfterAResetInTheSameSession)
  {
    auto player{ Player::Of(DemoTape()) };
    ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
    std::unique_ptr<session::Session> const session{ Opened() };
    ASSERT_NE(session, nullptr);

    Digests digests{ };
    session->Observe(digests);
    auto const once{ player->Play(*session) };
    ASSERT_TRUE(once.has_value()) << (once ? "" : once.error());
    std::vector<std::uint64_t> const first{ digests.Taken() };
    std::vector<std::byte> const ram{ WorkRam(*session) };

    ASSERT_TRUE(session->Reset().has_value());
    auto const twice{ player->Play(*session) };
    ASSERT_TRUE(twice.has_value()) << (twice ? "" : twice.error());

    // The reset drew the boot's first frame itself, which is the frame the
    // tape's opening anchor would otherwise have waited for.
    EXPECT_EQ(twice->segments, once->segments);
    EXPECT_EQ(twice->transitions, once->transitions);
    EXPECT_EQ(twice->retries, once->retries);
    EXPECT_EQ(twice->frames + session::RESET_FRAMES, once->frames);
    EXPECT_EQ(twice->waited + session::RESET_FRAMES, once->waited);
    EXPECT_EQ(session->Frames(),
              once->frames + session::RESET_FRAMES + twice->frames);
    ASSERT_EQ(digests.Taken().size(), first.size() * 2);
    EXPECT_TRUE(std::equal(first.begin(), first.end(),
                           digests.Taken().begin() + first.size()));
    EXPECT_EQ(Differing(WorkRam(*session), ram), 0u)
      << "of " << ram.size() << " bytes of work ram";
  }

  TEST(TapePlayer, EveryTransitionLandsInTheTrace)
  {
    auto const scratch{ utilities::ScratchAreaOf("tape-play-trace") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    auto player{ Player::Of(DemoTape()) };
    ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
    std::unique_ptr<session::Session> const session{ Opened() };
    ASSERT_NE(session, nullptr);

    std::filesystem::path const path{ scratch->File("play.bin") };
    auto writer{ trace::Writer::Open(path, "tape-tests") };
    ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());

    session->Record(&*writer);
    auto const counts{ player->Play(*session) };
    ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
    ASSERT_TRUE(writer->Flush().has_value());

    auto source{ trace::Reader::Open(path) };
    ASSERT_TRUE(source.has_value()) << (source ? "" : source.error());

    std::vector<trace::InputRecord> inputs{ };
    while (auto const record{ source->Next() })
      if (auto const* input{ std::get_if<trace::InputRecord>(&*record) })
        inputs.push_back(*input);

    ASSERT_EQ(inputs.size(), counts->transitions);
    EXPECT_EQ(inputs[0].pad, START);
    EXPECT_EQ(inputs[1].pad, 0u);
    EXPECT_LT(inputs[0].frame, inputs[2].frame);
  }

  TEST(TapePlayer, AMissedAnchorNamesItsSegment)
  {
    auto player{ Player::For(Waiting("0000000000000000", Recovery::FAIL),
                             DemoTape()) };
    ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
    std::unique_ptr<session::Session> const session{ Opened() };
    ASSERT_NE(session, nullptr);

    auto const counts{ player->Play(*session) };
    ASSERT_FALSE(counts.has_value());
    EXPECT_NE(counts.error().find("the-screen-that-never-comes"),
              std::string::npos) << counts.error();
    EXPECT_NE(counts.error().find("exact_hash"), std::string::npos)
      << counts.error();
    EXPECT_EQ(session->Frames(), SHORT_TIMEOUT);
  }

  TEST(TapePlayer, ARetryGoesBackToTheSegmentStartAndTriesAgain)
  {
    auto player{ Player::For(Waiting("0000000000000000", Recovery::RETRY),
                             DemoTape()) };
    ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
    std::unique_ptr<session::Session> const session{ Opened() };
    ASSERT_NE(session, nullptr);

    auto const counts{ player->Play(*session) };
    ASSERT_FALSE(counts.has_value());
    EXPECT_NE(counts.error().find("the-screen-that-never-comes"),
              std::string::npos) << counts.error();

    // One attempt, then DEFAULT_RETRIES more from the state it saved.
    EXPECT_EQ(session->Frames(), SHORT_TIMEOUT * (DEFAULT_RETRIES + 1));
  }

  TEST(TapePlayer, ASegmentWaitsOnEitherSideOfAnOrAnchor)
  {
    auto const scratch{ utilities::ScratchAreaOf("tape-any-of") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    Result<Anchor> const either{
      AnchorFrom("exact_hash 0000000000000000 or none") };
    ASSERT_TRUE(either.has_value()) << (either ? "" : either.error());

    Tape written{ Waiting("0000000000000000", Recovery::FAIL) };
    written.segments[0].anchor = *either;
    std::filesystem::path const path{ scratch->File("either.yaml") };
    ASSERT_TRUE(WriteTape(written, path).has_value());

    // The disjunction survives the yaml, so a tape says it as a predicate
    // typed at a prompt does.
    auto const read{ TapeFrom(path) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(read->segments.front().Waits(), *either);

    auto player{ Player::For(*read, path) };
    ASSERT_TRUE(player.has_value()) << (player ? "" : player.error());
    std::unique_ptr<session::Session> const session{ Opened() };
    ASSERT_NE(session, nullptr);

    // The hash side never comes; the none side does, on the first frame.
    auto const counts{ player->Play(*session) };
    ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
    EXPECT_LT(session->Frames(), SHORT_TIMEOUT);
  }
}
