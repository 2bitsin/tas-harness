#pragma once
// The muxer half of a recording: one MKV, one video track and, when a sample
// rate is given, one PCM track. Not thread safe; the encoder thread owns it.

#include "tash/recorder/_libav.hpp"
#include "tash/recorder/encoder-settings.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace tash::recorder::detail::mkv_writer
{
  using utilities::Outcome;
  using utilities::Result;

  class MkvWriter
  {
  public:
    [[nodiscard]] static auto Open(std::filesystem::path const& path,
                                   std::uint32_t width, std::uint32_t height,
                                   double fps, std::uint32_t sample_rate,
                                   EncoderSettings const& settings)
      -> Result<std::unique_ptr<MkvWriter>>;

    MkvWriter(MkvWriter const&) = delete;
    auto operator = (MkvWriter const&) -> MkvWriter& = delete;

    ~MkvWriter();

    // `at` is the picture's place on the record's clock, in nanoseconds.
    [[nodiscard]] auto WriteVideo(std::span<std::uint8_t const> packed_rows,
                                  std::uint32_t width, std::uint32_t height,
                                  std::int64_t at) -> Outcome;

    // `position` is the block's first sample counted from the start of the
    // run, as the producer counted it; a dropped block leaves a gap here.
    [[nodiscard]] auto WriteAudio(std::span<std::int16_t const> interleaved,
                                  std::int64_t position) -> Outcome;

    [[nodiscard]] auto Finish() -> Outcome;

  private:
    MkvWriter() = default;

    [[nodiscard]] auto Drain(libav::CodecHandle const& codec,
                             AVStream const& stream) -> Outcome;

    libav::FormatHandle _format{ };
    libav::CodecHandle  _video{ };
    libav::CodecHandle  _audio{ };
    libav::FrameHandle  _picture{ };
    libav::PacketHandle _packet{ };
    libav::ScalerHandle _scaler{ };
    std::filesystem::path _path{ };
    AVStream*           _video_stream{ nullptr };
    AVStream*           _audio_stream{ nullptr };
    std::uint32_t       _width{ 0 };   // the stream's, the source's upscaled
    std::uint32_t       _height{ 0 };
    std::uint32_t       _source_width{ 0 };
    std::uint32_t       _source_height{ 0 };
    std::uint32_t       _scale{ 1 };
    double              _scanlines{ 0.0 };
    bool                _opened{ false };
    bool                _finished{ false };
  };
}
