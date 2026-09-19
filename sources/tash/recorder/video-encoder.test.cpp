#include "tash/recorder/video-encoder.hpp"

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/_test-frame.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/bus/audio-ring.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>
#include <string>
#include <vector>

namespace tash::recorder
{
  namespace
  {
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 320 };
    constexpr std::uint32_t HEIGHT{ 240 };
    constexpr std::uint32_t SAMPLE_RATE{ 48000 };
    constexpr std::uint32_t PADDING_BYTES{ 16 };
    constexpr double        FPS{ 50.0 };
    constexpr std::int64_t  FRAME_NANOSECONDS{ 20'000'000 };
    constexpr std::uint64_t FRAME_COUNT{ 30 };
    constexpr std::size_t   SAMPLES_PER_FRAME{ 960 * 2 };

    struct Reopened
    {
      std::uint64_t video_packets{ 0 };
      std::uint64_t audio_packets{ 0 };
      std::int64_t  last_video_nanoseconds{ 0 };
      std::int64_t  last_audio_position{ -1 };
      AVRational    average_rate{ 0, 1 };
    };

    // What a reader sees, which is the only proof the muxer wrote what the
    // encoder was handed.
    [[nodiscard]] auto Reopen(std::filesystem::path const& path) -> Reopened
    {
      AVFormatContext* opened{ nullptr };
      Reopened seen{ };
      if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
          < 0)
        return seen;
      if (avformat_find_stream_info(opened, nullptr) < 0)
      {
        avformat_close_input(&opened);
        return seen;
      }

