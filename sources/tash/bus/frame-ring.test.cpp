#include "tash/bus/frame-ring.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace
{
  using tash::bus::FRAME_SLOTS;
  using tash::bus::FrameDescriptor;
  using tash::bus::FrameRing;

  auto Picture(std::uint32_t width, std::uint32_t height, std::size_t pitch,
               std::byte fill, std::byte padding) -> std::vector<std::byte>
  {
    std::vector<std::byte> pixels(height * pitch, padding);
    for (std::uint32_t line{ 0 }; line < height; ++line)
      for (std::size_t at{ 0 }; at < width * 2u; ++at)
        pixels[line * pitch + at] = fill;
    return pixels;
  }

  auto Descriptor(std::uint64_t number) -> FrameDescriptor
  {
    return FrameDescriptor{ number, 2, 2, 8, number / 60.0 };
  }
}

TEST(FrameRing, AnEmptyRingHasNothingToLookAtAndNothingToTake)
{
  FrameRing ring;
  EXPECT_FALSE(ring.Latest().has_value());
  EXPECT_FALSE(ring.Take().has_value());
  EXPECT_EQ(ring.Written(), 0u);
}

TEST(FrameRing, WhatWasPushedIsWhatIsTaken)
{
  FrameRing ring;
  std::vector<std::byte> const pixels{ Picture(2, 2, 8, std::byte{ 0x11 },
                                               std::byte{ 0 }) };
  ring.Push(Descriptor(7), pixels);

  auto const taken{ ring.Take() };
  ASSERT_TRUE(taken.has_value());
  EXPECT_EQ(taken->descriptor.number, 7u);
  EXPECT_EQ(taken->descriptor.pitch, 8u);
  EXPECT_EQ(std::vector<std::byte>(taken->pixels.begin(), taken->pixels.end()),
            pixels);
  EXPECT_FALSE(ring.Take().has_value());
}

TEST(FrameRing, TheOldestFrameIsLostOnceTheSlotsAreFull)
{
  FrameRing ring;
  std::vector<std::byte> const pixels{ Picture(2, 2, 8, std::byte{ 0x22 },
                                               std::byte{ 0 }) };
  for (std::uint64_t frame{ 0 }; frame < FRAME_SLOTS + 3; ++frame)
    ring.Push(Descriptor(frame), pixels);

  EXPECT_EQ(ring.Written(), FRAME_SLOTS + 3);
  EXPECT_EQ(ring.Dropped(), 3u);
  ASSERT_TRUE(ring.Latest().has_value());
  EXPECT_EQ(ring.Latest()->descriptor.number, FRAME_SLOTS + 2);

  auto const oldest{ ring.Take() };
  ASSERT_TRUE(oldest.has_value());
  EXPECT_EQ(oldest->descriptor.number, 3u);
}
