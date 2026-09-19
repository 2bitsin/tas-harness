#include "tash/session/cartridge.hpp"

#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <utility>

namespace
{
  using tash::session::Cartridge;

  auto Written(std::filesystem::path const& file, std::string const& content)
    -> std::filesystem::path
  {
    std::ofstream out{ file, std::ios::binary };
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file;
  }

  auto Area() -> oxbox::platform::ScratchArea
  {
    auto scratch{ tash::utilities::ScratchAreaOf("cartridge-test") };
    EXPECT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    return std::move(*scratch);
  }
}

TEST(Cartridge, ReadsTheFileByteForByteInItsOwnOrder)
{
  auto const scratch{ Area() };
  std::string const content{ "SEGA GENESIS\x01\x02\xfe\xff" };

  auto const held{ Cartridge::Of(
    Written(scratch.File("rom.bin"), content)) };

  ASSERT_TRUE(held.has_value()) << (held ? "" : held.error());
  ASSERT_EQ(held->Bytes().size(), content.size());
  EXPECT_TRUE(std::ranges::equal(
    held->Bytes(), std::as_bytes(std::span{ content })));
}

TEST(Cartridge, AnEmptyFileIsAnEmptyCartridge)
{
  auto const scratch{ Area() };

  auto const held{ Cartridge::Of(Written(scratch.File("empty.bin"), "")) };

  ASSERT_TRUE(held.has_value()) << (held ? "" : held.error());
  EXPECT_TRUE(held->Bytes().empty());
}

TEST(Cartridge, RefusesAFileThatIsNotThere)
{
  auto const scratch{ Area() };

  auto const held{ Cartridge::Of(scratch.File("absent.bin")) };

  EXPECT_FALSE(held.has_value());
}
