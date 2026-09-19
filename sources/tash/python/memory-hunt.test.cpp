#include "tash/python/memory-hunt.hpp"

#include "tash/watches/_synthetic-memory.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace
{
  using tash::python::CANDIDATE_LIMIT;
  using tash::python::MemoryHunt;
  using tash::watches::Candidate;
  using tash::watches::Endianness;
  using tash::watches::NumberFormat;
  using tash::watches::testing::AREA_BYTES;
  using tash::watches::testing::SyntheticMemory;

  constexpr std::uint32_t SCORE_AT{ 12u };
  constexpr std::uint32_t DECOY_AT{ 20u };
  constexpr std::size_t WORDS{ AREA_BYTES / 2u };

  [[nodiscard]] auto Hunting(SyntheticMemory const& memory,
                             NumberFormat format = NumberFormat{ },
                             std::uint32_t stride = 0u) -> MemoryHunt
  {
    auto hunt{ MemoryHunt::Of(memory.Map(), "system", format, stride) };
    EXPECT_TRUE(hunt.has_value()) << (hunt ? "" : hunt.error());
    return std::move(*hunt);
  }

  [[nodiscard]] auto Holds(MemoryHunt const& hunt, std::uint32_t address)
    -> bool
  {
    return std::ranges::any_of(hunt.Candidates(hunt.Count()),
                               [address](Candidate const& candidate)
                               { return candidate.address == address; });
  }
}

TEST(MemoryHunt, TheFirstStepSeedsAndTheNextNarrows)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(memory) };
  EXPECT_FALSE(hunt.Seeded());

  auto const seeded{ hunt.Step("changed", std::nullopt) };
  ASSERT_TRUE(seeded.has_value()) << (seeded ? "" : seeded.error());
  EXPECT_EQ(*seeded, WORDS);
  EXPECT_TRUE(hunt.Seeded());

  memory.Write16(SCORE_AT, 100u);
  auto const narrowed{ hunt.Step("changed", std::nullopt) };
  ASSERT_TRUE(narrowed.has_value()) << (narrowed ? "" : narrowed.error());
  EXPECT_EQ(*narrowed, 1u);
  ASSERT_EQ(hunt.Candidates(CANDIDATE_LIMIT).size(), 1u);
  EXPECT_EQ(hunt.Candidates(CANDIDATE_LIMIT).front().address, SCORE_AT);
  EXPECT_EQ(hunt.Candidates(CANDIDATE_LIMIT).front().value, 100);
}

TEST(MemoryHunt, EveryStepCountsFromTheStepBefore)
{
  SyntheticMemory memory;
  memory.Write16(SCORE_AT, 10u);
  memory.Write16(DECOY_AT, 10u);
  MemoryHunt hunt{ Hunting(memory) };
  ASSERT_TRUE(hunt.Step("increased", std::nullopt).has_value());

  memory.Write16(SCORE_AT, 20u);
  memory.Write16(DECOY_AT, 20u);
  ASSERT_TRUE(hunt.Step("increased", std::nullopt).has_value());
  EXPECT_EQ(hunt.Count(), 2u);

  memory.Write16(SCORE_AT, 30u);
  auto const again{ hunt.Step("increased", std::nullopt) };
  ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
  EXPECT_EQ(*again, 1u);
  EXPECT_TRUE(Holds(hunt, SCORE_AT));
}

TEST(MemoryHunt, AValueStepLooksForTheNumberOnTheScreen)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(memory) };
  ASSERT_TRUE(hunt.Step("value", 0).has_value());

  memory.Write16(SCORE_AT, 391u);
  auto const found{ hunt.Step("value", 391) };
  ASSERT_TRUE(found.has_value()) << (found ? "" : found.error());
  EXPECT_EQ(*found, 1u);
  EXPECT_TRUE(Holds(hunt, SCORE_AT));
}

TEST(MemoryHunt, AValueBelongsToAValueStepAndNowhereElse)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(memory) };

  auto const bare{ hunt.Step("value", std::nullopt) };
  ASSERT_FALSE(bare.has_value());
  EXPECT_NE(bare.error().find("needs the number"), std::string::npos);

  auto const spare{ hunt.Step("changed", 7) };
  ASSERT_FALSE(spare.has_value());
  EXPECT_NE(spare.error().find("takes no"), std::string::npos);

  auto const misspelt{ hunt.Step("bigger", std::nullopt) };
  ASSERT_FALSE(misspelt.has_value());
  EXPECT_NE(misspelt.error().find("not a comparison"), std::string::npos);
  EXPECT_FALSE(hunt.Seeded());
}

TEST(MemoryHunt, CandidatesStopAtTheLimitTheyAreGiven)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(memory) };
  ASSERT_TRUE(hunt.Step("changed", std::nullopt).has_value());

  EXPECT_EQ(hunt.Candidates(4u).size(), 4u);
  EXPECT_EQ(hunt.Candidates(WORDS * 2u).size(), WORDS);
  EXPECT_EQ(hunt.Candidates(4u).front().address, 0u);
}

TEST(MemoryHunt, ResetStartsTheHuntOver)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(memory) };
  ASSERT_TRUE(hunt.Step("changed", std::nullopt).has_value());
  memory.Write16(SCORE_AT, 5u);
  ASSERT_TRUE(hunt.Step("changed", std::nullopt).has_value());
  ASSERT_EQ(hunt.Count(), 1u);

  hunt.Reset();
  EXPECT_FALSE(hunt.Seeded());
  EXPECT_EQ(hunt.Count(), 0u);
  auto const seeded{ hunt.Step("changed", std::nullopt) };
  ASSERT_TRUE(seeded.has_value()) << (seeded ? "" : seeded.error());
  EXPECT_EQ(*seeded, WORDS);
}

TEST(MemoryHunt, TheWidthEndiannessAndStrideAreTheCallersOwn)
{
  SyntheticMemory memory;
  MemoryHunt hunt{ Hunting(
    memory, NumberFormat{ tash::watches::WIDTH_WORD, Endianness::LITTLE },
    1u) };
  ASSERT_TRUE(hunt.Step("changed", std::nullopt).has_value());
  EXPECT_EQ(hunt.Count(), AREA_BYTES - 1u);
  EXPECT_EQ(hunt.Stride(), 1u);
  EXPECT_EQ(hunt.Region(), "system");

  memory.Poke(SCORE_AT, { 0x2c, 0x01 });
  ASSERT_TRUE(hunt.Step("value", 300).has_value());
  EXPECT_EQ(hunt.Count(), 1u);
  EXPECT_TRUE(Holds(hunt, SCORE_AT));
}

TEST(MemoryHunt, ARegionTheCoreDoesNotHaveIsRefused)
{
  SyntheticMemory memory;
  auto const nowhere{ MemoryHunt::Of(memory.Map(), "vram", NumberFormat{ },
                                     0u) };
  ASSERT_FALSE(nowhere.has_value());
  EXPECT_NE(nowhere.error().find("system"), std::string::npos);

  auto const odd{ MemoryHunt::Of(memory.Map(), "system",
                                 NumberFormat{ 3u }, 0u) };
  EXPECT_FALSE(odd.has_value());
}
