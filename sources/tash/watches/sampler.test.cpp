#include "tash/watches/sampler.hpp"

#include "_synthetic-memory.hpp"
#include "tash/trace/reader.hpp"

#include <format>
#include "tash/watches/watch-spec.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
  using namespace tash::watches;
  using namespace tash::watches::testing;
  namespace trace = tash::trace;

  constexpr std::uint32_t SCORE_AT{ 4u };
  constexpr std::uint32_t LEVEL_AT{ 8u };
  constexpr std::uint32_t CASH_AT{ 16u };

  [[nodiscard]] auto TraceFile(std::string_view named)
    -> std::filesystem::path
  {
    auto const path{ std::filesystem::temp_directory_path()
                     / std::format("tash-{}.tash", named) };
    std::filesystem::remove(path);
    return path;
  }

  [[nodiscard]] auto TwoWatches() -> WatchSet
  {
    std::vector<WatchSpec> specs(2);
    specs[0].name = "score";
    specs[0].address = "0x4";
    specs[1].name = "level";
    specs[1].address = "0x8";
    specs[1].width = WIDTH_BYTE;
    auto const set{ WatchSet::From(specs) };
    EXPECT_TRUE(set.has_value()) << (set ? "" : set.error());
    return set.value_or(WatchSet{});
  }

  [[nodiscard]] auto WatchRecordsIn(std::filesystem::path const& path)
    -> std::vector<trace::WatchRecord>
  {
    std::vector<trace::WatchRecord> found;
    auto source{ trace::Reader::Open(path) };
    EXPECT_TRUE(source.has_value()) << (source ? "" : source.error());
    if (!source)
      return found;
    while (auto const record{ source->Next() })
      if (auto const* held{ std::get_if<trace::WatchRecord>(&*record) })
        found.push_back(*held);
    return found;
  }
}

TEST(Sampler, WritesAWatchOnlyWhenItsValueMoved)
{
  auto const path{ TraceFile("sampler-moves") };
  SyntheticMemory memory;
  {
    auto writer{ trace::Writer::Open(path, "sampler test") };
    ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());
    auto sampler{ Sampler::Open(TwoWatches(), memory.Map(), &*writer) };
    ASSERT_TRUE(sampler.has_value()) << (sampler ? "" : sampler.error());

    memory.Write16(SCORE_AT, 100u);
    EXPECT_TRUE((*sampler)->Sample(0u).has_value());
    EXPECT_TRUE((*sampler)->Sample(1u).has_value());
    memory.Write16(SCORE_AT, 200u);
    EXPECT_TRUE((*sampler)->Sample(2u).has_value());
    EXPECT_TRUE((*sampler)->Sample(3u).has_value());

    EXPECT_EQ((*sampler)->Value("score"), 200);
    EXPECT_EQ((*sampler)->Value("level"), 0);
    EXPECT_FALSE((*sampler)->Value("jewels").has_value());
    EXPECT_TRUE(writer->Flush().has_value());
  }

  // Both watches at frame 0, then only the one that moved, at frame 2.
  std::vector<trace::WatchRecord> const written{ WatchRecordsIn(path) };
  ASSERT_EQ(written.size(), 3u);
  EXPECT_EQ(written[0], (trace::WatchRecord{ 0u, 0u, 100 }));
  EXPECT_EQ(written[1], (trace::WatchRecord{ 0u, 1u, 0 }));
  EXPECT_EQ(written[2], (trace::WatchRecord{ 2u, 0u, 200 }));
  std::filesystem::remove(path);
}

