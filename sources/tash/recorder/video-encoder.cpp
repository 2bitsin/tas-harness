#include "tash/recorder/video-encoder.hpp"

#include "tash/recorder/_frame-queue.hpp"
#include "tash/recorder/_mkv-writer.hpp"

#include "tash/bus/audio-ring.hpp"

#include <cstring>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tash::recorder::detail::video_encoder
{
  using utilities::Outcome;
  using utilities::Refused;

  namespace
  {
    // libav's log level is global and there is nowhere else to put it; set
    // once when the first recording opens, never read back.
    auto QuietenLibav() -> void { av_log_set_level(AV_LOG_ERROR); }

    [[nodiscard]] auto PackedRows(bus::FrameView const& frame)
      -> std::vector<std::uint8_t>
    {
      auto const row{ std::size_t{ frame.descriptor.width }
                      * bus::BYTES_PER_PIXEL };
      std::vector<std::uint8_t> packed(row * frame.descriptor.height,
                                       std::uint8_t{ 0 });
      for (std::uint32_t line{ 0 }; line < frame.descriptor.height; ++line)
      {
        auto const start{ std::size_t{ line } * frame.descriptor.pitch };
        if (start + row > frame.pixels.size())
          return { };
        std::memcpy(packed.data() + std::size_t{ line } * row,
                    frame.pixels.data() + start, row);
      }
      return packed;
    }
  }

  VideoEncoder::VideoEncoder(std::unique_ptr<mkv_writer::MkvWriter> writer,
                             std::unique_ptr<frame_queue::FrameQueue> queue)
  : _writer{ std::move(writer) }, _queue{ std::move(queue) }
  {
  }

  VideoEncoder::~VideoEncoder()
  {
    static_cast<void>(Close());
  }

  auto VideoEncoder::Open(std::filesystem::path const& path,
                          std::uint32_t width, std::uint32_t height,
                          double fps, std::uint32_t sample_rate,
                          EncoderSettings settings)
    -> Result<std::unique_ptr<VideoEncoder>>
  {
    QuietenLibav();

    auto writer{ mkv_writer::MkvWriter::Open(path, width, height, fps,
                                             sample_rate, settings) };
    if (!writer)
      return utilities::Forwarded(writer);

    auto queue{ std::make_unique<frame_queue::FrameQueue>(
      settings.queue_depth, settings.when_full) };
    std::unique_ptr<VideoEncoder> encoder{ new VideoEncoder{
      std::move(*writer), std::move(queue) } };
    encoder->_thread = std::thread{ [held = encoder.get()] { held->Run(); } };
    return encoder;
  }

  auto VideoEncoder::Run() -> void
  {
    while (auto work = _queue->Pop())
    {
      // A refusal stops the encoding but not the draining: a producer that
      // keeps submitting must still never block on this thread.
      if (!_problem.empty())
        continue;

      Outcome written{ std::visit(
        [this](auto const& held) -> Outcome
        {
          using Held = std::decay_t<decltype(held)>;
          if constexpr (std::is_same_v<Held, frame_queue::VideoWork>)
            return _writer->WriteVideo(held.pixels, held.width, held.height,
                                       held.at);
          else
            return _writer->WriteAudio(held.samples, held.position);
        },
        *work) };

      if (!written)
      {
        _problem = written.error();
        continue;
      }
      if (std::holds_alternative<frame_queue::VideoWork>(*work))
        _encoded.fetch_add(1, std::memory_order_relaxed);
      else
        _audio_chunks.fetch_add(1, std::memory_order_relaxed);
    }
  }

  auto VideoEncoder::Submit(bus::FrameView const& frame, std::int64_t at)
    -> void
  {
    if (_closed || frame.descriptor.width == 0u
        || frame.descriptor.height == 0u)
      return;

    frame_queue::VideoWork work{ PackedRows(frame), frame.descriptor.width,
                                 frame.descriptor.height, at };
    if (work.pixels.empty())
      return;
    if (!_queue->Push(std::move(work)))
      _dropped_frames.fetch_add(1, std::memory_order_relaxed);
  }

  auto VideoEncoder::SubmitAudio(std::span<std::int16_t const> interleaved)
    -> void
  {
    if (_closed || interleaved.empty())
      return;

    // _audio_position belongs to the submitting thread, which is the only
    // one that knows where in the run this block really starts.
    frame_queue::AudioWork work{
      std::vector<std::int16_t>{ interleaved.begin(), interleaved.end() },
      _audio_position };
    _audio_position += static_cast<std::int64_t>(interleaved.size()
                                                 / bus::AUDIO_CHANNELS);
    if (!_queue->Push(std::move(work)))
      _dropped_audio.fetch_add(1, std::memory_order_relaxed);
  }

  auto VideoEncoder::Close() -> Result<EncodedCounts>
  {
    EncodedCounts const counts{ Encoded(), Dropped(), AudioChunks(),
                                AudioDropped() };
    if (_closed)
      return counts;
    _closed = true;

    _queue->Finish();
    if (_thread.joinable())
      _thread.join();

    if (Outcome const finished{ _writer->Finish() };
        !finished && _problem.empty())
      _problem = finished.error();
    if (!_problem.empty())
      return Result<EncodedCounts>{ std::unexpected{ _problem } };

    return EncodedCounts{ Encoded(), Dropped(), AudioChunks(),
                          AudioDropped() };
  }
}
