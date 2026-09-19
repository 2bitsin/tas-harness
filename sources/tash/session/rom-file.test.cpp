#include "tash/session/rom-file.hpp"

#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using tash::session::CartridgePresent;
  using tash::session::PreparedRom;

  auto Area() -> oxbox::platform::ScratchArea
  {
    auto scratch{ tash::utilities::ScratchAreaOf("rom-file-test") };
    EXPECT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    return std::move(*scratch);
  }

  auto Written(std::filesystem::path const& file, std::string_view content)
    -> std::filesystem::path
  {
    std::ofstream out{ file, std::ios::binary };
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file;
  }
}

TEST(RomFile, AZeroByteCartridgeIsAbsent)
{
  auto const scratch{ Area() };

  EXPECT_FALSE(CartridgePresent(Written(scratch.File("empty.zip"), "")));
}

TEST(RomFile, ACartridgeWithBytesIsPresent)
{
  auto const scratch{ Area() };

  EXPECT_TRUE(CartridgePresent(Written(scratch.File("game.md"), "SEGA")));
}

TEST(RomFile, AMissingCartridgeIsAbsent)
{
  auto const scratch{ Area() };

  EXPECT_FALSE(CartridgePresent(scratch.File("absent.zip")));
}

TEST(RomFile, APlaceholderIsRefusedAsAPlaceholderAndNotAsABrokenZip)
{
  auto const scratch{ Area() };

  auto const prepared{ PreparedRom(Written(scratch.File("empty.zip"), ""),
                                   scratch.Path()) };

  ASSERT_FALSE(prepared.has_value());
  EXPECT_NE(prepared.error().find("drop the real cartridge over it"),
            std::string::npos)
    << prepared.error();
}

TEST(RomFile, AFileThatIsNotThereIsRefusedAsMissing)
{
  auto const scratch{ Area() };

  auto const prepared{ PreparedRom(scratch.File("absent.zip"),
                                   scratch.Path()) };

  ASSERT_FALSE(prepared.has_value());
  EXPECT_NE(prepared.error().find("no ROM at"), std::string::npos)
    << prepared.error();
}
