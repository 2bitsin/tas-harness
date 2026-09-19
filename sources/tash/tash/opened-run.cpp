#include "tash/tash/opened-run.hpp"

#include "tash/clock/determinism.hpp"
#include "tash/python/checkpoint-store.hpp"
#include "tash/report/render.hpp"
#include "tash/utilities/version.hpp"
#include "tash/watches/memory-map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::cli::detail::opened_run
{
  using utilities::Forwarded;
  using utilities::HARNESS_VERSION;
  using utilities::Outcome;
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view DEFAULT_NAME{ "run" };

    inline constexpr std::uint64_t FNV_OFFSET{ 14695981039346656037ull };
    inline constexpr std::uint64_t FNV_PRIME{ 1099511628211ull };
    inline constexpr std::size_t   HASH_BLOCK{ 64u * 1024u };

    // A checkpoint outlives its run, so the cache key has to be the ROM's
    // content rather than its path (FNV-1a, 64 bits).
    [[nodiscard]] auto ContentHashOf(std::filesystem::path const& rom)
      -> std::string
    {
      std::ifstream reading{ rom, std::ios::binary };
      std::uint64_t hash{ FNV_OFFSET };
      std::array<char, HASH_BLOCK> block{ };
      while (reading.read(block.data(), block.size()) || reading.gcount() != 0)
        for (std::streamsize at{ 0 }; at < reading.gcount(); ++at)
          hash = (hash ^ static_cast<unsigned char>(block[
                   static_cast<std::size_t>(at)])) * FNV_PRIME;
      return std::format("{:016x}", hash);
    }

    [[nodiscard]] auto BundleName(RunRequest const& asked) -> std::string
    {
      if (!asked.name.empty())
        return asked.name;
      if (asked.profile.name && !asked.profile.name->empty())
        return *asked.profile.name;
      std::filesystem::path const beside{
        asked.profile_path.parent_path().filename() };
      return beside.empty() ? std::string{ DEFAULT_NAME } : beside.string();
    }

    [[nodiscard]] auto SampleRateOf(session::Session const& run)
      -> std::uint32_t
    {
      double const rate{ run.CoreOf().AvInfo().timing.sample_rate };
      return rate > 0.0 ? static_cast<std::uint32_t>(rate) : 0u;
    }

    [[nodiscard]] auto TapeHeaderOf(std::string_view name,
                                    RunProfile const& profile,
                                    std::string const& profile_path)
      -> tape::TapeHeader
    {
      tape::TapeHeader header{ };
      header.name = name;
      header.profile = profile_path;
      header.core = profile.core;
      return header;
    }

    [[nodiscard]] auto WatchNamesOf(watches::WatchSet const& watched)
      -> std::optional<std::vector<std::string>>
    {
      if (watched.Empty())
        return { };
      std::vector<std::string> names;
      for (watches::Watch const& watch : watched.All())
        names.push_back(watch.name);
      return names;
    }
  }

  auto CheckpointsUnder(std::string const& asked, std::string const& bundle)
    -> std::filesystem::path
  {
    if (!asked.empty())
      return asked;
    std::filesystem::path const beside{ bundle.empty()
                                          ? std::filesystem::path{ "." }
                                          : std::filesystem::path{ bundle } };
    return beside / python::CHECKPOINT_ROOT;
  }

  auto OpenedRun::Open(RunRequest asked) -> Result<std::unique_ptr<OpenedRun>>
  {
    Result<session::SessionSettings> settings{ SettingsFrom(asked.profile) };
    if (!settings)
      return Forwarded(settings);
    settings->pacing
      = asked.rate > 0.0 ? clock::Pacing::Paced : clock::Pacing::Stepped;
    settings->rate = asked.rate;

    Result<std::unique_ptr<session::Session>> opened{
      session::Session::Open(std::move(*settings)) };
    if (!opened)
      return Forwarded(opened);

    auto open{ std::make_unique<OpenedRun>() };
    open->_profile = asked.profile;
    open->_profile_path = asked.profile_path.string();
    open->_video_stride = asked.video_stride;
    open->_session = std::move(*opened);
    session::Session& run{ *open->_session };
    open->_rom_hash = ContentHashOf(run.RomPath());
    run.Observe(open->_frames);

    // Every run keeps its line, bundle or not: a restore folds back to a
    // place only this knows, and the trace records that place.
    open->_tape = std::make_unique<TapeRecording>(run);
    run.Observe(*open->_tape);

    if (!asked.trace.empty())
    {
      Result<std::unique_ptr<journal::FrameJournal>> journalled{
        journal::FrameJournal::Open(asked.trace, asked.producer) };
      if (!journalled)
        return Forwarded(journalled);
      open->_journal = std::move(*journalled);
      run.Observe(*open->_journal);
    }

    if (!asked.bundle.empty())
    {
      Result<recorder::Bundle> made{
        recorder::Bundle::Create(asked.bundle, BundleName(asked)) };
      if (!made)
        return Forwarded(made);
      open->_bundle = std::move(*made);

      Result<std::unique_ptr<BundleRecording>> recording{
        BundleRecording::Open(*open->_bundle, run.Fps(), SampleRateOf(run),
                              asked.producer, asked.video_stride,
                              asked.quality) };
      if (!recording)
        return Forwarded(recording);
      open->_recording = std::move(*recording);
      run.Observe(static_cast<session::FrameObserver&>(*open->_recording));
      run.Observe(static_cast<session::AudioObserver&>(*open->_recording));

      Result<recorder::VerdictsWriter> verdicts{
        recorder::VerdictsWriter::Open(open->_bundle->Verdicts()) };
      if (!verdicts)
        return Forwarded(verdicts);
      open->_verdicts = std::move(*verdicts);
    }

    run.Record(open->TraceOf());

    Result<watches::WatchSet> watched{ WatchesFrom(asked.profile) };
    if (!watched)
      return Forwarded(watched);
    open->_watched = std::move(*watched);

    if (!open->_watched.Empty())
    {
      Result<std::unique_ptr<watches::Sampler>> sampler{
        watches::Sampler::Open(open->_watched, watches::MemoryMap::Of(run),
                               open->TraceOf()) };
      if (!sampler)
        return Forwarded(sampler);
      open->_sampler = std::move(*sampler);
      run.Observe(*open->_sampler);
      open->_watching.emplace(*open->_sampler);
    }

    // Last of the observers, so the frame it publishes carries the watch
    // values the sampler read for that same frame.
    open->_latest = std::make_unique<python::LatestFrame>(
      run, open->WatchedOf());
    run.Observe(*open->_latest);

    python::RunParts parts;
    parts.session = open->_session.get();
    parts.trace = open->TraceOf();
    parts.bundle = open->BundleOf();
    parts.verdicts = open->_verdicts ? &*open->_verdicts : nullptr;
    parts.watches = open->WatchedOf();
    parts.report = &std::cout;
    parts.quality = asked.quality;
    parts.marks = open->_tape.get();
    parts.line = open->_tape.get();
    parts.tape = open.get();
    if (!asked.checkpoints.empty())
      parts.checkpoints = asked.checkpoints / open->_rom_hash;
    open->_run = std::make_unique<python::ScenarioRun>(parts);
    return open;
  }

  auto OpenedRun::TraceOf() noexcept -> trace::Writer*
  {
    if (_journal)
      return _journal->WriterOf();
    return _recording ? _recording->WriterOf() : nullptr;
  }

  auto OpenedRun::ManifestOf(std::string status) const -> recorder::RunManifest
  {
    return recorder::RunManifest{
      std::string{ HARNESS_VERSION },
      _session->CoreOf().Information().name,
      _session->CoreOf().Information().version,
      _profile.rom,
      _profile_path,
      _session->Frames(),
      _session->Fps(),
      std::string{ clock::Named(_session->DeterminismLevel()) },
      std::move(status),
      _tape->Frames(),
      _run->Probes().probed,
      _run->Probes().parted,
      _run->Probes().unprobed,
      WatchNamesOf(_watched),
      _taped ? std::optional<std::string>{ recorder::TAPE_NAME }
             : std::optional<std::string>{ },
      _video_stride > recorder::EVERY_FRAME
        ? std::optional<std::uint64_t>{ _video_stride }
        : std::optional<std::uint64_t>{ }
    };
  }

  auto OpenedRun::TapeIntoBundle() const -> Outcome
  {
    return _tape->Write(
      *_bundle, TapeHeaderOf(_bundle->Root().filename().string(), _profile,
                             _profile_path));
  }

  auto OpenedRun::WriteTape() -> Result<python::TapeWritten>
  {
    if (!_bundle)
      return Refused("this run has no bundle to write a tape into");
    if (Outcome const flushed{ Flush() }; !flushed)
      return std::unexpected{ flushed.error() };
    if (Outcome const taped{ TapeIntoBundle() }; !taped)
      return std::unexpected{ taped.error() };
    _taped = true;

    // A replay reads the profile out of run.yaml, so the tape needs one.
    if (Outcome const named{ WriteManifest() }; !named)
      return std::unexpected{ named.error() };
    return python::TapeWritten{ std::filesystem::absolute(_bundle->Tape()),
                                _tape->Frames() };
  }

  auto OpenedRun::Flush() -> Outcome
  {
    if (_journal)
      return _journal->Flush();
    return _recording ? _recording->Flush() : Outcome{ };
  }

  auto OpenedRun::WriteManifest() -> Outcome
  {
    if (!_bundle)
      return Refused("this run has no bundle to write a manifest into");
    if (Outcome const flushed{ Flush() }; !flushed)
      return flushed;
    return _bundle->Write(ManifestOf(std::string{ "running" }));
  }

  auto OpenedRun::Close() -> Result<RunClosed>
  {
    if (_closed)
      return Refused("this run is already closed");
    _closed = true;
    _session->Record(nullptr);

    RunClosed finished;
    if (_journal)
    {
      Result<journal::JournalCounts> const counts{ _journal->Close() };
      if (!counts)
        return Forwarded(counts);
      finished.journal = *counts;
    }
    if (!_recording)
      return finished;

    Result<RecordingCounts> const counts{ _recording->Close() };
    if (!counts)
      return Forwarded(counts);
    finished.recording = *counts;
    finished.problem = _recording->Problem();

    Outcome const taped{ TapeIntoBundle() };
    _taped = taped.has_value();
    if (!taped && finished.problem.empty())
      finished.problem = taped.error();

    if (Outcome const written{ _bundle->Write(ManifestOf(
          finished.problem.empty() ? std::string{ "completed" }
                                   : finished.problem)) }; !written)
      return Forwarded(written);

    Result<std::filesystem::path> const page{
      report::RenderReport(_bundle->Root()) };
    if (!page)
      return Forwarded(page);
    finished.page = *page;
    return finished;
  }
}
