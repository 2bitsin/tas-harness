#include "tash/libretro/libretro.h"
#include "tash/tash/profile.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
  auto Written(std::filesystem::path const& to, std::string_view yaml)
    -> std::filesystem::path
  {
    std::ofstream file{ to };
    file << yaml;
    return to;
  }
}

TEST(ProfileTest, TheColumnsExampleIsAProfile)
{
  auto const profile{
    tash::cli::ProfileFrom("../../../examples/columns/profile.yaml") };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
  EXPECT_EQ(profile->name, "columns");
  EXPECT_EQ(profile->core, "genesis_plus_gx");
  EXPECT_NE(profile->rom.find("Columns"), std::string::npos);
  ASSERT_EQ(profile->ports.size(), 2u);
  EXPECT_EQ(profile->ports[0], "joypad");
}

TEST(ProfileTest, AProfileNeedNotNameItself)
{
  auto const path{ Written(std::filesystem::temp_directory_path()
                             / "tash-nameless-profile.yaml",
                           "core: genesis_plus_gx\nrom: a.bin\n"
                           "system_dir: \"\"\nsave_dir: \"\"\n"
                           "ports:\n  - joypad\n") };
  auto const profile{ tash::cli::ProfileFrom(path) };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
  EXPECT_FALSE(profile->name.has_value());
  std::filesystem::remove(path);
}

TEST(ProfileTest, AProfileThatIsNotThereIsAnAnswer)
{
  auto const profile{ tash::cli::ProfileFrom("no-such-profile.yaml") };
  ASSERT_FALSE(profile.has_value());
  EXPECT_NE(profile.error().find("no-such-profile.yaml"), std::string::npos);
}

TEST(ProfileTest, AProfileThatIsNotAProfileNamesTheFile)
{
  auto const path{ Written(std::filesystem::temp_directory_path()
                           / "tash-not-a-profile.yaml", "[: not yaml\n") };
  auto const profile{ tash::cli::ProfileFrom(path) };
  EXPECT_FALSE(profile.has_value());
  std::filesystem::remove(path);
}

TEST(ProfileTest, AProfilesOwnDirectoryIsAProfile)
{
  auto const named{ tash::cli::ProfileFrom("../../../examples/columns") };
  ASSERT_TRUE(named.has_value()) << (named ? "" : named.error());
  EXPECT_EQ(named->name, "columns");

  auto const file{ tash::cli::ProfileFileAt("../../../examples/columns") };
  ASSERT_TRUE(file.has_value()) << (file ? "" : file.error());
  EXPECT_EQ(file->filename(), "profile.yaml");
}

TEST(ProfileTest, ADirectoryWithNoProfileInItNamesWhatIsMissing)
{
  auto const none{ tash::cli::ProfileFrom("../../../examples") };
  ASSERT_FALSE(none.has_value());
  EXPECT_NE(none.error().find("profile.yaml"), std::string::npos)
    << none.error();
}

TEST(DeviceOfTest, OnlyTheDevicesThisStepDrivesAreNamed)
{
  EXPECT_EQ(tash::cli::DeviceOf("joypad"), RETRO_DEVICE_JOYPAD);
  EXPECT_EQ(tash::cli::DeviceOf("none"), RETRO_DEVICE_NONE);
  EXPECT_FALSE(tash::cli::DeviceOf("mouse").has_value());
}

TEST(ProfileTest, TheColumnsExampleCarriesItsWatches)
{
  auto const profile{
    tash::cli::ProfileFrom("../../../examples/columns/profile.yaml") };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
  ASSERT_TRUE(profile->watches.has_value());

  auto const watches{ tash::cli::WatchesFrom(*profile) };
  ASSERT_TRUE(watches.has_value()) << (watches ? "" : watches.error());
  EXPECT_EQ(watches->Count(), profile->watches->size());

  auto const score{ watches->IndexOf("score") };
  ASSERT_TRUE(score.has_value()) << (score ? "" : score.error());
  auto const watch{ watches->At(*score) };
  ASSERT_TRUE(watch.has_value());
  EXPECT_EQ(watch->address, 0xc81eu);
  EXPECT_EQ(watch->format.width, 2u);
  EXPECT_EQ(watch->format.endianness, tash::watches::Endianness::LITTLE);
}

TEST(ProfileTest, AProfileWithNoWatchesKeyStillReads)
{
  auto const path{ Written(std::filesystem::temp_directory_path()
                           / "tash-watchless-profile.yaml",
                           "core: c\nrom: r\nsystem_dir: \"\"\n"
                           "save_dir: \"\"\nports: [joypad]\n") };
  auto const profile{ tash::cli::ProfileFrom(path) };
  ASSERT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
  EXPECT_FALSE(profile->watches.has_value());

  auto const watches{ tash::cli::WatchesFrom(*profile) };
  ASSERT_TRUE(watches.has_value()) << (watches ? "" : watches.error());
  EXPECT_TRUE(watches->Empty());
  std::filesystem::remove(path);
}
