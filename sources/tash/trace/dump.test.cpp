#include "tash/trace/dump.hpp"

#include "tash/trace/_scratch-path.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>

namespace tash::trace
{
  namespace
  {
    using detail::scratch_path::ScratchPath;

    constexpr std::int64_t CREATED{ 1'789'300'800'123'456'789 };
  }

  TEST(TraceDump, HeaderLineIsStable)
  {
    EXPECT_EQ(FormatHeaderLine(Header{ FORMAT_VERSION, CREATED, "tash 0.1" }),
              "# tash trace v5 created=2026-09-13T12:00:00.123456789Z "
              "producer=\"tash 0.1\"");
  }

  TEST(TraceDump, RecordLinesAreStable)
  {
    EXPECT_EQ(FormatRecordLine(FrameRecord{ 90, 1'500'000'000,
                                            0xdead'beef'0bad'c0deull,
                                            0x0102'0304'0506'0708ull,
                                            0x1122'3344'5566'7788ull, 0.125 }),
              "        90 frame    time=1.500000 exact=deadbeef0badc0de "
              "dhash=0102030405060708 phash=1122334455667788 change=0.125000");

    EXPECT_EQ(FormatRecordLine(InputRecord{ 91, 1, 0x30u }),
              "        91 input    port=1 pad=0x00000030");

    EXPECT_EQ(FormatRecordLine(WatchRecord{ 92, 7, -4242 }),
              "        92 watch    watch=7 value=-4242");

    EXPECT_EQ(FormatRecordLine(DecisionRecord{ 93, "agent", "play the tape" }),
              "        93 decision actor=\"agent\" text=\"play the tape\"");

    EXPECT_EQ(FormatRecordLine(VerdictRecord{ 94, "level", true, "reached" }),
              "        94 verdict  name=\"level\" outcome=pass "
              "text=\"reached\"");

    EXPECT_EQ(FormatRecordLine(VerdictRecord{ 95, "level", false, "stuck" }),
              "        95 verdict  name=\"level\" outcome=fail "
              "text=\"stuck\"");

    EXPECT_EQ(FormatRecordLine(TriggerRecord{ 96, "spike", "over 0.4" }),
              "        96 trigger  name=\"spike\" text=\"over 0.4\"");

    EXPECT_EQ(FormatRecordLine(MarkRecord{ 97, "first column" }),
              "        97 mark     text=\"first column\"");

    EXPECT_EQ(FormatRecordLine(MarkRecord{ 98, "trial 3", "decision" }),
              "        98 mark     text=\"trial 3\" group=\"decision\"");

    EXPECT_EQ(FormatRecordLine(RestoreRecord{ 99, 30, "search-0" }),
              "        99 restore  name=\"search-0\" to line frame 30");

    EXPECT_EQ(FormatRecordLine(ResetRecord{ 100 }),
              "       100 reset    to power on");
  }

  TEST(TraceDump, NegativeHarnessTimeKeepsItsSign)
  {
    EXPECT_EQ(FormatRecordLine(FrameRecord{ 0, -1'500'000, 0, 0, 0, 0.0 }),
              "         0 frame    time=-0.001500 exact=0000000000000000 "
              "dhash=0000000000000000 phash=0000000000000000 "
              "change=0.000000");
  }

  TEST(TraceDump, WholeFileIsStable)
  {
    ScratchPath const area{ };
    auto const path{ area.File("dump.bin") };
    {
      auto writing{ Writer::Open(
        path, Header{ FORMAT_VERSION, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{ 0, 0, 1, 2, 3, 0.0 }));
      EXPECT_TRUE(writing->Write(MarkRecord{ 0, "power on" }));
    }

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    std::ostringstream out{ };
    Dump(*reading, out);

    EXPECT_EQ(out.str(),
              "# tash trace v5 created=2026-09-13T12:00:00.123456789Z "
              "producer=\"tash 0.1\"\n"
              "         0 frame    time=0.000000 exact=0000000000000001 "
              "dhash=0000000000000002 phash=0000000000000003 "
              "change=0.000000\n"
              "         0 mark     text=\"power on\"\n"
              "# 2 records, 0 skipped\n");
  }

  TEST(TraceDump, ARestoreNamesTheLineItsTargetIsCountedOn)
  {
    ScratchPath const area{ };
    auto const path{ area.File("restored.bin") };
    {
      auto writing{ Writer::Open(
        path, Header{ FORMAT_VERSION, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(RestoreRecord{ 400, 30, "here" }));
      EXPECT_TRUE(writing->Write(FrameRecord{ 400, 0, 1, 2, 3, 0.0 }));
    }

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    std::ostringstream out{ };
    Dump(*reading, out);

    // The restore is stamped with the harness frame it was made at and its
    // target counts the line, so it prints ahead of what it restored to.
    EXPECT_NE(out.str().find("       400 restore  name=\"here\" "
                             "to line frame 30\n"
                             "       400 frame    "),
              std::string::npos) << out.str();
  }

  TEST(TraceDump, SaysSoWhenTheTailIsTruncated)
  {
    ScratchPath const area{ };
    auto const path{ area.File("cut.bin") };
    {
      auto writing{ Writer::Open(
        path, Header{ FORMAT_VERSION, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(MarkRecord{ 0, "power on" }));
      EXPECT_TRUE(writing->Write(MarkRecord{ 1, "cut in half" }));
    }
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - 4u);

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    std::ostringstream out{ };
    Dump(*reading, out);

    EXPECT_TRUE(out.str().ends_with(
      "# 1 records, 0 skipped\n"
      "# truncated: the last record is incomplete\n")) << out.str();
  }
}
