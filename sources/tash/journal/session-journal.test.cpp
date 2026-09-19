#include "tash/journal/frame-journal.hpp"

#include "tash/session/session.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>

namespace
{
  using tash::journal::FrameJournal;
  using tash::session::Session;
  using tash::session::SessionSettings;

  constexpr std::uint64_t RUN_FRAMES{ 120 };

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
    SessionSettings settings;
    settings.core = *beside / "genesis_plus_gx_libretro.so";
    settings.rom = Root() / "examples/homebrew/zsenilia.bin";
    auto session{ Session::Open(std::move(settings)) };
    EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
    return session ? std::move(*session) : nullptr;
  }
}

TEST(SessionJournal, TheHomebrewRunLandsOneRecordPerFrame)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("journal-session") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const path{ scratch->File("homebrew.tash") };

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  auto const journal{ FrameJournal::Open(path, "tash-test") };
  ASSERT_TRUE(journal.has_value()) << (journal ? "" : journal.error());
  session->Observe(**journal);

  session->Step(RUN_FRAMES);
  auto const counts{ (*journal)->Close() };
  ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
  EXPECT_EQ(counts->recorded, session->Frames());
  EXPECT_EQ(counts->dropped, 0u);

  std::uint64_t frames{ 0 };
  std::uint64_t changed{ 0 };
  auto reader{ tash::trace::Reader::Open(path) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&frames, &changed](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::FrameRecord>)
    {
      EXPECT_EQ(record.frame, frames);
      EXPECT_GE(record.harness_time, 0);
      ++frames;
      if (record.change_amount > 0.0)
        ++changed;
    }
  });
  EXPECT_FALSE(reader->Truncated());
  EXPECT_EQ(frames, session->Frames());
  EXPECT_GT(changed, 0u);
}
