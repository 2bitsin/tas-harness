#include "tash/tash/run-command.hpp"

#include "tash/python/checkpoint-store.hpp"
#include "tash/tash/profile.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <oxbox/serialization/io.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
  using tash::cli::RunCommand;

  constexpr std::uint64_t RUN_FRAMES{ 4 };
  constexpr std::size_t PADDING_BYTES{ 1024 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  // The homebrew profile with an absolute rom, so the run resolves it from
  // wherever ctest runs the test from.
  auto ProfileFor(std::filesystem::path const& where,
                  std::filesystem::path const& rom) -> std::filesystem::path
  {
    auto profile{ tash::cli::ProfileFrom(Root()
                                         / "examples/homebrew/profile.yaml") };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return { };
    profile->rom = (rom.empty() ? Root() / profile->rom : rom).string();
    std::filesystem::create_directories(where);
    std::filesystem::path const path{ where / "profile.yaml" };
    oxbox::serialization::SerializeTo(*profile, path);
    return path;
  }

  auto ProfileBeside(std::filesystem::path const& where)
    -> std::filesystem::path
  {
    return ProfileFor(where, { });
  }

  auto RomPath() -> std::filesystem::path
  {
    auto const profile{ tash::cli::ProfileFrom(Root()
                                         / "examples/homebrew/profile.yaml") };
    return profile ? Root() / profile->rom : std::filesystem::path{ };
  }

  // The one directory the store's key named under the checkpoints root.
  auto KeyUnder(std::filesystem::path const& bundles) -> std::string
  {
    std::filesystem::path const root{ bundles
                                      / tash::python::CHECKPOINT_ROOT };
    if (!std::filesystem::is_directory(root))
      return { };
    for (auto const& keyed : std::filesystem::directory_iterator{ root })
      if (keyed.is_directory())
        return keyed.path().filename().string();
    return { };
  }

  auto StateUnder(std::filesystem::path const& root) -> std::filesystem::path
  {
    if (!std::filesystem::exists(root))
      return { };
    for (auto const& kept :
         std::filesystem::recursive_directory_iterator{ root })
      if (kept.path().extension() == ".state")
        return kept.path();
    return { };
  }

  auto Wrote(std::filesystem::path const& file, std::string const& text)
    -> void
  {
    std::ofstream writing{ file };
    writing << text;
  }

  auto ShotIn(std::filesystem::path const& bundles)
    -> std::filesystem::path
  {
    for (auto const& run : std::filesystem::directory_iterator{ bundles })
      for (auto const& shot :
           std::filesystem::directory_iterator{ run.path() / "shots" })
        return shot.path();
    return { };
  }
}

TEST(RunCommand, WritesTheShotAtThePathItWasGiven)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("run-shot") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const shot{ scratch->File("last.png") };

  RunCommand run;
  run.profile = ProfileBeside(scratch->Path()).string();
  run.frames = RUN_FRAMES;
  run.shot = shot.string();
  ASSERT_EQ(run().Code(), 0);

  EXPECT_TRUE(std::filesystem::exists(shot)) << shot.string();
}

TEST(RunCommand, WritesThatPathWithABundleOpenAndTheBundlesOwnShotToo)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("run-shot-bundle") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const shot{ scratch->File("asked.png") };
  auto const bundles{ scratch->Path() / "runs" };

  RunCommand run;
  run.profile = ProfileBeside(scratch->Path()).string();
  run.frames = RUN_FRAMES;
  run.shot = shot.string();
  run.bundle = bundles.string();
  ASSERT_EQ(run().Code(), 0);

  EXPECT_TRUE(std::filesystem::exists(shot)) << shot.string();
  auto const kept{ ShotIn(bundles) };
  EXPECT_TRUE(std::filesystem::exists(kept)) << kept.string();
  EXPECT_EQ(kept.extension(), ".png");
}

TEST(RunCommand, TheCheckpointsKeyIsTheContentsBytesAndNotItsPath)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("run-content-key") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const scenario{ scratch->File("taking.py") };
  Wrote(scenario, "import tash\ntash.run.checkpoint('crossing')\n");

  std::filesystem::path const rom{ RomPath() };
  ASSERT_TRUE(std::filesystem::is_regular_file(rom)) << rom.string();
  std::filesystem::path const same{ scratch->Path() / "same.bin" };
  std::filesystem::path const other{ scratch->Path() / "other.bin" };
  std::filesystem::copy_file(rom, same);
  std::filesystem::copy_file(rom, other);

  // Trailing bytes no Genesis rom reads: the content differs, the run does
  // not, and the store has to tell the two apart all the same.
  {
    std::ofstream padding{ other, std::ios::binary | std::ios::app };
    std::vector<char> const zeroes(PADDING_BYTES, '\0');
    padding.write(zeroes.data(), static_cast<std::streamsize>(zeroes.size()));
  }

  std::vector<std::string> keys;
  for (auto const& [named, content] :
       { std::pair{ "first", rom }, std::pair{ "copy", same },
         std::pair{ "padded", other } })
  {
    auto const bundles{ scratch->Path() / named / "runs" };
    RunCommand run;
    run.profile = ProfileFor(scratch->Path() / named, content).string();
    run.frames = RUN_FRAMES;
    run.scenario = scenario.string();
    run.bundle = bundles.string();
    ASSERT_EQ(run().Code(), 0) << named;
    keys.push_back(KeyUnder(bundles));
    EXPECT_FALSE(keys.back().empty()) << named;
  }

  EXPECT_EQ(keys[0], keys[1]) << "the same bytes at two paths are one key";
  EXPECT_NE(keys[0], keys[2]) << "different bytes are a key of their own";
}

TEST(RunCommand, CachesAScenarioCheckpointBesideTheBundlesByDefault)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("run-checkpoints") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const bundles{ scratch->Path() / "runs" };
  auto const scenario{ scratch->File("taking.py") };
  Wrote(scenario, "import tash\ntash.run.checkpoint('crossing')\n");

  RunCommand run;
  run.profile = ProfileBeside(scratch->Path()).string();
  run.frames = RUN_FRAMES;
  run.scenario = scenario.string();
  run.bundle = bundles.string();
  ASSERT_EQ(run().Code(), 0);

  auto const kept{ StateUnder(bundles / tash::python::CHECKPOINT_ROOT) };
  EXPECT_EQ(kept.filename(), "crossing.state") << kept.string();
  EXPECT_FALSE(std::filesystem::exists(bundles / "_checkpoints"
                                       / "crossing.state"))
    << "the rom hash names the directory the state sits in";
}
