#include "tash/tash/core-path.hpp"
#include "tash/utilities/executable-directory.hpp"

#include <gtest/gtest.h>

#include <filesystem>

TEST(ResolvedCoreTest, ABareNameIsTheCoreBesideTheBinary)
{
  auto const found{ tash::cli::ResolvedCore("genesis_plus_gx") };
  ASSERT_TRUE(found.has_value()) << (found ? "" : found.error());
  EXPECT_EQ(found->filename(), "genesis_plus_gx_libretro.so");
  EXPECT_TRUE(std::filesystem::is_regular_file(*found));
}

TEST(ResolvedCoreTest, APathIsTakenAsThePathItIs)
{
  auto const beside{ tash::utilities::ExecutableDirectory() };
  ASSERT_TRUE(beside.has_value());
  auto const found{ tash::cli::ResolvedCore(
    (*beside / "genesis_plus_gx_libretro.so").string()) };
  ASSERT_TRUE(found.has_value()) << (found ? "" : found.error());
  EXPECT_EQ(found->filename(), "genesis_plus_gx_libretro.so");
}

TEST(ResolvedCoreTest, ACoreThatIsNotThereNamesWhatWasTried)
{
  auto const found{ tash::cli::ResolvedCore("no_such_core") };
  ASSERT_FALSE(found.has_value());
  EXPECT_NE(found.error().find("no_such_core_libretro.so"),
            std::string::npos);
  EXPECT_FALSE(tash::cli::ResolvedCore("").has_value());
}
