#include "tash/libretro/core.hpp"
#include "tash/libretro/shared-library.hpp"
#include "tash/utilities/executable-directory.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <stdexcept>

namespace
{
  using tash::libretro::Core;
  using tash::libretro::CoreDirectories;
  using tash::libretro::SharedLibrary;

  auto CorePath() -> std::filesystem::path
  {
    auto const beside{ tash::utilities::ExecutableDirectory() };
    EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
    return *beside / "genesis_plus_gx_libretro.so";
  }

  auto Loaded() -> std::unique_ptr<Core>
  {
    auto core{ Core::Load(CorePath(), CoreDirectories{ ".", "." }) };
    EXPECT_TRUE(core.has_value()) << (core ? "" : core.error());
    return core ? std::move(*core) : nullptr;
  }
}

TEST(SharedLibraryTest, ALibraryThatIsNotThereIsAnAnswer)
{
  auto const opened{ SharedLibrary::Open("no_such_libretro.so") };
  ASSERT_FALSE(opened.has_value());
  EXPECT_NE(opened.error().find("no_such_libretro.so"), std::string::npos);
}

TEST(SharedLibraryTest, ASymbolThatIsNotThereIsAnAnswer)
{
  auto const opened{ SharedLibrary::Open(CorePath()) };
  ASSERT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
  EXPECT_TRUE(opened->Symbol("retro_run").has_value());
  EXPECT_FALSE(opened->Symbol("retro_fly").has_value());
}

TEST(CoreTest, TheCoreAnnouncesItself)
{
  auto const core{ Loaded() };
  ASSERT_NE(core, nullptr);
  EXPECT_EQ(core->Information().name, "Genesis Plus GX");
  EXPECT_FALSE(core->Information().version.empty());
  EXPECT_NE(core->Information().extensions.find("md"), std::string::npos);
  EXPECT_EQ(core->ApiVersion(), 1u);
}

TEST(CoreTest, ASecondLiveCoreIsRefusedByTheOneStaticSlot)
{
  auto const core{ Loaded() };
  ASSERT_NE(core, nullptr);
  EXPECT_THROW(
    {
      auto const second{ Core::Load(CorePath(), CoreDirectories{ ".", "." }) };
    },
    std::logic_error);
}

TEST(CoreTest, WhatTheCoreWasRefusedIsCountedByCommand)
{
  auto const core{ Loaded() };
  ASSERT_NE(core, nullptr);
  auto const refused{ core->Refusals() };
  EXPECT_FALSE(refused.empty());
  for (tash::libretro::Refusal const& one : refused)
    EXPECT_GT(one.count, 0u);
}

TEST(CoreTest, ThePixelFormatIsTheOneTheBusCarries)
{
  auto const core{ Loaded() };
  ASSERT_NE(core, nullptr);
  ASSERT_TRUE(core->LoadGame(std::filesystem::current_path()
                             / "../../../examples/homebrew/zsenilia.bin")
                .has_value());
  EXPECT_EQ(core->PixelFormat(), RETRO_PIXEL_FORMAT_RGB565);
  EXPECT_GT(core->StateSize(), 0u);
  EXPECT_FALSE(core->Memory(RETRO_MEMORY_SYSTEM_RAM).empty());
}
