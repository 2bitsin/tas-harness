#include "tash/watches/sampler.hpp"

#include "tash/session/session.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"
#include "tash/watches/memory-search.hpp"
#include "tash/watches/search-driver.hpp"
#include "tash/watches/search-script.hpp"
#include "tash/watches/watch-spec.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <type_traits>
#include <vector>

namespace
{
  using namespace tash::watches;
  using tash::session::Session;
  using tash::session::SessionSettings;

  constexpr std::uint64_t RUN_FRAMES{ 120 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Opened() -> std::unique_ptr<Session>
  {
    auto const beside{ tash::utilities::ExecutableDirectory() };
    EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
    SessionSettings settings;
    settings.core = *beside / "genesis_plus_gx_libretro.so";
    settings.rom = Root() / "examples/homebrew/zsenilia.bin";
    auto session{ Session::Open(std::move(settings)) };
    EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
    return session ? std::move(*session) : nullptr;
  }

  auto OneWatch() -> WatchSet
  {
    std::vector<WatchSpec> specs(1);
    specs[0].name = "first-word";
    specs[0].address = "0x0";
    auto const set{ WatchSet::From(specs) };
    EXPECT_TRUE(set.has_value()) << (set ? "" : set.error());
    return set.value_or(WatchSet{});
  }
}

TEST(SessionSampler, AWatchFollowsTheHomebrewRunWithoutRefusing)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("watches-session") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const path{ scratch->File("homebrew.tash") };

  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);
  MemoryMap const memory{ MemoryMap::Of(*session) };
  ASSERT_FALSE(memory.Empty());
  EXPECT_TRUE(memory.Area("system").has_value());

  std::uint64_t written{ 0 };
  {
    auto writer{ tash::trace::Writer::Open(path, "tash-test") };
    ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());
    auto const sampler{ Sampler::Open(OneWatch(), memory, &*writer) };
    ASSERT_TRUE(sampler.has_value()) << (sampler ? "" : sampler.error());
    session->Observe(**sampler);

    session->Step(RUN_FRAMES);
    EXPECT_EQ(session->Frames(), RUN_FRAMES);
    EXPECT_EQ((*sampler)->Counts().refused, 0u);
    EXPECT_TRUE((*sampler)->Value("first-word").has_value());
    written = (*sampler)->Counts().written;
    EXPECT_GE(written, 1u);
    ASSERT_TRUE(writer->Flush().has_value());
  }

  std::uint64_t records{ 0 };
  auto reader{ tash::trace::Reader::Open(path) };
  ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
  reader->ForEach([&records](auto const& record) {
    if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                 tash::trace::WatchRecord>)
    {
      EXPECT_EQ(record.watch, 0u);
      ++records;
    }
  });
  EXPECT_EQ(records, written);
}

TEST(SessionSampler, AHuntNarrowsTheHomebrewRamAndPressesAButton)
{
  auto const session{ Opened() };
  ASSERT_NE(session, nullptr);

  MemorySearch search{ MemoryMap::Of(*session), NumberFormat{} };
  auto const steps{ StepsFrom(
    "run 60; snapshot; hold start; run 4; release; run 60; changed; "
    "list 3") };
  ASSERT_TRUE(steps.has_value()) << (steps ? "" : steps.error());

  std::ostringstream report;
  SearchDriver driver{ *session, search };
  ASSERT_TRUE(driver.Play(*steps, report).has_value());

  EXPECT_EQ(session->Frames(), 124u);
  EXPECT_TRUE(search.Seeded());
  EXPECT_GT(search.Count(), 0u);
  EXPECT_LT(search.Count(), 1000u);
  EXPECT_NE(report.str().find("snapshot"), std::string::npos);
  EXPECT_NE(report.str().find("system"), std::string::npos);
}
