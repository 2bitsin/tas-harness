#include "tash/watches/region-order.hpp"

#include "_synthetic-memory.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
  using tash::watches::Endianness;
  using tash::watches::RegionOrder;
  using tash::watches::TextIn;
  using tash::watches::testing::SyntheticMemory;

  constexpr std::uint32_t AT{ 8 };

  // "Take Oil" as the 68000 wrote it, and as Genesis Plus GX hands it back:
  // every pair of bytes the other way round.
  auto Swapped() -> SyntheticMemory
  {
    SyntheticMemory memory;
    memory.Poke(AT, { 'a', 'T', 'e', 'k', 'O', ' ', 'l', 'i', '\0', '\0' });
    return memory;
  }

  auto Addressed() -> SyntheticMemory
  {
    SyntheticMemory memory;
    memory.Poke(AT, { 'T', 'a', 'k', 'e', ' ', 'O', 'i', 'l', '\0', '\0' });
    return memory;
  }
}

TEST(RegionOrder, GenesisPlusGxKeepsItsWorkRamByteSwapped)
{
  EXPECT_EQ(RegionOrder("Genesis Plus GX", "system"), Endianness::SWAPPED);
  EXPECT_EQ(RegionOrder("Genesis Plus GX", "video"), Endianness::BIG);
  EXPECT_EQ(RegionOrder("Gambatte", "system"), Endianness::BIG);
  EXPECT_EQ(RegionOrder("", ""), Endianness::BIG);
}

TEST(RegionOrder, ASwappedRegionReadsBackInAddressOrder)
{
  SyntheticMemory const memory{ Swapped() };
  EXPECT_EQ(TextIn(memory.View(), AT, 8, Endianness::SWAPPED), "Take Oil");
}

TEST(RegionOrder, AnUnswappedRegionReadsBackAsItLies)
{
  SyntheticMemory const memory{ Addressed() };
  EXPECT_EQ(TextIn(memory.View(), AT, 8, Endianness::BIG), "Take Oil");

  // The same bytes read the other way round, which is what a hard-coded
  // swap would have answered.
  EXPECT_EQ(TextIn(memory.View(), AT, 8, Endianness::SWAPPED), "aTekO li");
}

TEST(RegionOrder, TheTextIsCutAtTheFirstNull)
{
  SyntheticMemory const swapped{ Swapped() };
  EXPECT_EQ(TextIn(swapped.View(), AT, 32, Endianness::SWAPPED), "Take Oil");

  SyntheticMemory const addressed{ Addressed() };
  EXPECT_EQ(TextIn(addressed.View(), AT, 32, Endianness::BIG), "Take Oil");

  // An odd address in a swapped region starts on the byte before it.
  EXPECT_EQ(TextIn(swapped.View(), AT + 1u, 8, Endianness::SWAPPED),
            "ake Oil");
}

TEST(RegionOrder, ARunOfNullsIsTheEmptyString)
{
  SyntheticMemory const memory;
  EXPECT_EQ(TextIn(memory.View(), 0, 16, Endianness::SWAPPED), std::string{ });
  EXPECT_EQ(TextIn(memory.View(), 0, 16, Endianness::BIG), std::string{ });
}
