#include "tash/trace/line.hpp"

#include "tash/trace/_scratch-path.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <span>
#include <variant>
#include <vector>

namespace tash::trace
{
  namespace
  {
    using detail::scratch_path::ScratchPath;

    constexpr std::int64_t CREATED{ 1'789'300'800'123'456'789 };

    constexpr std::uint16_t BEFORE_LINE_FOLD{ EARLIEST_FOLDABLE_VERSION - 1 };

    auto WroteAs(std::filesystem::path const& path, std::uint16_t version,
                 std::span<Record const> records) -> void
    {
      auto writing{ Writer::Open(path,
                                 Header{ version, CREATED, "tash" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      for (Record const& held : records)
        std::visit([&writing](auto const& record) {
          EXPECT_TRUE(writing->Write(record)); }, held);
    }

    auto Wrote(std::filesystem::path const& path,
               std::span<Record const> records) -> void
    {
      WroteAs(path, FORMAT_VERSION, records);
    }

    auto Frames(std::uint64_t from, std::uint64_t to)
      -> std::vector<Record>
    {
      std::vector<Record> made{ };
      for (std::uint64_t frame{ from }; frame < to; ++frame)
        made.push_back(FrameRecord{ frame, 0, frame, 0, 0, 0.0 });
      return made;
    }
  }

  TEST(TraceLine, AllTheFramesAreOnTheLineWhenNothingRestored)
  {
    ScratchPath const area{ };
    auto const path{ area.File("straight.bin") };
    Wrote(path, Frames(0, 6));

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 6u);
    EXPECT_TRUE(played->Holds(0));
    EXPECT_TRUE(played->Holds(5));
    EXPECT_FALSE(played->Holds(6));
  }

  TEST(TraceLine, ARestoreFoldsTheFramesMadeSinceItsCheckpoint)
  {
    ScratchPath const area{ };
    auto const path{ area.File("folded.bin") };
    std::vector<Record> written{ Frames(0, 10) };
    written.push_back(RestoreRecord{ 10, 4, "here" });
    for (Record const& made : Frames(10, 14))
      written.push_back(made);
    Wrote(path, written);

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 8u);
    EXPECT_TRUE(played->Holds(3));
    EXPECT_FALSE(played->Holds(4));
    EXPECT_FALSE(played->Holds(9));
    EXPECT_TRUE(played->Holds(10));
    EXPECT_TRUE(played->Holds(13));
    EXPECT_FALSE(played->Holds(14));
  }

  TEST(TraceLine, ANestedRestoreFoldsToTheFramesOfTheLineNotOfTheRun)
  {
    ScratchPath const area{ };
    auto const path{ area.File("nested.bin") };

    // The line is 7 frames where the second checkpoint was taken and the run
    // had made 13: folding to 13 would keep thirteen of them, not seven.
    std::vector<Record> written{ Frames(0, 10) };
    written.push_back(RestoreRecord{ 10, 4, "outer" });
    for (Record const& made : Frames(10, 20))
      written.push_back(made);
    written.push_back(RestoreRecord{ 20, 7, "inner" });
    Wrote(path, written);

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 7u);
    EXPECT_TRUE(played->Holds(3));
    EXPECT_FALSE(played->Holds(4));
    EXPECT_TRUE(played->Holds(12));
    EXPECT_FALSE(played->Holds(13));
  }

  TEST(TraceLine, ATraceTooOldToFoldIsRefusedWhenItRestored)
  {
    ScratchPath const area{ };
    auto const path{ area.File("old-restore.bin") };
    std::vector<Record> written{ Frames(0, 10) };
    written.push_back(RestoreRecord{ 10, 4, "here" });
    for (Record const& made : Frames(10, 14))
      written.push_back(made);
    WroteAs(path, BEFORE_LINE_FOLD, written);

    auto const played{ Line::Of(path) };
    ASSERT_FALSE(played.has_value());
    EXPECT_EQ(played.error(),
              std::format(
                "trace: format {} wrote a restore's target as a frame of the "
                "run and not a position on the line, so what it kept cannot "
                "be folded out of it; the bundle has to be run again to be "
                "reported or replayed",
                BEFORE_LINE_FOLD));
  }

  TEST(TraceLine, ATraceTooOldToFoldReadsWhenNothingRestored)
  {
    ScratchPath const area{ };
    auto const path{ area.File("old-reset.bin") };
    std::vector<Record> written{ Frames(0, 10) };
    written.push_back(ResetRecord{ 10 });
    for (Record const& made : Frames(10, 14))
      written.push_back(made);
    WroteAs(path, BEFORE_LINE_FOLD, written);

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 4u);
    EXPECT_FALSE(played->Holds(9));
    EXPECT_TRUE(played->Holds(13));
  }

  TEST(TraceLine, AResetFoldsTheWholeLineOff)
  {
    ScratchPath const area{ };
    auto const path{ area.File("reset.bin") };
    std::vector<Record> written{ Frames(0, 10) };
    written.push_back(ResetRecord{ 10 });
    for (Record const& made : Frames(10, 14))
      written.push_back(made);
    Wrote(path, written);

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 4u);
    EXPECT_FALSE(played->Holds(0));
    EXPECT_FALSE(played->Holds(9));
    EXPECT_TRUE(played->Holds(10));
    EXPECT_TRUE(played->Holds(13));
  }

  TEST(TraceLine, EveryTrialOfASearchFoldsBackToTheSameCheckpoint)
  {
    ScratchPath const area{ };
    auto const path{ area.File("searched.bin") };
    std::vector<Record> written{ Frames(0, 5) };
    for (std::uint64_t trial{ 0 }; trial < 3; ++trial)
    {
      for (Record const& made : Frames(5 + (trial * 4), 9 + (trial * 4)))
        written.push_back(made);
      written.push_back(RestoreRecord{ 9 + (trial * 4), 5, "trial" });
    }
    Wrote(path, written);

    auto const played{ Line::Of(path) };
    ASSERT_TRUE(played.has_value()) << played.error();
    EXPECT_EQ(played->Frames(), 5u);
    EXPECT_TRUE(played->Holds(4));
    EXPECT_FALSE(played->Holds(5));
  }
}
