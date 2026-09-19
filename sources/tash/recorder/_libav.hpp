#pragma once
// libav's C handles as unique_ptrs, and its integer errors as words, so the
// encoder reads as C++ and every failure can be forwarded as a refusal.

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <memory>
#include <string>

namespace tash::recorder::detail::libav
{
  struct FormatCloser
  {
    auto operator () (AVFormatContext* context) const noexcept -> void
    { avformat_free_context(context); }
  };

  struct CodecCloser
  {
    auto operator () (AVCodecContext* context) const noexcept -> void
    { avcodec_free_context(&context); }
  };

  struct FrameCloser
  {
    auto operator () (AVFrame* frame) const noexcept -> void
    { av_frame_free(&frame); }
  };

  struct PacketCloser
  {
    auto operator () (AVPacket* packet) const noexcept -> void
    { av_packet_free(&packet); }
  };

  struct ScalerCloser
  {
    auto operator () (SwsContext* scaler) const noexcept -> void
    { sws_freeContext(scaler); }
  };

  using FormatHandle = std::unique_ptr<AVFormatContext, FormatCloser>;
  using CodecHandle  = std::unique_ptr<AVCodecContext, CodecCloser>;
  using FrameHandle  = std::unique_ptr<AVFrame, FrameCloser>;
  using PacketHandle = std::unique_ptr<AVPacket, PacketCloser>;
  using ScalerHandle = std::unique_ptr<SwsContext, ScalerCloser>;

  [[nodiscard]] inline auto Worded(int error) -> std::string
  {
    char said[AV_ERROR_MAX_STRING_SIZE]{ };
    av_strerror(error, said, sizeof said);
    return std::string{ said };
  }
}
