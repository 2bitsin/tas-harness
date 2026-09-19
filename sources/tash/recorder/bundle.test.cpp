#include "tash/recorder/bundle.hpp"

#include "tash/recorder/verdicts-writer.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <oxbox/serialization/io.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

namespace tash::recorder
{
  namespace
  {
    // 2026-09-13T12:00:00Z, so the directory name is a golden string.
    constexpr std::chrono::sys_seconds STARTED{
      std::chrono::seconds{ 1'789'300'800 } };

    [[nodiscard]] auto Sample() -> RunManifest
    {
      return RunManifest{ "0.1.0",   "Genesis Plus GX", "1.7.4",
                          "/roms/columns.zip",          "/tas/columns.yaml",
                          600u,      59.9227,           "stepped",
                          "the first column landed" };
    }

    [[nodiscard]] auto LinesOf(std::filesystem::path const& path)
      -> std::vector<std::string>
    {
      std::vector<std::string> lines{ };
      std::ifstream file{ path };
      for (std::string line; std::getline(file, line); )
        lines.push_back(line);
      return lines;
    }
  }

  TEST(RecorderBundle, TheDirectoryIsNamedForTheMomentTheRunStarted)
  {
    EXPECT_EQ(Bundle::DirectoryName("columns", STARTED),
              "2026-09-13T12-00-00Z-columns");
  }

  TEST(RecorderBundle, EveryPathTheRunNeedsIsThere)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-bundle") };
    ASSERT_TRUE(area.has_value()) << area.error();

    auto const bundle{ Bundle::Create(area->Path(), "columns", STARTED) };
    ASSERT_TRUE(bundle.has_value()) << bundle.error();

    EXPECT_EQ(bundle->Root().filename().string(),
              "2026-09-13T12-00-00Z-columns");
    EXPECT_TRUE(std::filesystem::is_directory(bundle->Root()));
    EXPECT_TRUE(std::filesystem::is_directory(bundle->Shots()));
    EXPECT_TRUE(std::filesystem::is_directory(bundle->Clips()));
    EXPECT_EQ(bundle->Video().filename().string(), VIDEO_NAME);
    EXPECT_EQ(bundle->Trace().filename().string(), TRACE_NAME);
    EXPECT_EQ(bundle->Verdicts().filename().string(), VERDICTS_NAME);
    EXPECT_EQ(bundle->Manifest().filename().string(), MANIFEST_NAME);
  }

  TEST(RecorderBundle, TheManifestRoundTripsThroughYaml)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-manifest") };
    ASSERT_TRUE(area.has_value()) << area.error();

    auto const bundle{ Bundle::Create(area->Path(), "columns", STARTED) };
    ASSERT_TRUE(bundle.has_value()) << bundle.error();

    RunManifest const written{ Sample() };
    ASSERT_TRUE(bundle->Write(written).has_value());
    ASSERT_TRUE(std::filesystem::is_regular_file(bundle->Manifest()));

    auto const read{ bundle->Read() };
    ASSERT_TRUE(read.has_value()) << read.error();
    EXPECT_EQ(read->harness_version, written.harness_version);
    EXPECT_EQ(read->core_name, written.core_name);
    EXPECT_EQ(read->core_version, written.core_version);
    EXPECT_EQ(read->rom, written.rom);
    EXPECT_EQ(read->profile, written.profile);
    EXPECT_EQ(read->frames, written.frames);
    EXPECT_DOUBLE_EQ(read->fps, written.fps);
    EXPECT_EQ(read->determinism, written.determinism);
    EXPECT_EQ(read->outcome, written.outcome);
  }

  TEST(RecorderBundle, ANameWithASlashIsRefused)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-slash") };
    ASSERT_TRUE(area.has_value()) << area.error();

    auto const bundle{ Bundle::Create(area->Path(), "a/b", STARTED) };
    ASSERT_FALSE(bundle.has_value());
    EXPECT_NE(bundle.error().find("one path component"), std::string::npos)
      << bundle.error();
  }

  TEST(RecorderBundle, EachVerdictIsOneJsonLine)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-verdicts") };
    ASSERT_TRUE(area.has_value()) << area.error();

    auto const bundle{ Bundle::Create(area->Path(), "columns", STARTED) };
    ASSERT_TRUE(bundle.has_value()) << bundle.error();

    {
      auto writer{ VerdictsWriter::Open(bundle->Verdicts()) };
      ASSERT_TRUE(writer.has_value()) << writer.error();
      ASSERT_TRUE(writer->Append(
        trace::VerdictRecord{ 120u, "level-reached", true,
                              "the level watch says 2" }).has_value());
      ASSERT_TRUE(writer->Append(
        trace::VerdictRecord{ 600u, "no-crash", false,
                              "the frame stopped changing" }).has_value());
      EXPECT_EQ(writer->Written(), 2u);
    }

    std::vector<std::string> const lines{ LinesOf(bundle->Verdicts()) };
    ASSERT_EQ(lines.size(), 2u);

    auto const first{ oxbox::serialization::FromJson<VerdictLine>(lines[0]) };
    EXPECT_EQ(first, (VerdictLine{ 120u, "level-reached", true,
                                   "the level watch says 2" }));

    auto const second{ oxbox::serialization::FromJson<VerdictLine>(lines[1]) };
    EXPECT_EQ(second, (VerdictLine{ 600u, "no-crash", false,
                                    "the frame stopped changing" }));
  }
}