TEST(Sampler, WritesEveryWatchAgainAfterAReset)
{
  auto const path{ TraceFile("sampler-reset") };
  SyntheticMemory memory;
  {
    auto writer{ trace::Writer::Open(path, "sampler test") };
    ASSERT_TRUE(writer.has_value()) << (writer ? "" : writer.error());
    auto sampler{ Sampler::Open(TwoWatches(), memory.Map(), &*writer) };
    ASSERT_TRUE(sampler.has_value()) << (sampler ? "" : sampler.error());

    memory.Write16(SCORE_AT, 100u);
    EXPECT_TRUE((*sampler)->Sample(0u).has_value());
    (*sampler)->OnReset(tash::session::Reset{ 1u });
    EXPECT_TRUE((*sampler)->Sample(2u).has_value());
    EXPECT_TRUE(writer->Flush().has_value());
  }

  // The records at frame 0 are off the line a reset starts, so both watches
  // are written again at the frame it landed at, unmoved as they are.
  std::vector<trace::WatchRecord> const written{ WatchRecordsIn(path) };
  ASSERT_EQ(written.size(), 4u);
  EXPECT_EQ(written[2], (trace::WatchRecord{ 1u, 0u, 100 }));
  EXPECT_EQ(written[3], (trace::WatchRecord{ 1u, 1u, 0 }));
  std::filesystem::remove(path);
}

TEST(Sampler, SamplesWithNoTraceOpenForAPredicateToRead)
{
  SyntheticMemory memory;
  auto sampler{ Sampler::Open(TwoWatches(), memory.Map()) };
  ASSERT_TRUE(sampler.has_value()) << (sampler ? "" : sampler.error());
  EXPECT_FALSE((*sampler)->Value("score").has_value());

  memory.Poke(LEVEL_AT, { 3u });
  EXPECT_TRUE((*sampler)->Sample(0u).has_value());
  EXPECT_EQ((*sampler)->Value("level"), 3);
  EXPECT_EQ((*sampler)->Counts(), (SamplerCounts{ 0u, 0u }));
}

TEST(Sampler, RefusesAtTheProfileRatherThanOnEveryFrame)
{
  SyntheticMemory memory;
  std::vector<WatchSpec> specs(1);
  specs[0].name = "past-the-end";
  specs[0].address = std::format("{}", AREA_BYTES);
  auto const set{ WatchSet::From(specs) };
  ASSERT_TRUE(set.has_value());
  EXPECT_FALSE(Sampler::Open(*set, memory.Map()).has_value());

  specs[0].region = "video";
  auto const elsewhere{ WatchSet::From(specs) };
  ASSERT_TRUE(elsewhere.has_value());
  auto const missing{ Sampler::Open(*elsewhere, memory.Map()) };
  ASSERT_FALSE(missing.has_value());
  EXPECT_NE(missing.error().find("video"), std::string::npos);
}

TEST(Sampler, ReadsAByteSwappedLongwordThroughAWatch)
{
  SyntheticMemory memory;
  // 200,000 as a 68000 longword in byte-swapped work ram.
  memory.Poke(CASH_AT, { 0x03, 0x00, 0x40, 0x0D });

  std::vector<WatchSpec> specs(1);
  specs[0].name = "cash";
  specs[0].address = std::format("{}", CASH_AT);
  specs[0].width = WIDTH_LONG;
  specs[0].endian = "swapped";
  auto const set{ WatchSet::From(specs) };
  ASSERT_TRUE(set.has_value()) << (set ? "" : set.error());

  auto sampler{ Sampler::Open(*set, memory.Map()) };
  ASSERT_TRUE(sampler.has_value()) << (sampler ? "" : sampler.error());
  EXPECT_TRUE((*sampler)->Sample(0u).has_value());
  EXPECT_EQ((*sampler)->Value("cash"), 200000);
}

TEST(WatchSet, TwoWatchesCannotShareAName)
{
  std::vector<WatchSpec> specs(2);
  specs[0].name = "score";
  specs[0].address = "0";
  specs[1].name = "score";
  specs[1].address = "4";
  EXPECT_FALSE(WatchSet::From(specs).has_value());

  auto const set{ TwoWatches() };
  EXPECT_EQ(set.Count(), 2u);
  EXPECT_EQ(set.IndexOf("level"), 1u);
  EXPECT_FALSE(set.At(2u).has_value());
}
