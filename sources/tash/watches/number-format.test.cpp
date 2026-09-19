#include "tash/watches/number-format.hpp"

#include "_synthetic-memory.hpp"
#include "tash/watches/watch.hpp"
#include "tash/watches/watch-spec.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace tash::watches;
  using namespace tash::watches::testing;

  // 0x12 0x34 0x56 0x78 at 0, and 0xFF 0xFE at 8.
  [[nodiscard]] auto Probe() -> SyntheticMemory
  {
    SyntheticMemory memory;
    memory.Poke(0u, { 0x12, 0x34, 0x56, 0x78 });
    memory.Poke(8u, { 0xFF, 0xFE });
    return memory;
  }

  [[nodiscard]] auto Read(SyntheticMemory const& memory, std::uint32_t at,
                          std::uint32_t width, Endianness endianness,
                          bool is_signed) -> std::int64_t
  {
    auto const value{ ReadNumber(memory.View(), at,
                                 NumberFormat{ width, endianness,
                                               is_signed }) };
    EXPECT_TRUE(value.has_value()) << (value ? "" : value.error());
    return value.value_or(0);
  }
}

TEST(NumberFormat, ReadsEveryWidthBigEndian)
{
  auto const memory{ Probe() };
  EXPECT_EQ(Read(memory, 0u, WIDTH_BYTE, Endianness::BIG, false), 0x12);
  EXPECT_EQ(Read(memory, 0u, WIDTH_WORD, Endianness::BIG, false), 0x1234);
  EXPECT_EQ(Read(memory, 0u, WIDTH_LONG, Endianness::BIG, false), 0x12345678);
}

TEST(NumberFormat, ReadsEveryWidthLittleEndian)
{
  auto const memory{ Probe() };
  EXPECT_EQ(Read(memory, 0u, WIDTH_BYTE, Endianness::LITTLE, false), 0x12);
  EXPECT_EQ(Read(memory, 0u, WIDTH_WORD, Endianness::LITTLE, false), 0x3412);
  EXPECT_EQ(Read(memory, 0u, WIDTH_LONG, Endianness::LITTLE, false),
            0x78563412);
}

TEST(NumberFormat, ReadsAByteSwappedLongword)
{
  SyntheticMemory memory;
  // 200,000 as a 68000 longword in byte-swapped work ram, and -200,000.
  memory.Poke(0u, { 0x03, 0x00, 0x40, 0x0D });
  memory.Poke(4u, { 0xFC, 0xFF, 0xC0, 0xF2 });

  EXPECT_EQ(Read(memory, 0u, WIDTH_LONG, Endianness::SWAPPED, false), 200000);
  EXPECT_EQ(Read(memory, 4u, WIDTH_LONG, Endianness::SWAPPED, true), -200000);
  EXPECT_EQ(Read(memory, 4u, WIDTH_LONG, Endianness::SWAPPED, false),
            0xFFFCF2C0);
}

TEST(NumberFormat, ASwappedWordIsLittleAndASwappedByteIsEither)
{
  auto const memory{ Probe() };
  EXPECT_EQ(Read(memory, 0u, WIDTH_WORD, Endianness::SWAPPED, false),
            Read(memory, 0u, WIDTH_WORD, Endianness::LITTLE, false));
  EXPECT_EQ(Read(memory, 8u, WIDTH_WORD, Endianness::SWAPPED, true), -257);
  EXPECT_EQ(Read(memory, 0u, WIDTH_BYTE, Endianness::SWAPPED, false), 0x12);
  EXPECT_EQ(Read(memory, 8u, WIDTH_BYTE, Endianness::SWAPPED, true), -1);
}

TEST(NumberFormat, TheTopBitIsASignOnlyWhenAskedFor)
{
  auto const memory{ Probe() };
  EXPECT_EQ(Read(memory, 8u, WIDTH_WORD, Endianness::BIG, false), 0xFFFE);
  EXPECT_EQ(Read(memory, 8u, WIDTH_WORD, Endianness::BIG, true), -2);
  EXPECT_EQ(Read(memory, 8u, WIDTH_BYTE, Endianness::BIG, true), -1);
  EXPECT_EQ(Read(memory, 8u, WIDTH_LONG, Endianness::BIG, false),
            0xFFFE0000);
  EXPECT_EQ(Read(memory, 8u, WIDTH_LONG, Endianness::BIG, true), -131072);
}

TEST(NumberFormat, RefusesAWidthNoMachineHas)
{
  auto const memory{ Probe() };
  EXPECT_FALSE(ReadNumber(memory.View(), 0u, NumberFormat{ 3u }).has_value());
  EXPECT_FALSE(WidthChecked(0u).has_value());
  EXPECT_EQ(WidthChecked(WIDTH_LONG), WIDTH_LONG);
}

TEST(NumberFormat, RefusesAnAddressTheRegionEndsBefore)
{
  auto const memory{ Probe() };
  auto const past{ static_cast<std::uint32_t>(AREA_BYTES) - 1u };
  auto const read{ ReadNumber(memory.View(), past, NumberFormat{}) };
  ASSERT_FALSE(read.has_value());
  EXPECT_NE(read.error().find("past"), std::string::npos);
}

TEST(NumberFormat, EndiannessIsSpeltOutOrRefused)
{
  EXPECT_EQ(EndiannessOf("big"), Endianness::BIG);
  EXPECT_EQ(EndiannessOf("little"), Endianness::LITTLE);
  EXPECT_EQ(EndiannessOf("swapped"), Endianness::SWAPPED);
  EXPECT_FALSE(EndiannessOf("middle").has_value());
  EXPECT_EQ(NameOf(Endianness::BIG), "big");
  EXPECT_EQ(NameOf(Endianness::LITTLE), "little");
  EXPECT_EQ(NameOf(Endianness::SWAPPED), "swapped");
}

TEST(WatchSpec, AnAddressReadsInHexOrDecimal)
{
  EXPECT_EQ(AddressOf("0xff00e2"), 0xFF00E2u);
  EXPECT_EQ(AddressOf("255"), 255u);
  EXPECT_FALSE(AddressOf("0xzz").has_value());
  EXPECT_FALSE(AddressOf("").has_value());
}

TEST(WatchSpec, ASpecBecomesAWatchOrSaysWhyNot)
{
  WatchSpec spec;
  spec.name = "score";
  spec.address = "0x10";
  spec.width = WIDTH_LONG;
  spec.endian = "little";
  spec.is_signed = true;

  auto const made{ WatchFrom(spec) };
  ASSERT_TRUE(made.has_value()) << (made ? "" : made.error());
  EXPECT_EQ(made->name, "score");
  EXPECT_EQ(made->region, SYSTEM_RAM);
  EXPECT_EQ(made->address, 0x10u);
  EXPECT_EQ(made->format,
            (NumberFormat{ WIDTH_LONG, Endianness::LITTLE, true }));

  spec.endian = "sideways";
  EXPECT_FALSE(WatchFrom(spec).has_value());
}

TEST(Watch, ReadsItselfOutOfARegion)
{
  auto const memory{ Probe() };
  Watch const watch{ "word", std::string{ SYSTEM_RAM }, 0u,
                     NumberFormat{ WIDTH_WORD, Endianness::BIG, false } };
  EXPECT_EQ(ReadWatch(watch, memory.View()), 0x1234);
}
