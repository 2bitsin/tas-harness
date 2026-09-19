#pragma once

#include "tash/bus/frame-descriptor.hpp"
#include "tash/recorder/encoder-settings.hpp"
#include "tash/utilities/outcome.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <thread>

namespace tash::recorder::detail::frame_queue { class FrameQueue; }
namespace tash::recorder::detail::mkv_writer { class MkvWriter; }

namespace tash::recorder::detail::video_encoder
{
  using utilities::Result;

  struct EncodedCounts
  {
    std::uint64_t frames{ 0 };
    std::uint64_t dropped{ 0 };
    std::uint64_t audio_chunks{ 0 };
    std::uint64_t audio_dropped{ 0 };
  };

  // One MKV: an H.264 track (FFV1 when the settings ask for lossless) and, if
  // a sample rate is given, a PCM s16le stereo track. Submit copies the
  // visible rows and queues them, waiting only when the settings say WAIT.
  class VideoEncoder
  {
  public:
    [[nodiscard]] static auto Open(std::filesystem::path const& path,
                                   std::uint32_t width, std::uint32_t height,
                                   double fps, std::uint32_t sample_rate,
                                   EncoderSettings settings = { })
      -> Result<std::unique_ptr<VideoEncoder>>;

    VideoEncoder(VideoEncoder const&) = delete;
    auto operator = (VideoEncoder const&) -> VideoEncoder& = delete;

    ~VideoEncoder();

    // `at` is where the frame sits on this record's own clock, in
    // nanoseconds, counted the way the audio's sample position is.
    auto Submit(bus::FrameView const& frame, std::int64_t at) -> void;

    // The block is stamped with the run's sample position before it is
    // queued, so a dropped block leaves a gap and never a shift.
    auto SubmitAudio(std::span<std::int16_t const> interleaved) -> void;

    [[nodiscard]] auto Close() -> Result<EncodedCounts>;

    [[nodiscard]] auto Encoded() const noexcept -> std::uint64_t
    { return _encoded.load(std::memory_order_relaxed); }

    [[nodiscard]] auto AudioChunks() const noexcept -> std::uint64_t
    { return _audio_chunks.load(std::memory_order_relaxed); }

    [[nodiscard]] auto Dropped() const noexcept -> std::uint64_t
    { return _dropped_frames.load(std::memory_order_relaxed); }

    [[nodiscard]] auto AudioDropped() const noexcept -> std::uint64_t
    { return _dropped_audio.load(std::memory_order_relaxed); }

  private:
    VideoEncoder(std::unique_ptr<mkv_writer::MkvWriter> writer,
                 std::unique_ptr<frame_queue::FrameQueue> queue);

    auto Run() -> void;

    std::unique_ptr<mkv_writer::MkvWriter>   _writer;
    std::unique_ptr<frame_queue::FrameQueue> _queue;
    std::thread                              _thread{ };
    std::atomic<std::uint64_t>               _encoded{ 0 };
    std::atomic<std::uint64_t>               _audio_chunks{ 0 };
    std::atomic<std::uint64_t>               _dropped_frames{ 0 };
    std::atomic<std::uint64_t>               _dropped_audio{ 0 };
    std::int64_t                             _audio_position{ 0 };
    std::string                              _problem{ };
    bool                                     _closed{ false };
  };
}

namespace tash::recorder
{
  using detail::video_encoder::EncodedCounts;
  using detail::video_encoder::VideoEncoder;
}
