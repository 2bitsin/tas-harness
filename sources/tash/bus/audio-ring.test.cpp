#include "tash/bus/audio-ring.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{
  using tash::bus::AUDIO_CAPACITY_FRAMES;
  using tash::bus::AUDIO_CHANNELS;
  using tash::bus::AudioRing;

  auto Tone(std::size_t samples, std::int16_t from) -> std::vector<std::int16_t>
  {
    std::vector<std::int16_t> tone(samples);
    for (std::size_t at{ 0 }; at < samples; ++at)
      tone[at] = static_cast<std::int16_t>(from + at);
    return tone;
  }
}

TEST(AudioRing, WhatWasWrittenIsWhatIsRead)
{
  AudioRing ring;
  std::vector<std::int16_t> const written{ Tone(8, 100) };
  ring.Write(written);
  EXPECT_EQ(ring.Available(), written.size());

  std::vector<std::int16_t> read(written.size());
  EXPECT_EQ(ring.Read(read), written.size());
  EXPECT_EQ(read, written);
  EXPECT_EQ(ring.Available(), 0u);
  EXPECT_EQ(ring.Dropped(), 0u);
}

TEST(AudioRing, ReadingMoreThanIsThereAnswersWhatIsThere)
{
  AudioRing ring;
  ring.Write(Tone(4, 0));
  std::vector<std::int16_t> read(16, -1);
  EXPECT_EQ(ring.Read(read), 4u);
  EXPECT_EQ(read[4], -1);
}

TEST(AudioRing, TheOldestSamplesGoWhenNothingReadsThem)
{
  AudioRing ring;
  std::size_t const capacity{ AUDIO_CAPACITY_FRAMES * AUDIO_CHANNELS };
  ring.Write(Tone(capacity, 0));
  ring.Write(Tone(4, 1));

  EXPECT_EQ(ring.Dropped(), 4u);
  EXPECT_EQ(ring.Available(), capacity);
  std::vector<std::int16_t> read(1);
  EXPECT_EQ(ring.Read(read), 1u);
  EXPECT_EQ(read[0], 4);
}
