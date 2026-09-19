#include "tash/recorder/video-encoder.hpp"

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/_test-frame.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace tash::recorder
{
  namespace
  {
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 96 };
    constexpr std::uint32_t HEIGHT{ 64 };
    constexpr double        FPS{ 60.0 };
    constexpr std::int64_t  FRAME_NANOSECONDS{ 16'666'667 };
    constexpr std::uint64_t FRAME_COUNT{ 240 };

    struct Packets
    {
      std::size_t all{ 0 };
      std::size_t key{ 0 };
    };

    [[nodiscard]] auto PacketsIn(std::filesystem::path const& path) -> Packets
    {
      Packets seen{ };
      AVFormatContext* opened{ nullptr };
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
        if (opened->streams[packet->stream_index]->codecpar->codec_type
            == AVMEDIA_TYPE_VIDEO)
        {
          ++seen.all;
          if ((packet->flags & AV_PKT_FLAG_KEY) != 0)
            ++seen.key;
        }
        av_packet_unref(packet.get());
      }
      avformat_close_input(&opened);
      return seen;
    }

    [[nodiscard]] auto Filmed(std::filesystem::path const& path,
                              EncoderSettings settings) -> Packets
    {
      TestFrame frame{ WIDTH, HEIGHT };
      auto encoder{ VideoEncoder::Open(path, WIDTH, HEIGHT, FPS, 0u,
                                       std::move(settings)) };
      EXPECT_TRUE(encoder.has_value()) << (encoder ? "" : encoder.error());
      if (!encoder)
        return { };
      for (std::uint64_t number{ 0 }; number < FRAME_COUNT; ++number)
      {
        frame.Paint(static_cast<std::uint32_t>(number));
        (*encoder)->Submit(frame.View(number),
                           static_cast<std::int64_t>(number)
                             * FRAME_NANOSECONDS);
      }
      auto const counts{ (*encoder)->Close() };
      EXPECT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
      if (counts)
        EXPECT_EQ(counts->frames, FRAME_COUNT);
      return PacketsIn(path);
    }
  }

  TEST(RecorderFilmKeyframes, ARecordIsEveryFrameAKeyframe)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-record-gop") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());

    // WAIT only so the count is the whole run; a record's own producer is
    // live and would drop at this pace.
    Packets const seen{ Filmed(
      area->File("record.mkv"),
      EncoderSettings{ .when_full = WhenFull::WAIT }) };
    EXPECT_EQ(seen.all, FRAME_COUNT);
    EXPECT_EQ(seen.key, FRAME_COUNT);
  }

  TEST(RecorderFilmKeyframes, AFilmCarriesAGroupOfPicturesInstead)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-film-gop") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());

    Packets const seen{ Filmed(
      area->File("film.mkv"),
      EncoderSettings{ .preset = FILM_PRESET, .crf = FILM_CRF,
                       .when_full = WhenFull::WAIT,
                       .keyframe_seconds = FILM_KEYFRAME_SECONDS }) };
    EXPECT_EQ(seen.all, FRAME_COUNT);
    EXPECT_GT(seen.key, 0u);

    // A keyframe every FILM_KEYFRAME_SECONDS of run, plus whatever x264
    // decides a scene cut is worth; nowhere near one per frame.
    auto const group{ static_cast<std::size_t>(FPS)
                      * FILM_KEYFRAME_SECONDS };
    EXPECT_LE(seen.key, FRAME_COUNT / group * 4u);
    EXPECT_LT(seen.key, FRAME_COUNT);
  }
}
