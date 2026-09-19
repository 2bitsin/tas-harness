#pragma once
// One opened run: the session and everything that records it, assembled
// once for `tash run` and for the run the mcp server hosts.

#include "tash/journal/frame-journal.hpp"
#include "tash/python/latest-frame.hpp"
#include "tash/python/sampler-watches.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/python/tape-sink.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/verdicts-writer.hpp"
#include "tash/session/restore-probe.hpp"
#include "tash/session/session.hpp"
#include "tash/tash/bundle-recording.hpp"
#include "tash/tash/frame-count.hpp"
#include "tash/tash/profile.hpp"
#include "tash/tash/tape-recording.hpp"
#include "tash/utilities/outcome.hpp"
#include "tash/watches/sampler.hpp"
#include "tash/watches/watch-set.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace tash::cli::detail::opened_run
{
  using utilities::Outcome;
  using utilities::Result;

  // Beside the bundles, never inside one, so a serve session shares them.
  [[nodiscard]] auto CheckpointsUnder(std::string const& asked,
                                      std::string const& bundle)
    -> std::filesystem::path;

  struct RunRequest
  {
    RunProfile            profile{ };
    std::filesystem::path profile_path{ };

    // A bare trace file, or a bundle root; never both.
    std::filesystem::path trace{ };
    std::filesystem::path bundle{ };

    // Where named checkpoints are cached between runs; empty keeps the
    // run's own in the process alone.
    std::filesystem::path checkpoints{ };

    std::string           name{ };
    double                rate{ 0.0 };

    // One frame in this many reaches the encoder, so the video of a long
    // run is a time-lapse the disk can hold.
    std::uint64_t         video_stride{ recorder::EVERY_FRAME };

    // A run's film is its search record; a replay's is meant to be watched,
    // so it is upscaled, better compressed, and waited on rather than lossy.
    recorder::Quality     quality{ recorder::Quality::RECORD };
    std::string           producer{ };
  };

  // What Close() finished, for whoever reports it.
  struct RunClosed
  {
    std::optional<journal::JournalCounts> journal{ };
    std::optional<RecordingCounts>        recording{ };
    std::optional<std::filesystem::path>  page{ };
    std::string                           problem{ };
  };

  class OpenedRun : public python::TapeSink
  {
  public:
    [[nodiscard]] static auto Open(RunRequest asked)
      -> Result<std::unique_ptr<OpenedRun>>;

    OpenedRun()                                       = default;
    ~OpenedRun() override                             = default;
    OpenedRun(OpenedRun const&)                        = delete;
    auto operator = (OpenedRun const&) -> OpenedRun&   = delete;

    [[nodiscard]] auto Live() noexcept -> session::Session&
    { return *_session; }

    [[nodiscard]] auto ProfileOf() const noexcept -> RunProfile const&
    { return _profile; }

    [[nodiscard]] auto ProfilePathOf() const noexcept -> std::string const&
    { return _profile_path; }

    [[nodiscard]] auto RomHashOf() const noexcept -> std::string const&
    { return _rom_hash; }

    [[nodiscard]] auto BundleOf() noexcept -> recorder::Bundle*
    { return _bundle ? &*_bundle : nullptr; }

    [[nodiscard]] auto SamplerOf() noexcept -> watches::Sampler*
    { return _sampler.get(); }

    [[nodiscard]] auto WatchedOf() const noexcept -> python::WatchValues const*
    { return _watching ? &*_watching : nullptr; }

    [[nodiscard]] auto TraceOf() noexcept -> trace::Writer*;

    // The frames of the line the run kept, which is what its tape holds.
    [[nodiscard]] auto KeptOf() const noexcept -> std::uint64_t
    { return _tape->Frames(); }

    // What the run's checkpoints cost in probes, and what they found.
    [[nodiscard]] auto ProbesOf() const noexcept -> session::ProbeCounts
    { return _run->Probes(); }

    // Frames made since the run opened, safe to read from another thread.
    [[nodiscard]] auto FramesMade() const noexcept -> std::uint64_t
    { return _frames.Made(); }

    [[nodiscard]] auto ScenarioOf() noexcept -> python::ScenarioRun&
    { return *_run; }

    // The frame the step thread published last, and the observation over
    // it: the two reads a tool makes while a python job holds the run.
    [[nodiscard]] auto Seen() const -> Result<bus::FrameKept>
    { return _latest->Kept(); }

    [[nodiscard]] auto Observed() -> Result<python::Observation>
    { return _latest->Observed(); }

    // The manifest as the run stands, so a report rendered before the run
    // ends has the run's names and numbers in it.
    [[nodiscard]] auto WriteManifest() -> Outcome;

    // The tape as the line stands, so a replay films a run still going.
    [[nodiscard]] auto WriteTape() -> Result<python::TapeWritten> override;

    // Closes the writers, writes the manifest and renders the report.
    [[nodiscard]] auto Close() -> Result<RunClosed>;

  private:
    [[nodiscard]] auto ManifestOf(std::string status) const
      -> recorder::RunManifest;
    [[nodiscard]] auto Flush() -> Outcome;

    [[nodiscard]] auto TapeIntoBundle() const -> Outcome;

    RunProfile                              _profile{ };
    std::string                             _profile_path{ };
    std::string                             _rom_hash{ };
    std::uint64_t                           _video_stride{
                                              recorder::EVERY_FRAME };
    std::unique_ptr<session::Session>       _session{ };
    FrameCount                              _frames{ };
    std::unique_ptr<journal::FrameJournal>  _journal{ };
    std::optional<recorder::Bundle>         _bundle{ };
    std::unique_ptr<BundleRecording>        _recording{ };
    std::unique_ptr<TapeRecording>          _tape{ };
    std::optional<recorder::VerdictsWriter> _verdicts{ };
    watches::WatchSet                       _watched{ };
    std::unique_ptr<watches::Sampler>       _sampler{ };
    std::optional<python::SamplerWatches>   _watching{ };
    std::unique_ptr<python::LatestFrame>    _latest{ };

    // Last, so it is gone before anything it points at.
    std::unique_ptr<python::ScenarioRun>    _run{ };

    bool                                    _closed{ false };
    bool                                    _taped{ false };
  };
}

namespace tash::cli
{
  using detail::opened_run::CheckpointsUnder;
  using detail::opened_run::OpenedRun;
  using detail::opened_run::RunClosed;
  using detail::opened_run::RunRequest;
}
