#include "tash/trace/reader.hpp"
#include "tash/trace/writer.hpp"

#include "tash/trace/_codec.hpp"
#include "tash/trace/_scratch-path.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tash::trace
{
  using utilities::Result;

  namespace
  {
    using detail::scratch_path::ScratchPath;

    constexpr std::int64_t CREATED{ 1'789'300'800'123'456'789 };

    auto Sample() -> std::vector<Record>
    {
      return {
        FrameRecord{ 1, 16'666'667, 0xdead'beef'0bad'c0deull,
                     0x0102'0304'0506'0708ull, 0x1122'3344'5566'7788ull,
                     0.125 },
        InputRecord{ 2, 1, 0x0000'0030u },
        WatchRecord{ 3, 7, -4242 },
        DecisionRecord{ 4, "agent", "play the tape to the title screen" },
        VerdictRecord{ 5, "level-reached", true, "level watch says 2" },
        TriggerRecord{ 6, "change-spike", "change amount over 0.4" },
        MarkRecord{ 7, "first falling column" },
        MarkRecord{ 8, "trial 3", "decision" },
        RestoreRecord{ 9, 30, "search-0" },
        ResetRecord{ 10 },
      };
    }

    auto Opened(std::filesystem::path const& path,
                std::size_t flush_interval = DEFAULT_FLUSH_INTERVAL)
      -> Result<Writer>
    {
      return Writer::Open(path, Header{ FORMAT_VERSION, CREATED, "tash-test" },
                          flush_interval);
    }

    auto AppendRaw(std::filesystem::path const& path, std::uint8_t kind,
                   std::vector<std::byte> const& payload) -> void
    {
      std::array<std::byte, RECORD_FRAME_SIZE> frame{ };
      oxbox::utilities::BoundedWriter into{ std::span{ frame } };
      detail::codec::Put<std::uint8_t>(into, kind);
      detail::codec::Put<std::uint32_t>(
        into, static_cast<std::uint32_t>(payload.size()));

      std::ofstream file{ path, std::ios::binary | std::ios::app };
      file.write(reinterpret_cast<char const*>(frame.data()),
                 static_cast<std::streamsize>(frame.size()));
      file.write(reinterpret_cast<char const*>(payload.data()),
                 static_cast<std::streamsize>(payload.size()));
    }

    auto MarkPayload(std::uint64_t frame, std::string const& text)
      -> std::vector<std::byte>
    {
      MarkRecord const record{ frame, text };
      std::vector<std::byte> payload(detail::codec::PayloadSize(record),
                                     std::byte{ 0 });
      oxbox::utilities::BoundedWriter into{ std::span{ payload } };
      detail::codec::Encode(into, record);
      return payload;
    }

    // A mark as format version 1 wrote it: the frame and the text, and
    // nothing where the group now goes.
    auto LegacyMarkPayload(std::uint64_t frame, std::string const& text)
      -> std::vector<std::byte>
    {
      std::vector<std::byte> payload(
        8u + detail::codec::TextSize(text), std::byte{ 0 });
      oxbox::utilities::BoundedWriter into{ std::span{ payload } };
      detail::codec::Put<std::uint64_t>(into, frame);
      detail::codec::PutText(into, text);
      return payload;
    }
  }

  TEST(TraceWriterReader, AMarkWrittenBeforeGroupsReadsAsUngrouped)
  {
    ScratchPath const area{ };
    auto const path{ area.File("version-one.bin") };
    {
      auto writing{ Writer::Open(
        path, Header{ 1, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
    }
    AppendRaw(path, std::to_underlying(Kind::MARK),
              LegacyMarkPayload(4, "power on"));

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    auto const held{ reading->Next() };
    ASSERT_TRUE(held.has_value());
    EXPECT_EQ(std::get<MarkRecord>(*held), (MarkRecord{ 4, "power on", "" }));
    EXPECT_FALSE(reading->Truncated());
  }

  TEST(TraceWriterReader, ATraceWrittenBeforeRestoresStillReads)
  {
    ScratchPath const area{ };
    auto const path{ area.File("version-two.bin") };
    {
      auto writing{ Writer::Open(path, Header{ 2, CREATED, "tash 0.2" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(MarkRecord{ 4, "trial 3", "decision" }));
    }

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    EXPECT_EQ(reading->FileHeader().format_version, 2u);
    auto const held{ reading->Next() };
    ASSERT_TRUE(held.has_value());
    EXPECT_EQ(std::get<MarkRecord>(*held),
              (MarkRecord{ 4, "trial 3", "decision" }));
    EXPECT_FALSE(reading->Truncated());
  }

  TEST(TraceWriterReader, RoundTripsEveryKind)
  {
    ScratchPath const area{ };
    auto const path{ area.File("round-trip.bin") };

    {
      auto writing{ Opened(path) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      for (auto const& record : Sample())
        EXPECT_TRUE(writing->Write(record).has_value());
      EXPECT_EQ(writing->Records(), Sample().size());
    }

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    EXPECT_EQ(reading->FileHeader().format_version, FORMAT_VERSION);
    EXPECT_EQ(reading->FileHeader().creation_time, CREATED);
    EXPECT_EQ(reading->FileHeader().producer, "tash-test");

    std::vector<Record> read{ };
    while (auto held = reading->Next())
      read.push_back(*held);

    EXPECT_EQ(read, Sample());
    EXPECT_FALSE(reading->Truncated());
    EXPECT_EQ(reading->Unknown(), 0u);
  }

  TEST(TraceWriterReader, SkipsAnUnknownKind)
  {
    ScratchPath const area{ };
    auto const path{ area.File("unknown.bin") };

    {
      auto writing{ Opened(path) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{ 1, 0, 1, 2, 3, 0.0 }));
    }
    AppendRaw(path, 0x7f, std::vector<std::byte>(9u, std::byte{ 0xab }));
    AppendRaw(path, 7, MarkPayload(2, "after the stranger"));

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();

    auto const first{ reading->Next() };
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(KindOf(*first), Kind::FRAME);

    auto const second{ reading->Next() };
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(KindOf(*second), Kind::MARK);
    EXPECT_EQ(std::get<MarkRecord>(*second).text, "after the stranger");

    EXPECT_FALSE(reading->Next().has_value());
    EXPECT_EQ(reading->Unknown(), 1u);
    EXPECT_EQ(reading->Records(), 2u);
    EXPECT_FALSE(reading->Truncated());
  }

  TEST(TraceWriterReader, ReportsATruncatedPayload)
  {
    ScratchPath const area{ };
    auto const path{ area.File("cut-payload.bin") };

    {
      auto writing{ Opened(path) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{ 1, 0, 1, 2, 3, 0.0 }));
      EXPECT_TRUE(writing->Write(FrameRecord{ 2, 0, 4, 5, 6, 0.0 }));
      EXPECT_TRUE(writing->Write(
        MarkRecord{ 3, "the record the crash cut in half" }));
    }
    std::filesystem::resize_file(path,
                                 std::filesystem::file_size(path) - 5u);

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();

    std::vector<Record> read{ };
    reading->ForEach([&read](auto const& held) { read.push_back(held); });

    EXPECT_EQ(read.size(), 2u);
    EXPECT_EQ(reading->Records(), 2u);
    EXPECT_TRUE(reading->Truncated());
  }

  TEST(TraceWriterReader, ReportsATruncatedRecordHeading)
  {
    ScratchPath const area{ };
    auto const path{ area.File("cut-heading.bin") };

    {
      auto writing{ Opened(path) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{ 1, 0, 1, 2, 3, 0.0 }));
      EXPECT_TRUE(writing->Write(MarkRecord{ 2, "gone" }));
    }
    std::filesystem::resize_file(
      path, std::filesystem::file_size(path)
              - detail::codec::PayloadSize(MarkRecord{ 2, "gone" })
              - (RECORD_FRAME_SIZE - 2u));

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    EXPECT_TRUE(reading->Next().has_value());
    EXPECT_FALSE(reading->Next().has_value());
    EXPECT_TRUE(reading->Truncated());
  }

  TEST(TraceWriterReader, RefusesAFileThatIsNotATrace)
  {
    ScratchPath const area{ };
    auto const path{ area.File("not-a-trace.bin") };
    {
      std::ofstream file{ path, std::ios::binary };
      file << "this is not a trace, it is a sentence of about this length";
    }

    auto const reading{ Reader::Open(path) };
    ASSERT_FALSE(reading.has_value());
    EXPECT_NE(reading.error().find("magic"), std::string::npos)
      << reading.error();
  }

  TEST(TraceWriterReader, RefusesAMissingFile)
  {
    ScratchPath const area{ };
    auto const reading{ Reader::Open(area.File("nothing-here.bin")) };
    ASSERT_FALSE(reading.has_value());
    EXPECT_TRUE(reading.error().starts_with("trace: cannot open"))
      << reading.error();
  }

  TEST(TraceWriterReader, FlushesOnTheDocumentedInterval)
  {
    ScratchPath const area{ };
    auto const path{ area.File("flush.bin") };

    auto writing{ Opened(path, 2u) };
    ASSERT_TRUE(writing.has_value()) << writing.error();
    auto const after_header{ std::filesystem::file_size(path) };

    EXPECT_TRUE(writing->Write(MarkRecord{ 1, "one" }));
    EXPECT_EQ(std::filesystem::file_size(path), after_header);

    EXPECT_TRUE(writing->Write(MarkRecord{ 2, "two" }));
    EXPECT_GT(std::filesystem::file_size(path), after_header);
  }

  TEST(TraceWriterReader, AFailedWriteLatchesItsRefusal)
  {
    ScratchPath const area{ };
    auto writing{ Opened(area.File("latch.bin")) };
    ASSERT_TRUE(writing.has_value()) << writing.error();

    MarkRecord const oversized{ 1, std::string(MAXIMUM_PAYLOAD_SIZE, 'x') };
    EXPECT_FALSE(writing->Write(oversized).has_value());
    EXPECT_FALSE(writing->Status().has_value());
    EXPECT_FALSE(writing->Write(MarkRecord{ 2, "after" }).has_value());
    EXPECT_EQ(writing->Records(), 0u);
  }
}
