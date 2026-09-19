#include "tash/journal/frame-journal.hpp"

#include "tash/journal/_synthetic-frame.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <string>
#include <variant>
#include <vector>

namespace
{
  using tash::journal::FrameJournal;
  using tash::journal::JournalCounts;
  using tash::journal::testing::FRAME_WIDTH;
  using tash::journal::testing::SyntheticFrame;
  using tash::trace::FrameRecord;

  constexpr std::uint16_t RGB565_BLACK{ 0x0000 };
  constexpr std::uint16_t RGB565_WHITE{ 0xFFFF };
  constexpr double FRAME_SECONDS{ 1.0 / 60.0 };
  constexpr std::int64_t NANOSECONDS_PER_SECOND{ 1'000'000'000 };

  auto Scratch() -> oxbox::platform::ScratchArea
  {
    auto area{ tash::utilities::ScratchAreaOf("journal-test") };
    EXPECT_TRUE(area.has_value()) << (area ? "" : area.error());
    return std::move(*area);
  }

  auto Opened(std::filesystem::path const& path)
    -> std::unique_ptr<FrameJournal>
  {
    auto journal{ FrameJournal::Open(path, "tash-test") };
    EXPECT_TRUE(journal.has_value()) << (journal ? "" : journal.error());
    return journal ? std::move(*journal) : nullptr;
  }

  auto FramesIn(std::filesystem::path const& path) -> std::vector<FrameRecord>
  {
    std::vector<FrameRecord> frames;
    auto reader{ tash::trace::Reader::Open(path) };
    EXPECT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    if (!reader)
      return frames;
    reader->ForEach([&frames](auto const& record) {
      if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                   FrameRecord>)
        frames.push_back(record);
    });
    EXPECT_FALSE(reader->Truncated());
    return frames;
  }
}

TEST(FrameJournal, WritesOneRecordPerFrameInOrder)
{
  auto const scratch{ Scratch() };
  auto const path{ scratch.File("frames.tash") };
  constexpr std::uint64_t FRAMES{ 120 };

  SyntheticFrame frame{ RGB565_BLACK };
  JournalCounts counts;
  {
    auto const journal{ Opened(path) };
    ASSERT_NE(journal, nullptr);
    for (std::uint64_t number{ 0 }; number < FRAMES; ++number)
    {
      frame.Set(static_cast<std::uint32_t>(number % FRAME_WIDTH), 0u,
                RGB565_WHITE);
      journal->OnFrame(
        frame.View(number, static_cast<double>(number) * FRAME_SECONDS),
        static_cast<std::int64_t>(static_cast<double>(number) * FRAME_SECONDS
                                  * NANOSECONDS_PER_SECOND));
    }
    auto const closed{ journal->Close() };
    ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
    counts = *closed;
  }

  EXPECT_EQ(counts.recorded, FRAMES);
  EXPECT_EQ(counts.dropped, 0u);
  auto const frames{ FramesIn(path) };
  ASSERT_EQ(frames.size(), FRAMES);
  for (std::size_t index{ 1 }; index < frames.size(); ++index)
  {
    EXPECT_EQ(frames[index].frame, frames[index - 1].frame + 1);
    EXPECT_GT(frames[index].harness_time, frames[index - 1].harness_time);
  }
}

TEST(FrameJournal, RecordsTheHashesAndTheChangeOfEachFrame)
{
  auto const scratch{ Scratch() };
  auto const path{ scratch.File("hashes.tash") };

  SyntheticFrame const black{ RGB565_BLACK };
  SyntheticFrame const white{ RGB565_WHITE };
  {
    auto const journal{ Opened(path) };
    ASSERT_NE(journal, nullptr);
    journal->OnFrame(black.View(0u, 0.0), 0);
    journal->OnFrame(black.View(1u, FRAME_SECONDS), 16'666'666);
    journal->OnFrame(white.View(2u, 2 * FRAME_SECONDS), 33'333'333);
    auto const closed{ journal->Close() };
    ASSERT_TRUE(closed.has_value()) << (closed ? "" : closed.error());
    EXPECT_EQ(closed->recorded, 3u);
    EXPECT_EQ(closed->dropped, 0u);
  }

  auto const frames{ FramesIn(path) };
  ASSERT_EQ(frames.size(), 3u);
  auto const black_hash{ tash::perception::ExactHash(black.View(0u, 0.0)) };
  ASSERT_TRUE(black_hash.has_value());
  EXPECT_EQ(frames[0].hash_exact, *black_hash);
  EXPECT_EQ(frames[1].hash_exact, *black_hash);
  EXPECT_NE(frames[2].hash_exact, *black_hash);

  EXPECT_EQ(frames[0].change_amount, 0.0);
  EXPECT_EQ(frames[1].change_amount, 0.0);
  EXPECT_GT(frames[2].change_amount, 0.0);
  EXPECT_EQ(frames[1].harness_time, 16'666'666);
}
