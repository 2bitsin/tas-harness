#include "tash/watches/memory-search.hpp"

#include "_synthetic-memory.hpp"
#include "tash/watches/search-script.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
  using namespace tash::watches;
  using namespace tash::watches::testing;

  constexpr std::uint32_t SCORE_AT{ 12u };
  constexpr std::uint32_t DECOY_AT{ 20u };

  // Every word of the area is a candidate until a comparison says otherwise.
  constexpr std::size_t WORDS{ AREA_BYTES / 2u };

  [[nodiscard]] auto Holds(MemorySearch const& search, std::uint32_t address)
    -> bool
  {
    return std::ranges::any_of(search.Candidates(),
                               [address](Candidate const& candidate)
                               { return candidate.address == address; });
  }
}

TEST(MemorySearch, SeedsEveryAlignedAddressAndNarrowsOnChange)
{
  SyntheticMemory memory;
  MemorySearch search{ memory.Map(), NumberFormat{} };

  ASSERT_TRUE(search.Snapshot().has_value());
  EXPECT_EQ(search.Count(), WORDS);
  EXPECT_EQ(search.Stride(), WIDTH_WORD);

  memory.Write16(SCORE_AT, 100u);
  ASSERT_TRUE(search.Narrow(Comparison::CHANGED).has_value());
  ASSERT_EQ(search.Count(), 1u);
  EXPECT_EQ(search.Candidates().front().address, SCORE_AT);
  EXPECT_EQ(search.Candidates().front().value, 100);
  EXPECT_EQ(search.AreaName(search.Candidates().front().area), "system");
}

TEST(MemorySearch, EqualKeepsWhatStoodStill)
{
  SyntheticMemory memory;
  memory.Write16(SCORE_AT, 7u);
  MemorySearch search{ memory.Map(), NumberFormat{} };
  ASSERT_TRUE(search.Snapshot().has_value());

  memory.Write16(SCORE_AT, 8u);
  ASSERT_TRUE(search.Narrow(Comparison::EQUAL).has_value());
  EXPECT_EQ(search.Count(), WORDS - 1u);
  EXPECT_FALSE(Holds(search, SCORE_AT));
}

TEST(MemorySearch, IncreasedAndDecreasedSplitTwoMovingWords)
{
  SyntheticMemory memory;
  memory.Write16(SCORE_AT, 100u);
  memory.Write16(DECOY_AT, 100u);
  MemorySearch search{ memory.Map(), NumberFormat{} };
  ASSERT_TRUE(search.Snapshot().has_value());

  memory.Write16(SCORE_AT, 150u);
  memory.Write16(DECOY_AT, 50u);
  ASSERT_TRUE(search.Narrow(Comparison::INCREASED).has_value());
  ASSERT_EQ(search.Count(), 1u);
  EXPECT_EQ(search.Candidates().front().address, SCORE_AT);

  MemorySearch falling{ memory.Map(), NumberFormat{} };
  ASSERT_TRUE(falling.Snapshot().has_value());
  memory.Write16(DECOY_AT, 10u);
  ASSERT_TRUE(falling.Narrow(Comparison::DECREASED).has_value());
  ASSERT_EQ(falling.Count(), 1u);
  EXPECT_EQ(falling.Candidates().front().address, DECOY_AT);
}

TEST(MemorySearch, ValueFindsTheNumberOnScreen)
{
  SyntheticMemory memory;
  memory.Write16(SCORE_AT, 4242u);
  MemorySearch search{ memory.Map(), NumberFormat{} };
  ASSERT_TRUE(search.Snapshot().has_value());
  ASSERT_TRUE(search.Narrow(Comparison::VALUE, 4242).has_value());
  ASSERT_EQ(search.Count(), 1u);
  EXPECT_EQ(search.Candidates().front().address, SCORE_AT);
}

TEST(MemorySearch, ASecondSnapshotMovesTheBaselineWithoutDropping)
{
  SyntheticMemory memory;
  MemorySearch search{ memory.Map(), NumberFormat{} };
  ASSERT_TRUE(search.Snapshot().has_value());
  memory.Write16(SCORE_AT, 5u);
  ASSERT_TRUE(search.Snapshot().has_value());
  EXPECT_EQ(search.Count(), WORDS);

  ASSERT_TRUE(search.Narrow(Comparison::EQUAL).has_value());
  EXPECT_EQ(search.Count(), WORDS);
}

TEST(MemorySearch, NarrowingBeforeASnapshotIsAnAnswer)
{
  SyntheticMemory memory;
  MemorySearch search{ memory.Map(), NumberFormat{} };
  auto const early{ search.Narrow(Comparison::CHANGED) };
  ASSERT_FALSE(early.has_value());
  EXPECT_NE(early.error().find("snapshot"), std::string::npos);
  EXPECT_FALSE(search.Seeded());
}

TEST(MemorySearch, AStrideOfOneSeesTheUnalignedWord)
{
  SyntheticMemory memory;
  MemorySearch search{ memory.Map(), NumberFormat{}, 1u };
  ASSERT_TRUE(search.Snapshot().has_value());
  EXPECT_EQ(search.Count(), AREA_BYTES - 1u);

  memory.Write16(SCORE_AT + 1u, 300u);
  ASSERT_TRUE(search.Narrow(Comparison::CHANGED).has_value());
  EXPECT_TRUE(Holds(search, SCORE_AT + 1u));
}

TEST(MemorySearch, ComparisonsAreSpeltOutOrRefused)
{
  EXPECT_EQ(ComparisonOf("increased"), Comparison::INCREASED);
  EXPECT_EQ(NameOf(Comparison::VALUE), "value");
  EXPECT_FALSE(ComparisonOf("bigger").has_value());
}
