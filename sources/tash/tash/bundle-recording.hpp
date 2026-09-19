#pragma once

#include "tash/journal/frame-journal.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/run-manifest.hpp"
#include "tash/recorder/video-encoder.hpp"
#include "tash/session/audio-observer.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tash::cli::detail::bundle_recording
{
  using utilities::Outcome;
  using utilities::Result;

  struct RecordingCounts
  {
    std::uint64_t recorded{ 0 };
    std::uint64_t encoded{ 0 };
    std::uint64_t dropped{ 0 };
    std::uint64_t audio_chunks{ 0 };

    // Blocks no encoder ever existed to take, which is none unless the
    // recording could not be opened at all.
    std::uint64_t audio_dropped{ 0 };

    // The film's own size, the run's geometry times the quality's upscale.
    std::uint32_t width{ 0 };
    std::uint32_t height{ 0 };
  };

  // A Mega Drive boots in one video mode and switches to another within a
  // few frames, so the first frame's geometry is not the run's. The first
  // second is held and the recording takes the widest geometry in it.
  inline constexpr std::size_t SETTLE_FRAMES{ 60 };

  // The held second arrives at the encoder in one burst, so the queue has to
  // be deeper than that burst before it drops or blocks.
  inline constexpr std::size_t RECORDING_QUEUE_DEPTH{ SETTLE_FRAMES * 4 };

  // Everything a run writes into a bundle while it runs: the trace through a
  // journal, the video and its audio through an encoder.
  class BundleRecording : public session::FrameObserver,
                          public session::AudioObserver
  {
  public:
    [[nodiscard]] static auto Open(recorder::Bundle const& bundle, double fps,
                                   std::uint32_t sample_rate,
                                   std::string producer,
                                   std::uint64_t video_stride
                                     = recorder::EVERY_FRAME,
                                   recorder::Quality quality
                                     = recorder::Quality::RECORD)
      -> Result<std::unique_ptr<BundleRecording>>;

    BundleRecording(recorder::Bundle const& bundle,
                    std::unique_ptr<journal::FrameJournal> journal, double fps,
                    std::uint32_t sample_rate, std::uint64_t video_stride,
                    recorder::Quality quality);
    ~BundleRecording() override = default;

    auto OnFrame(bus::FrameView const& frame, std::int64_t harness_time)
      -> void override;

    // Into the trace and not the film: the film is what this run ran.
    auto OnRewind(session::Rewind const& back) -> void override
    { _journal->OnRewind(back); }

    auto OnAudio(std::span<std::int16_t const> interleaved) -> void override;

    [[nodiscard]] auto Close() -> Result<RecordingCounts>;

    // The trace on disk, caught up with the run.
    [[nodiscard]] auto Flush() -> Outcome
    { return _journal->Flush(); }

    // The trace inside the bundle, for a second observer to write into.
    [[nodiscard]] auto WriterOf() noexcept -> trace::Writer*
    { return _journal->WriterOf(); }

    // Empty while the run is going the way it was asked to.
    [[nodiscard]] auto Problem() const -> std::string const&
    { return _problem; }

  private:
    struct HeldFrame
    {
      bus::FrameDescriptor   descriptor;
      std::vector<std::byte> pixels;
      std::int64_t           at{ 0 };
    };

    // The geometry the encoder opens on comes from every frame; only the
    // ones the stride keeps are held for it.
    auto Hold(bus::FrameView const& frame, std::int64_t at, bool keeping)
      -> void;

    // One kept frame is one frame period, whatever the run's counter did.
    [[nodiscard]] auto TimeOf(std::uint64_t kept) const noexcept
      -> std::int64_t;
    auto HoldAudio(std::span<std::int16_t const> interleaved) -> void;
    auto OpenEncoder() -> void;
    auto Latch(std::string reason) -> void;

    std::filesystem::path _video;
    std::unique_ptr<journal::FrameJournal> _journal;
    std::unique_ptr<recorder::VideoEncoder> _encoder;
    double _fps;
    std::uint32_t _sample_rate;
    std::uint64_t _stride;
    recorder::Quality _quality;
    std::uint64_t _seen{ 0 };
    std::uint64_t _kept{ 0 };
    std::vector<HeldFrame> _held;
    std::vector<std::vector<std::int16_t>> _held_audio;
    std::uint32_t _width{ 0 };
    std::uint32_t _height{ 0 };
    std::uint32_t _scale{ 1 };
    std::uint64_t _audio_dropped{ 0 };
    std::string _problem;
    bool _closed{ false };
  };
}

namespace tash::cli
{
  using detail::bundle_recording::BundleRecording;
  using detail::bundle_recording::RecordingCounts;
}
