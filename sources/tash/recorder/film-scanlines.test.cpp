#include "tash/recorder/_scanlines.hpp"

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/_test-frame.hpp"
#include "tash/recorder/video-encoder.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace tash::recorder
{
  namespace
  {
    using detail::scanlines::Darken;
    using detail::scanlines::STUDIO_BLACK;
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 80 };
    constexpr std::uint32_t HEIGHT{ 48 };
    constexpr std::uint32_t SCALE{ 4 };
    constexpr double        FPS{ 60.0 };
    constexpr std::int64_t  FRAME_NANOSECONDS{ 16'666'667 };
    constexpr std::uint64_t FRAME_COUNT{ 8 };

    // A neutral grey, so the chroma planes the pass never touches carry no
    // colour and a row's brightness is the whole of what it is.
    constexpr std::uint16_t GREY{ 0x8410 };

    // A flat picture at crf 16 through x264's deblocking filter came back
    // at most 0.74 off the pass's own arithmetic, measured here.
    constexpr double CODED_TOLERANCE{ 1.0 };

    constexpr std::uint32_t PLANE_WIDTH{ 8 };
    constexpr std::uint32_t PLANE_HEIGHT{ 8 };
    constexpr std::uint32_t PLANE_PITCH{ 11 };
    constexpr std::uint8_t  PLANE_PADDING{ 0xCD };

    [[nodiscard]] auto Kept() -> double
    { return 1.0 - FILM_SCANLINE_DARKENING; }

    [[nodiscard]] auto Filmed(std::filesystem::path const& path,
                              EncoderSettings settings) -> bool
    {
      TestFrame frame{ WIDTH, HEIGHT };
      for (std::uint32_t y{ 0 }; y < HEIGHT; ++y)
        for (std::uint32_t x{ 0 }; x < WIDTH; ++x)
          frame.Set(x, y, GREY);

      auto encoder{ VideoEncoder::Open(path, WIDTH, HEIGHT, FPS, 0u,
                                       std::move(settings)) };
      EXPECT_TRUE(encoder.has_value()) << (encoder ? "" : encoder.error());
      if (!encoder)
        return false;
      for (std::uint64_t number{ 0 }; number < FRAME_COUNT; ++number)
        (*encoder)->Submit(frame.View(number),
                           static_cast<std::int64_t>(number)
                             * FRAME_NANOSECONDS);
      auto const counts{ (*encoder)->Close() };
      EXPECT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
      return counts.has_value() && counts->frames == FRAME_COUNT;
    }

    // The first coded picture back in RGB, one mean brightness per row, so
    // a row of the film can be held against its neighbours whatever the
    // codec kept its pixels in.
    [[nodiscard]] auto RowBrightness(std::filesystem::path const& path)
      -> std::vector<double>
    {
      std::vector<double> rows;
      AVFormatContext* opened{ nullptr };
      if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
          < 0)
        return rows;
      if (avformat_find_stream_info(opened, nullptr) < 0)
      {
        avformat_close_input(&opened);
        return rows;
      }

      int which{ -1 };
      for (unsigned index{ 0 }; index != opened->nb_streams; ++index)
        if (opened->streams[index]->codecpar->codec_type
            == AVMEDIA_TYPE_VIDEO)
          which = static_cast<int>(index);
      if (which < 0)
      {
        avformat_close_input(&opened);
        return rows;
      }

      AVCodecParameters const& about{ *opened->streams[which]->codecpar };
      AVCodec const* const codec{ avcodec_find_decoder(about.codec_id) };
      detail::libav::CodecHandle decoder{ avcodec_alloc_context3(codec) };
      EXPECT_NE(decoder, nullptr);
      EXPECT_GE(avcodec_parameters_to_context(decoder.get(), &about), 0);
      EXPECT_GE(avcodec_open2(decoder.get(), codec, nullptr), 0);

      detail::libav::PacketHandle packet{ av_packet_alloc() };
      detail::libav::FrameHandle picture{ av_frame_alloc() };
      while (rows.empty() && av_read_frame(opened, packet.get()) >= 0)
      {
        if (packet->stream_index == which
            && avcodec_send_packet(decoder.get(), packet.get()) >= 0
            && avcodec_receive_frame(decoder.get(), picture.get()) >= 0)
        {
          auto const wide{ static_cast<std::size_t>(picture->width) };
          auto const tall{ static_cast<std::size_t>(picture->height) };
          std::vector<std::uint8_t> rgb(wide * tall * 3u);
          detail::libav::ScalerHandle scaler{ sws_getContext(
            picture->width, picture->height,
            static_cast<AVPixelFormat>(picture->format), picture->width,
            picture->height, AV_PIX_FMT_RGB24, SWS_POINT, nullptr, nullptr,
            nullptr) };
          std::uint8_t* const into[]{ rgb.data(), nullptr, nullptr, nullptr };
          int const pitch[]{ static_cast<int>(wide * 3u), 0, 0, 0 };
          sws_scale(scaler.get(), picture->data, picture->linesize, 0,
                    picture->height, into, pitch);
          for (std::size_t y{ 0 }; y < tall; ++y)
          {
            double sum{ 0.0 };
            for (std::size_t at{ 0 }; at < wide * 3u; ++at)
              sum += rgb[y * wide * 3u + at];
            rows.push_back(sum / static_cast<double>(wide * 3u));
          }
        }
        av_packet_unref(packet.get());
      }
      avformat_close_input(&opened);
      return rows;
    }
  }

  TEST(RecorderFilmScanlines, TheLastRowOfEverySourceRowGoesTowardsBlack)
  {
    std::vector<std::uint8_t> plane(std::size_t{ PLANE_PITCH } * PLANE_HEIGHT,
                                    PLANE_PADDING);
    for (std::uint32_t y{ 0 }; y < PLANE_HEIGHT; ++y)
      for (std::uint32_t x{ 0 }; x < PLANE_WIDTH; ++x)
        plane[std::size_t{ PLANE_PITCH } * y + x] =
          static_cast<std::uint8_t>(STUDIO_BLACK + 20u * y + x);

    std::vector<std::uint8_t> const before{ plane };
    Darken(plane, PLANE_PITCH, PLANE_WIDTH, PLANE_HEIGHT, SCALE,
           FILM_SCANLINE_DARKENING);

    for (std::uint32_t y{ 0 }; y < PLANE_HEIGHT; ++y)
      for (std::uint32_t x{ 0 }; x < PLANE_PITCH; ++x)
      {
        auto const at{ std::size_t{ PLANE_PITCH } * y + x };
        if (x >= PLANE_WIDTH || y % SCALE != SCALE - 1u)
        {
          EXPECT_EQ(plane[at], before[at]) << "row " << y << " column " << x;
          continue;
        }
        auto const above{ static_cast<double>(before[at] - STUDIO_BLACK) };
        EXPECT_EQ(plane[at], static_cast<std::uint8_t>(
                    STUDIO_BLACK + std::lround(above * Kept())))
          << "row " << y << " column " << x;
      }
  }

  TEST(RecorderFilmScanlines, AFilmCarriesScanlinesAndARecordDoesNot)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-scanlines") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());

    auto const film{ area->File("film.mkv") };
    ASSERT_TRUE(Filmed(film, EncoderSettings{
      .preset = FILM_PRESET, .crf = FILM_CRF, .when_full = WhenFull::WAIT,
      .scale = SCALE, .keyframe_seconds = FILM_KEYFRAME_SECONDS,
      .scanline_darkening = FILM_SCANLINE_DARKENING }));

    std::vector<double> const rows{ RowBrightness(film) };
    ASSERT_EQ(rows.size(), std::size_t{ HEIGHT } * SCALE);

    double plain{ 0.0 };
    std::size_t counted{ 0 };
    for (std::size_t y{ 0 }; y < rows.size(); ++y)
      if (y % SCALE != SCALE - 1u)
      {
        plain += rows[y];
        ++counted;
      }
    plain /= static_cast<double>(counted);

    for (std::size_t y{ 0 }; y < rows.size(); ++y)
    {
      double const wanted{ y % SCALE == SCALE - 1u ? plain * Kept() : plain };
      EXPECT_NEAR(rows[y], wanted, CODED_TOLERANCE) << "row " << y;
    }

    auto const record{ area->File("record.mkv") };
    ASSERT_TRUE(Filmed(record, EncoderSettings{ .when_full = WhenFull::WAIT,
                                                .scale = SCALE }));
    std::vector<double> const flat{ RowBrightness(record) };
    ASSERT_EQ(flat.size(), rows.size());
    for (std::size_t y{ 0 }; y < flat.size(); ++y)
      EXPECT_NEAR(flat[y], flat[0], CODED_TOLERANCE) << "row " << y;
  }

  TEST(RecorderFilmScanlines, ALosslessRecordingIsNeverDarkened)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-scanlines-ffv1") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());

    auto const path{ area->File("lossless.mkv") };
    ASSERT_TRUE(Filmed(path, EncoderSettings{
      .lossless = true, .when_full = WhenFull::WAIT, .scale = SCALE,
      .scanline_darkening = FILM_SCANLINE_DARKENING }));

    std::vector<double> const rows{ RowBrightness(path) };
    ASSERT_EQ(rows.size(), std::size_t{ HEIGHT } * SCALE);
    for (std::size_t y{ 0 }; y < rows.size(); ++y)
      EXPECT_EQ(rows[y], rows[0]) << "row " << y;
  }
}