      detail::libav::PacketHandle packet{ av_packet_alloc() };
      while (av_read_frame(opened, packet.get()) >= 0)
      {
        AVStream const& stream{ *opened->streams[packet->stream_index] };
        if (stream.codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
          ++seen.video_packets;
          seen.last_video_nanoseconds = av_rescale_q(
            packet->pts, stream.time_base,
            AVRational{ 1, static_cast<int>(NANOSECONDS_PER_SECOND) });
        }
        else if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
          ++seen.audio_packets;
          seen.last_audio_position = av_rescale_q(
            packet->pts, stream.time_base,
            AVRational{ 1, stream.codecpar->sample_rate });
        }
        av_packet_unref(packet.get());
      }
      for (unsigned which{ 0 }; which != opened->nb_streams; ++which)
        if (opened->streams[which]->codecpar->codec_type
            == AVMEDIA_TYPE_VIDEO)
          seen.average_rate = opened->streams[which]->avg_frame_rate;
      avformat_close_input(&opened);
      return seen;
    }

    [[nodiscard]] auto Tone(std::size_t count) -> std::vector<std::int16_t>
    {
      std::vector<std::int16_t> samples(count, std::int16_t{ 0 });
      for (std::size_t which{ 0 }; which != count; ++which)
        samples[which] = static_cast<std::int16_t>((which % 512u) * 32u);
      return samples;
    }
  }

  TEST(RecorderVideoEncoder, EveryFrameIsWrittenAtItsHarnessTime)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-video") };
    ASSERT_TRUE(area.has_value()) << area.error();
    auto const path{ area->File("run.mkv") };

    // Deep enough that nothing is dropped: this test is about the muxer, and
    // back pressure has one of its own.
    EncoderSettings settings{ };
    settings.queue_depth = FRAME_COUNT * 4u;

    auto encoder{ VideoEncoder::Open(path, WIDTH, HEIGHT, FPS, SAMPLE_RATE,
                                     settings) };
    ASSERT_TRUE(encoder.has_value()) << encoder.error();

    TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
    auto const tone{ Tone(SAMPLES_PER_FRAME) };
    std::int64_t last{ 0 };
    for (std::uint64_t which{ 0 }; which != FRAME_COUNT; ++which)
    {
      frame.Paint(static_cast<std::uint32_t>(which));
      last = static_cast<std::int64_t>(which) * FRAME_NANOSECONDS;
      (*encoder)->Submit(frame.View(which), last);
      (*encoder)->SubmitAudio(tone);
    }

    auto const counts{ (*encoder)->Close() };
    ASSERT_TRUE(counts.has_value()) << counts.error();
    EXPECT_EQ(counts->frames, FRAME_COUNT);
    EXPECT_EQ(counts->dropped, 0u);
    EXPECT_EQ(counts->audio_chunks, FRAME_COUNT);

    Reopened const seen{ Reopen(path) };
    EXPECT_EQ(seen.video_packets, FRAME_COUNT);
    EXPECT_GT(seen.audio_packets, 0u);
    EXPECT_EQ(seen.last_video_nanoseconds, last);
    EXPECT_EQ(av_q2d(seen.average_rate), FPS);
  }

  TEST(RecorderVideoEncoder, AFullQueueDropsInsteadOfHoldingTheBusUp)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-drop") };
    ASSERT_TRUE(area.has_value()) << area.error();

    constexpr std::uint32_t WIDE{ 640 };
    constexpr std::uint32_t TALL{ 480 };
    constexpr std::uint64_t BURST{ 400 };

    EncoderSettings settings{ };
    settings.lossless = true;
    settings.queue_depth = 1u;

    auto encoder{ VideoEncoder::Open(area->File("burst.mkv"), WIDE, TALL, FPS,
                                     0u, settings) };
    ASSERT_TRUE(encoder.has_value()) << encoder.error();

    TestFrame frame{ WIDE, TALL };
    frame.Paint(0u);
    for (std::uint64_t which{ 0 }; which != BURST; ++which)
      (*encoder)->Submit(frame.View(which),
                         static_cast<std::int64_t>(which) * FRAME_NANOSECONDS);

    auto const counts{ (*encoder)->Close() };
    ASSERT_TRUE(counts.has_value()) << counts.error();
    EXPECT_EQ(counts->frames + counts->dropped, BURST);
    EXPECT_GT(counts->dropped, 0u);
  }

  TEST(RecorderVideoEncoder, TheHashNeverSeesTheEncoder)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-hash") };
    ASSERT_TRUE(area.has_value()) << area.error();

    TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
    frame.Paint(7u);
    auto const before{ perception::ExactHash(frame.View()) };
    ASSERT_TRUE(before.has_value()) << before.error();

    for (bool lossless : { false, true })
    {
      EncoderSettings settings{ };
      settings.lossless = lossless;
      settings.crf = lossless ? DEFAULT_CRF : 40;

      auto encoder{ VideoEncoder::Open(
        area->File(lossless ? "lossless.mkv" : "lossy.mkv"), WIDTH, HEIGHT,
        FPS, 0u, settings) };
      ASSERT_TRUE(encoder.has_value()) << encoder.error();

      (*encoder)->Submit(frame.View(), 0);
      auto const counts{ (*encoder)->Close() };
      ASSERT_TRUE(counts.has_value()) << counts.error();

      auto const after{ perception::ExactHash(frame.View()) };
      ASSERT_TRUE(after.has_value()) << after.error();
      EXPECT_EQ(*after, *before) << (lossless ? "lossless" : "lossy");
    }
  }

  TEST(RecorderVideoEncoder, ADroppedAudioBlockLeavesAGapAndNotAShift)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-audio") };
    ASSERT_TRUE(area.has_value()) << area.error();
    auto const path{ area->File("audio.mkv") };

    constexpr std::uint64_t BLOCKS{ 200 };
    constexpr std::int64_t SAMPLES_PER_BLOCK{ SAMPLES_PER_FRAME / 2 };

    EncoderSettings settings{ };
    settings.queue_depth = 1u;

    auto encoder{ VideoEncoder::Open(path, WIDTH, HEIGHT, FPS, SAMPLE_RATE,
                                     settings) };
    ASSERT_TRUE(encoder.has_value()) << encoder.error();

    TestFrame frame{ WIDTH, HEIGHT };
    frame.Paint(0u);
    auto const tone{ Tone(static_cast<std::size_t>(SAMPLES_PER_BLOCK)
                          * bus::AUDIO_CHANNELS) };
    (*encoder)->Submit(frame.View(), 0);
    for (std::uint64_t which{ 0 }; which != BLOCKS; ++which)
      (*encoder)->SubmitAudio(tone);

    // Nothing is in flight once every submission has been either written or
    // dropped, so the last block below cannot be the one that is dropped.
    auto const settled{ [&encoder] {
      return (*encoder)->Encoded() + (*encoder)->AudioChunks()
             + (*encoder)->Dropped() + (*encoder)->AudioDropped(); } };
    while (settled() < BLOCKS + 1u)
      std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
    (*encoder)->SubmitAudio(tone);

    auto const counts{ (*encoder)->Close() };
    ASSERT_TRUE(counts.has_value()) << counts.error();
    EXPECT_GT(counts->audio_dropped, 0u);
    EXPECT_EQ(counts->dropped, 0u);
    EXPECT_LT(counts->audio_chunks, BLOCKS + 1u);

    Reopened const seen{ Reopen(path) };
    EXPECT_EQ(seen.last_audio_position,
              static_cast<std::int64_t>(BLOCKS) * SAMPLES_PER_BLOCK);
  }

  TEST(RecorderVideoEncoder, ARecordingWithoutAGeometryIsRefused)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-empty") };
    ASSERT_TRUE(area.has_value()) << area.error();

    auto const encoder{ VideoEncoder::Open(area->File("none.mkv"), 0u, 0u,
                                           FPS, 0u) };
    ASSERT_FALSE(encoder.has_value());
    EXPECT_TRUE(encoder.error().starts_with("recorder: "))
      << encoder.error();
  }
}
