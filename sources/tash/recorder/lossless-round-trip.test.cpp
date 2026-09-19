#include "tash/recorder/video-encoder.hpp"

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/_test-frame.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace tash::recorder
{
  namespace
  {
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 96 };
    constexpr std::uint32_t HEIGHT{ 64 };
    constexpr std::uint32_t PADDING_BYTES{ 24 };
    constexpr double        FPS{ 50.0 };
    constexpr std::int64_t  FRAME_NANOSECONDS{ 20'000'000 };
    constexpr std::uint64_t FRAME_COUNT{ 30 };

    // The visible rows of one frame, packed, which is what a hash sees and
    // what a lossless recording has to give back.
    [[nodiscard]] auto PackedRows(TestFrame const& frame)
      -> std::vector<std::uint8_t>
    {
      bus::FrameView const view{ frame.View() };
      std::vector<std::uint8_t> rows;
      auto const width{ std::size_t{ WIDTH } * bus::BYTES_PER_PIXEL };
      for (std::uint32_t line{ 0 }; line < HEIGHT; ++line)
      {
        auto const start{ line * view.descriptor.pitch };
        for (std::size_t at{ 0 }; at < width; ++at)
          rows.push_back(
            static_cast<std::uint8_t>(view.pixels[start + at]));
      }
      return rows;
    }

    // Decodes the recording back to RGB565, one packed picture per frame.
    [[nodiscard]] auto DecodedRows(std::filesystem::path const& path)
      -> std::vector<std::vector<std::uint8_t>>
    {
      std::vector<std::vector<std::uint8_t>> pictures;
      AVFormatContext* opened{ nullptr };
      if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
          < 0)
        return pictures;
      if (avformat_find_stream_info(opened, nullptr) < 0)
      {
        avformat_close_input(&opened);
        return pictures;
      }

      int which{ -1 };
      for (unsigned index{ 0 }; index != opened->nb_streams; ++index)
        if (opened->streams[index]->codecpar->codec_type
            == AVMEDIA_TYPE_VIDEO)
          which = static_cast<int>(index);
      if (which < 0)
      {
        avformat_close_input(&opened);
        return pictures;
      }

      AVCodecParameters const& about{ *opened->streams[which]->codecpar };
      AVCodec const* const codec{ avcodec_find_decoder(about.codec_id) };
      detail::libav::CodecHandle decoder{ avcodec_alloc_context3(codec) };
      EXPECT_NE(decoder, nullptr);
      EXPECT_GE(avcodec_parameters_to_context(decoder.get(), &about), 0);
      EXPECT_GE(avcodec_open2(decoder.get(), codec, nullptr), 0);

      detail::libav::PacketHandle packet{ av_packet_alloc() };
      detail::libav::FrameHandle picture{ av_frame_alloc() };
      detail::libav::ScalerHandle scaler{ };
      auto const row{ std::size_t{ WIDTH } * bus::BYTES_PER_PIXEL };

      auto take = [&](AVFrame const& decoded) {
        if (scaler == nullptr)
          scaler.reset(sws_getContext(
            decoded.width, decoded.height,
            static_cast<AVPixelFormat>(decoded.format), decoded.width,
            decoded.height, AV_PIX_FMT_RGB565LE, SWS_POINT, nullptr, nullptr,
            nullptr));
        std::vector<std::uint8_t> rows(row * HEIGHT);
        std::uint8_t* const into[]{ rows.data(), nullptr, nullptr, nullptr };
        int const pitch[]{ static_cast<int>(row), 0, 0, 0 };
        sws_scale(scaler.get(), decoded.data, decoded.linesize, 0,
                  decoded.height, into, pitch);
        pictures.push_back(std::move(rows));
      };

      while (av_read_frame(opened, packet.get()) >= 0)
      {
        if (packet->stream_index == which
            && avcodec_send_packet(decoder.get(), packet.get()) >= 0)
          while (avcodec_receive_frame(decoder.get(), picture.get()) >= 0)
            take(*picture);
        av_packet_unref(packet.get());
      }
      if (avcodec_send_packet(decoder.get(), nullptr) >= 0)
        while (avcodec_receive_frame(decoder.get(), picture.get()) >= 0)
          take(*picture);

      avformat_close_input(&opened);
      return pictures;
    }
  }

  TEST(RecorderLossless, EveryFrameComesBackAsItWentIn)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-lossless") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const path{ area->File("lossless.mkv") };

    std::vector<std::vector<std::uint8_t>> submitted;
    {
      auto encoder{ VideoEncoder::Open(path, WIDTH, HEIGHT, FPS, 0u,
                                       EncoderSettings{ .lossless = true }) };
      ASSERT_TRUE(encoder.has_value()) << (encoder ? "" : encoder.error());
      for (std::uint64_t number{ 0 }; number < FRAME_COUNT; ++number)
      {
        TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
        frame.Paint(static_cast<std::uint32_t>(number));
        submitted.push_back(PackedRows(frame));
        (*encoder)->Submit(frame.View(number),
                           static_cast<std::int64_t>(number)
                             * FRAME_NANOSECONDS);
      }
      auto const counts{ (*encoder)->Close() };
      ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
      EXPECT_EQ(counts->frames, FRAME_COUNT);
      EXPECT_EQ(counts->dropped, 0u);
    }

    auto const decoded{ DecodedRows(path) };
    ASSERT_EQ(decoded.size(), FRAME_COUNT);
    for (std::size_t number{ 0 }; number < FRAME_COUNT; ++number)
      EXPECT_EQ(decoded[number], submitted[number])
        << "frame " << number << " came back changed";
  }
}
