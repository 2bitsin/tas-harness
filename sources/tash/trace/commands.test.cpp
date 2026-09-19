#include "tash/trace/commands.hpp"

#include "tash/trace/_scratch-path.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace tash::trace
{
  namespace
  {
    using detail::scratch_path::ScratchPath;

    constexpr std::int64_t CREATED{ 1'789'300'800'123'456'789 };

    auto WriteSample(std::filesystem::path const& path) -> void
    {
      auto writing{ Writer::Open(
        path, Header{ FORMAT_VERSION, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{ 1, 0, 1, 2, 3, 0.0 }));
      EXPECT_TRUE(writing->Write(MarkRecord{ 1, "power on" }));
    }

    auto Drive(std::span<std::string_view const> arguments) -> int
    {
      return RunTraceCommands(arguments).Code();
    }
  }

  TEST(TraceCommands, DumpPrintsOneLinePerRecord)
  {
    ScratchPath const area{ };
    auto const path{ area.File("run.bin") };
    WriteSample(path);
    auto const spelled{ path.string() };

    std::array<std::string_view, 2> const line{ "dump", spelled };
    testing::internal::CaptureStdout();
    auto const code{ Drive(line) };
    auto const printed{ testing::internal::GetCapturedStdout() };

    EXPECT_EQ(code, 0);
    EXPECT_EQ(printed,
              "# tash trace v5 created=2026-09-13T12:00:00.123456789Z "
              "producer=\"tash 0.1\"\n"
              "         1 frame    time=0.000000 exact=0000000000000001 "
              "dhash=0000000000000002 phash=0000000000000003 "
              "change=0.000000\n"
              "         1 mark     text=\"power on\"\n"
              "# 2 records, 0 skipped\n");
  }

  TEST(TraceCommands, DumpFailsOnAFileThatIsNotATrace)
  {
    ScratchPath const area{ };
    auto const spelled{ area.File("absent.bin").string() };
    std::array<std::string_view, 2> const line{ "dump", spelled };
    EXPECT_NE(Drive(line), 0);
  }

  TEST(TraceCommands, SqliteWritesTheDatabase)
  {
    ScratchPath const area{ };
    auto const path{ area.File("run.bin") };
    auto const database{ area.File("run.db") };
    WriteSample(path);
    auto const spelled{ path.string() };
    auto const spelled_database{ database.string() };

    std::array<std::string_view, 3> const line{ "sqlite", spelled,
                                                spelled_database };
    testing::internal::CaptureStdout();
    auto const code{ Drive(line) };
    auto const printed{ testing::internal::GetCapturedStdout() };

    EXPECT_EQ(code, 0);
    EXPECT_EQ(printed, std::format("2 rows into {}\n", spelled_database));
    EXPECT_TRUE(std::filesystem::exists(database));
  }

  TEST(TraceCommands, TheBareTreeShowsItsSubcommands)
  {
    std::array<std::string_view, 0> const line{ };
    testing::internal::CaptureStdout();
    auto const code{ Drive(line) };
    auto const printed{ testing::internal::GetCapturedStdout() };

    EXPECT_NE(code, 0);
    EXPECT_NE(printed.find("dump"), std::string::npos) << printed;
    EXPECT_NE(printed.find("sqlite"), std::string::npos) << printed;
  }
}
