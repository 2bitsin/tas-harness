#include "tash/linked/tash.hpp"

#include "_synthetic-target.hpp"

#include "tash/tape/tape.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace tash::linked
{
  namespace
  {
    using detail::synthetic_target::FRAMES;
    using detail::synthetic_target::Guest;
    using detail::synthetic_target::Picture;
    using detail::synthetic_target::Play;

    constexpr std::string_view TARGET{ "synthetic" };

    [[nodiscard]] auto Text(std::span<std::byte const> bytes) -> std::string
    {
      return std::string{ reinterpret_cast<char const*>(bytes.data()),
                          bytes.size() };
    }

    auto Wrote(std::filesystem::path const& path,
               std::span<std::byte const> bytes) -> void
    {
      std::ofstream file{ path, std::ios::binary | std::ios::trunc };
      file.write(reinterpret_cast<char const*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    }
  }

  TEST(MemorySink, TheBytesAreTheFilesAHostWithNoneWouldHaveWritten)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-memory") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    std::vector<std::uint16_t> const picture{ Picture() };

    std::filesystem::path const root{ scratch->File("run") };
    {
      Guest guest{ };
      Harness harness{ Options{ std::string{ TARGET }, { },
                                Sink::DIRECTORY, root, 60.0 } };
      Play(harness, guest, picture);
    }

    Guest guest{ };
    Harness harness{ Options{ std::string{ TARGET }, { }, Sink::MEMORY,
                              { }, 60.0 } };
    ASSERT_EQ(harness.Mode(), Mode::FILE);
    Play(harness, guest, picture);

    std::ifstream written{ root / "tape.yaml", std::ios::binary };
    std::string const on_disk{ std::istreambuf_iterator<char>{ written },
                               std::istreambuf_iterator<char>{ } };
    EXPECT_EQ(Text(harness.Bytes(Artifact::TAPE)), on_disk);
    EXPECT_FALSE(harness.Bytes(Artifact::MANIFEST).empty());

    // The trace is the one artifact the two cannot be equal in: its header
    // carries the moment it was created.
    std::filesystem::path const fetched{ scratch->File("fetched.bin") };
    Wrote(fetched, harness.Bytes(Artifact::TRACE));

    auto reader{ trace::Reader::Open(fetched) };
    ASSERT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    std::uint64_t frames{ 0 };
    reader->ForEach([&frames](auto const& held)
    {
      if constexpr (std::is_same_v<std::decay_t<decltype(held)>,
                                   trace::FrameRecord>)
        ++frames;
    });
    EXPECT_EQ(frames, FRAMES);
    EXPECT_FALSE(reader->Truncated());
  }

  TEST(MemorySink, TheTapeTheTargetFetchesIsOneThatReadsBack)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-memory-tape") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());

    std::vector<std::uint16_t> const picture{ Picture() };
    Guest guest{ };
    Harness harness{ Options{ std::string{ TARGET }, { }, Sink::MEMORY,
                              { }, 60.0 } };
    Play(harness, guest, picture);

    std::filesystem::path const path{ scratch->File("tape.yaml") };
    Wrote(path, harness.Bytes(Artifact::TAPE));

    auto const read{ tape::TapeFrom(path) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    ASSERT_EQ(read->segments.size(), 1u);
    EXPECT_EQ(read->segments.front().frames, FRAMES);
  }
}
