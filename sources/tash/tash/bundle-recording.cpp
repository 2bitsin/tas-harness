#include "tash/tash/bundle-recording.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tash::cli::detail::bundle_recording
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    // A record is searched and its producer is live; a film is watched, and
    // a replay has all the time in the world to make one.
    [[nodiscard]] auto SettingsFor(recorder::Quality quality,
                                   std::uint32_t width)
      -> recorder::EncoderSettings
    {
      recorder::EncoderSettings settings{ };
      settings.queue_depth = RECORDING_QUEUE_DEPTH;
      if (quality == recorder::Quality::RECORD)
        return settings;
      settings.preset = recorder::FILM_PRESET;
      settings.crf = recorder::FILM_CRF;
      settings.when_full = recorder::WhenFull::WAIT;
      settings.scale = recorder::FilmScaleFor(width);
      settings.keyframe_seconds = recorder::FILM_KEYFRAME_SECONDS;
      settings.scanline_darkening = recorder::FILM_SCANLINE_DARKENING;
      return settings;
    }
  }

  auto BundleRecording::Open(recorder::Bundle const& bundle, double fps,
                             std::uint32_t sample_rate, std::string producer,
                             std::uint64_t video_stride,
                             recorder::Quality quality)
    -> Result<std::unique_ptr<BundleRecording>>
  {
    if (video_stride == 0)
      return Refused("a video stride counts frames, so it is at least 1");
    Result<std::unique_ptr<journal::FrameJournal>> journal{
      journal::FrameJournal::Open(bundle.Trace(), std::move(producer)) };
    if (!journal)
      return Forwarded(journal);
    return std::make_unique<BundleRecording>(bundle, std::move(*journal), fps,
                                             sample_rate, video_stride,
                                             quality);
  }

  BundleRecording::BundleRecording(
    recorder::Bundle const& bundle,
    std::unique_ptr<journal::FrameJournal> journal, double fps,
    std::uint32_t sample_rate, std::uint64_t video_stride,
    recorder::Quality quality)
  : _video{ bundle.Video() }, _journal{ std::move(journal) }, _fps{ fps },
    // A time-lapse has nothing to lay real-time sound under, so above a
    // stride of one the recording opens with no audio track at all.
    _sample_rate{ video_stride > recorder::EVERY_FRAME ? 0u : sample_rate },
    _stride{ video_stride }, _quality{ quality }
  {
  }

  auto BundleRecording::OnFrame(bus::FrameView const& frame,
                                std::int64_t harness_time) -> void
  {
    bool const keeping{ _seen++ % _stride == 0 };
    std::int64_t const at{ TimeOf(_kept) };
    if (keeping)
      ++_kept;
    if (_encoder != nullptr)
    {
      if (keeping)
        _encoder->Submit(frame, at);
    }
    else if (_problem.empty())
    {
      Hold(frame, at, keeping);
      if (_seen >= SETTLE_FRAMES)
        OpenEncoder();
    }
    _journal->OnFrame(frame, harness_time);
  }

  auto BundleRecording::OnAudio(std::span<std::int16_t const> interleaved)
    -> void
  {
    if (_sample_rate == 0)
      return;
    if (_encoder != nullptr)
      _encoder->SubmitAudio(interleaved);
    else
      HoldAudio(interleaved);
  }

  auto BundleRecording::Close() -> Result<RecordingCounts>
  {
    if (_closed)
      return Refused("the recording is closed already");
    _closed = true;

    if (_encoder == nullptr && !_held.empty())
      OpenEncoder();

    RecordingCounts counts{ };
    if (_encoder != nullptr)
    {
      Result<recorder::EncodedCounts> const encoded{ _encoder->Close() };
      if (!encoded)
        Latch(encoded.error());
      else
      {
        counts.encoded = encoded->frames;
        counts.dropped = encoded->dropped;
        counts.audio_chunks = encoded->audio_chunks;
        counts.audio_dropped = encoded->audio_dropped;
      }
      counts.width = _width * _scale;
      counts.height = _height * _scale;
    }

    Result<journal::JournalCounts> const recorded{ _journal->Close() };
    if (!recorded)
      return Forwarded(recorded);
    counts.recorded = recorded->recorded;
    // The encoder's refusals, plus the blocks that never reached it.
    counts.audio_dropped += _audio_dropped;
    return counts;
  }

  auto BundleRecording::Hold(bus::FrameView const& frame, std::int64_t at,
                             bool keeping) -> void
  {
    _width = std::max(_width, frame.descriptor.width);
    _height = std::max(_height, frame.descriptor.height);
    if (!keeping)
      return;
    _held.push_back(HeldFrame{
      frame.descriptor,
      std::vector<std::byte>{ frame.pixels.begin(), frame.pixels.end() },
      at });
  }

  auto BundleRecording::HoldAudio(std::span<std::int16_t const> interleaved)
    -> void
  {
    if (!_problem.empty() || _held_audio.size() >= SETTLE_FRAMES)
    {
      ++_audio_dropped;
      return;
    }
    _held_audio.emplace_back(interleaved.begin(), interleaved.end());
  }

  auto BundleRecording::OpenEncoder() -> void
  {
    recorder::EncoderSettings const settings{ SettingsFor(_quality, _width) };
    _scale = settings.scale;
    Result<std::unique_ptr<recorder::VideoEncoder>> opened{
      recorder::VideoEncoder::Open(_video, _width, _height, _fps,
                                   _sample_rate, settings) };
    if (!opened)
    {
      Latch(opened.error());
      _audio_dropped += _held_audio.size();
      _held.clear();
      _held_audio.clear();
      return;
    }
    _encoder = std::move(*opened);
    for (HeldFrame const& held : _held)
      _encoder->Submit(bus::FrameView{ held.descriptor, held.pixels },
                       held.at);
    for (std::vector<std::int16_t> const& block : _held_audio)
      _encoder->SubmitAudio(block);
    _held.clear();
    _held_audio.clear();
  }

  auto BundleRecording::TimeOf(std::uint64_t kept) const noexcept
    -> std::int64_t
  {
    if (_fps <= 0.0)
      return 0;
    return std::llround(static_cast<double>(kept)
                        * static_cast<double>(
                            recorder::NANOSECONDS_PER_SECOND) / _fps);
  }

  auto BundleRecording::Latch(std::string reason) -> void
  {
    if (_problem.empty())
      _problem = std::move(reason);
  }
}
