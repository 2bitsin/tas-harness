#include "tash/recorder/video-encoder.hpp"

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/_test-frame.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tash::recorder
{
  namespace
  {
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 320 };
    constexpr std::uint32_t HEIGHT{ 224 };
    constexpr std::uint32_t SCALE{ 4 };
    constexpr std::uint32_t PATTERN_SIDE{ 64 };
    constexpr double        FPS{ 60.0 };
    constexpr std::int64_t  FRAME_NANOSECONDS{ 16'666'667 };
    constexpr std::uint64_t FRAME_COUNT{ 8 };

    constexpr std::uint16_t RED{ 0xF800 };
    constexpr std::uint16_t GREEN{ 0x07E0 };
    constexpr std::uint16_t BLUE{ 0x001F };
    constexpr std::uint16_t WHITE{ 0xFFFF };

    struct Stream
    {
      std::uint32_t width{ 0 };
      std::uint32_t height{ 0 };
    };

    [[nodiscard]] auto StreamOf(std::filesystem::path const& path) -> Stream
    {
      Stream seen{ };
      AVFormatContext* opened{ nullptr };
      if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
          < 0)
        return seen;
      if (avformat_find_stream_info(opened, nullptr) >= 0)
        for (unsigned which{ 0 }; which != opened->nb_streams; ++which)
        {
          AVCodecParameters const& about{ *opened->streams[which]->codecpar };
          if (about.codec_type != AVMEDIA_TYPE_VIDEO)
            continue;
          seen.width  = static_cast<std::uint32_t>(about.width);
          seen.height = static_cast<std::uint32_t>(about.height);
        }
      avformat_close_input(&opened);
      return seen;
    }

    // The first picture of the film, back in RGB565 at the stream's size.
    [[nodiscard]] auto FirstPicture(std::filesystem::path const& path,
                                    Stream const& size)
      -> std::vector<std::uint16_t>
    {
      std::vector<std::uint16_t> picture;
      AVFormatContext* opened{ nullptr };
      if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
          < 0)
        return picture;
      if (avformat_find_stream_info(opened, nullptr) < 0)
      {
        avformat_close_input(&opened);
        return picture;
      }

      int which{ -1 };
      for (unsigned index{ 0 }; index != opened->nb_streams; ++index)
        if (opened->streams[index]->codecpar->codec_type
            == AVMEDIA_TYPE_VIDEO)
          which = static_cast<int>(index);
      if (which < 0)
      {
        avformat_close_input(&opened);
        return picture;
      }

      AVCodecParameters const& about{ *opened->streams[which]->codecpar };
      AVCodec const* const codec{ avcodec_find_decoder(about.codec_id) };
      detail::libav::CodecHandle decoder{ avcodec_alloc_context3(codec) };
      EXPECT_NE(decoder, nullptr);
      EXPECT_GE(avcodec_parameters_to_context(decoder.get(), &about), 0);
      EXPECT_GE(avcodec_open2(decoder.get(), codec, nullptr), 0);

      detail::libav::PacketHandle packet{ av_packet_alloc() };
      detail::libav::FrameHandle decoded{ av_frame_alloc() };
      while (picture.empty() && av_read_frame(opened, packet.get()) >= 0)
      {
        if (packet->stream_index == which
            && avcodec_send_packet(decoder.get(), packet.get()) >= 0
            && avcodec_receive_frame(decoder.get(), decoded.get()) >= 0)
        {
          detail::libav::ScalerHandle scaler{ sws_getContext(
            decoded->width, decoded->height,
            static_cast<AVPixelFormat>(decoded->format), decoded->width,
            decoded->height, AV_PIX_FMT_RGB565LE, SWS_POINT, nullptr, nullptr,
            nullptr) };
          picture.resize(std::size_t{ size.width } * size.height);
          std::uint8_t* const into[]{
            reinterpret_cast<std::uint8_t*>(picture.data()), nullptr, nullptr,
            nullptr };
          int const pitch[]{ static_cast<int>(size.width) * 2, 0, 0, 0 };
          sws_scale(scaler.get(), decoded->data, decoded->linesize, 0,
                    decoded->height, into, pitch);
        }
        av_packet_unref(packet.get());
      }
      avformat_close_input(&opened);
      return picture;
    }

    [[nodiscard]] auto Written(std::filesystem::path const& path,
                               TestFrame const& frame, std::uint32_t width,
                               std::uint32_t height,
                               EncoderSettings settings) -> bool
    {
      auto encoder{ VideoEncoder::Open(path, width, height, FPS, 0u,
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
      if (!counts)
        return false;
      EXPECT_EQ(counts->frames, FRAME_COUNT);
      EXPECT_EQ(counts->dropped, 0u);
      return true;
    }
  }

  TEST(RecorderFilmScale, TheFilmWidthPicksTheSmallestWholeUpscale)
  {
    EXPECT_EQ(FilmScaleFor(WIDTH), SCALE);
    EXPECT_GE(FilmScaleFor(WIDTH) * WIDTH, FILM_WIDTH_AT_LEAST);
    EXPECT_EQ(FilmScaleFor(FILM_WIDTH_AT_LEAST / 2u), 2u);
    EXPECT_EQ(FilmScaleFor(FILM_WIDTH_AT_LEAST), 1u);
    EXPECT_EQ(FilmScaleFor(FILM_WIDTH_AT_LEAST + 1u), 1u);
    EXPECT_EQ(FilmScaleFor(0u), 1u);
  }

  TEST(RecorderFilmScale, AFilmAtScaleFourIsFourTimesTheCoresGeometry)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-film-size") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const path{ area->File("film.mkv") };

    TestFrame frame{ WIDTH, HEIGHT };
    frame.Paint(0u);
    ASSERT_TRUE(Written(path, frame, WIDTH, HEIGHT,
                        EncoderSettings{ .preset = FILM_PRESET,
                                         .crf = FILM_CRF,
                                         .when_full = WhenFull::WAIT,
                                         .scale = SCALE }));

    Stream const seen{ StreamOf(path) };
    EXPECT_EQ(seen.width, WIDTH * SCALE);
    EXPECT_EQ(seen.height, HEIGHT * SCALE);
  }

  TEST(RecorderFilmScale, EverySourcePixelBecomesABlockOfItsOwnColour)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-film-blocks") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const path{ area->File("blocks.mkv") };

    // Neighbours never share a colour, so a blended upscale could leave no
    // block flat; the codec is the lossless one, so what comes back is the
    // scaler's work and not the quantiser's.
    std::uint16_t const corners[2][2]{ { RED, GREEN }, { BLUE, WHITE } };
    TestFrame frame{ PATTERN_SIDE, PATTERN_SIDE };
    for (std::uint32_t y{ 0 }; y < PATTERN_SIDE; ++y)
      for (std::uint32_t x{ 0 }; x < PATTERN_SIDE; ++x)
        frame.Set(x, y, corners[y % 2u][x % 2u]);
    ASSERT_TRUE(Written(path, frame, PATTERN_SIDE, PATTERN_SIDE,
                        EncoderSettings{ .lossless = true,
                                         .when_full = WhenFull::WAIT,
                                         .scale = SCALE }));

    Stream const seen{ StreamOf(path) };
    ASSERT_EQ(seen.width, PATTERN_SIDE * SCALE);
    ASSERT_EQ(seen.height, PATTERN_SIDE * SCALE);

    std::vector<std::uint16_t> const picture{ FirstPicture(path, seen) };
    ASSERT_EQ(picture.size(), std::size_t{ seen.width } * seen.height);

    std::size_t blended{ 0 };
    for (std::uint32_t y{ 0 }; y < seen.height; ++y)
      for (std::uint32_t x{ 0 }; x < seen.width; ++x)
        if (picture[std::size_t{ y } * seen.width + x]
            != corners[(y / SCALE) % 2u][(x / SCALE) % 2u])
          ++blended;
    EXPECT_EQ(blended, 0u);
  }
}
