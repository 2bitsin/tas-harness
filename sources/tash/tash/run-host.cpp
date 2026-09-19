#include "tash/tash/run-host.hpp"

#include "tash/report/render.hpp"
#include "tash/tash/profile.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <system_error>
#include <utility>

namespace tash::cli::detail::run_host
{
  using utilities::Outcome;
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view PRODUCER{ "tash serve" };

    // What a waited-for job answers with: all of both streams.
    inline constexpr std::size_t WHOLE_JOB{ 0 };

    [[nodiscard]] auto SamePath(std::filesystem::path const& one,
                                std::filesystem::path const& other) -> bool
    {
      std::error_code failed;
      std::filesystem::path const first{
        std::filesystem::weakly_canonical(one, failed) };
      std::filesystem::path const second{
        std::filesystem::weakly_canonical(other, failed) };
      return !failed && first == second;
    }
  }

  RunHost::~RunHost()
  {
    Wait();
  }

  auto RunHost::Reopens(mcp::LaunchAsked const& asked) const -> bool
  {
    if (!asked.profile)
      return !asked.bundle && !asked.name && !asked.rate;
    Result<std::filesystem::path> const file{ ProfileFileAt(*asked.profile) };
    return file && SamePath(*file, _open->ProfilePathOf());
  }

  auto RunHost::Launch(mcp::LaunchAsked const& asked) -> Result<mcp::Launched>
  {
    Reap();

    // `tash serve --profile` opens the run before the first call, so the
    // launch an agent opens with is the run it already has.
    if (_open)
    {
      if (Reopens(asked))
        return *_opened;
      return Refused("mcp: launch: a session is already open; shutdown first");
    }

    std::optional<std::filesystem::path> const wanted{
      asked.profile ? asked.profile : _known.profile };
    if (!wanted)
      return Refused("mcp: launch: no profile: name one, or start the "
                     "server with `tash serve --profile <p>`");

    Result<std::filesystem::path> const file{ ProfileFileAt(*wanted) };
    if (!file)
      return std::unexpected{ file.error() };
    Result<RunProfile> read{ ProfileFrom(*file) };
    if (!read)
      return std::unexpected{ read.error() };

    RunRequest opening;
    opening.profile = std::move(*read);
    opening.profile_path = *file;
    opening.bundle = (asked.bundle ? asked.bundle : _known.bundle)
                       .value_or(std::filesystem::path{ });
    opening.name = (asked.name ? asked.name : _known.name)
                     .value_or(std::string{ });
    opening.rate = (asked.rate ? asked.rate : _known.rate).value_or(0.0);
    opening.video_stride
      = (asked.video_stride ? asked.video_stride : _known.video_stride)
          .value_or(recorder::EVERY_FRAME);
    opening.checkpoints = _checkpoints;
    opening.producer = PRODUCER;

    Result<std::unique_ptr<OpenedRun>> opened{
      OpenedRun::Open(std::move(opening)) };
    if (!opened)
      return std::unexpected{ opened.error() };
    _open = std::move(*opened);
    _interpreter.Bind(_open->ScenarioOf());

    session::Session& run{ _open->Live() };
    _opened = mcp::Launched{
      run.CoreOf().Information().name,
      run.CoreOf().Information().version,
      _open->ProfileOf().rom,
      _open->RomHashOf(),
      run.Fps(),
      _open->BundleOf()
        ? std::optional{ std::filesystem::absolute(_open->BundleOf()->Root()) }
        : std::nullopt
    };
    return *_opened;
  }

  auto RunHost::Shutdown() -> Result<std::string>
  {
    std::string said;
    if (std::optional<std::string> const busy{ Busy() }; busy)
      said = std::format("waited for {}\n", *busy);
    Wait();
    if (!_open)
      return Refused("mcp: shutdown: no session is open");

    said += std::format("ran {} frames", _open->Live().Frames());
    Result<RunClosed> const closed{ _open->Close() };
    if (!closed)
      { Forget(); return std::unexpected{ closed.error() }; }
    if (closed->recording)
      said += std::format(
        ", {} encoded into {}", closed->recording->encoded,
        std::filesystem::absolute(_open->BundleOf()->Root()).string());
    if (closed->page)
      said += std::format("\nreport {}",
                          std::filesystem::absolute(*closed->page).string());
    Forget();
    return said;
  }

  auto RunHost::Begins(std::uint64_t frame_budget) -> void
  {
    _job_from = _open ? _open->FramesMade() : 0;
    _job_budget = frame_budget;
    if (python::ScenarioRun* const run{ Run() }; run != nullptr)
      run->Budget().Begin(_job_from, frame_budget);
  }

  auto RunHost::Ran() const -> std::uint64_t
  {
    return _open ? _open->FramesMade() - _job_from : 0;
  }

  // The one place a job is started, detached or waited for: a synchronous
  // call is a job whose caller waits, so a client that stopped waiting
  // loses the answer and nothing else.
  auto RunHost::Started(std::string source, std::uint64_t frame_budget)
    -> Result<std::string>
  {
    Reap();
    if (std::optional<std::string> const busy{ Busy() }; busy)
      return Refused("mcp: {} is running; python_status", *busy);
    if (!_open)
      return Refused("mcp: nothing is launched; call launch first");

    std::string name{ std::format("python-{}", _jobs + 1) };
    Begins(frame_budget);

    // The worker needs the interpreter this thread is holding, and gets it
    // until it is waited for; nothing here touches python until then.
    _lent.emplace();
    if (Outcome const started{ _job.Start(name, [this, source]
        {
          pybind11::gil_scoped_acquire const holding;
          return _interpreter.Evaluate(
            source, [this](std::string_view text) { _job.Print(text); },
            [this](std::string_view text) { _job.PrintError(text); });
        }) }; !started)
    {
      _lent.reset();
      if (python::ScenarioRun* const run{ Run() }; run != nullptr)
        run->Budget().End();
      return std::unexpected{ started.error() };
    }
    ++_jobs;
    return name;
  }

  auto RunHost::Evaluate(std::string source, std::uint64_t frame_budget)
    -> Result<mcp::JobReport>
  {
    Result<std::string> const name{
      Started(std::move(source), frame_budget) };
    if (!name)
      return std::unexpected{ name.error() };
    Wait();
    return JobOf(WHOLE_JOB);
  }

  auto RunHost::Cancel() -> Result<std::string>
  {
    std::optional<std::string> const busy{ Busy() };
    if (!busy)
      return Refused("mcp: no python job is running");
    Run()->Budget().Stop();
    return std::format("{} stops at its next step, run_until or play", *busy);
  }

  auto RunHost::Detach(std::string source, std::uint64_t frame_budget)
    -> Result<std::string>
  {
    return Started(std::move(source), frame_budget);
  }

  auto RunHost::Busy() const -> std::optional<std::string>
  {
    if (!_job.Running())
      return { };
    return _job.Name();
  }

  auto RunHost::JobOf(std::size_t tail) -> Result<mcp::JobReport>
  {
    Reap();
    if (_jobs == 0)
      return Refused("mcp: no python job has been detached in this session");

    mcp::JobReport report;
    report.name = _job.Name();
    report.running = _job.Running();
    report.budget = _job_budget;
    report.printed = _job.Printed(tail);
    report.errored = _job.Errored(tail);
    // The live count until the worker has been waited for, because a job
    // that ends between the two reads has not been counted yet.
    report.frames = _job.Started() && _open ? Ran() : _job_frames;
    if (!report.running)
    {
      if (Result<std::string> const answered{ _job.Answer() }; answered)
        report.answered = *answered;
      else
        report.refusal = answered.error();
    }
    return report;
  }

  // Neither reaps: both are read while a job still holds the interpreter.
  auto RunHost::Seen() const -> Result<bus::FrameKept>
  {
    if (!_open)
      return Refused("mcp: nothing is launched; call launch first");
    return _open->Seen();
  }

  auto RunHost::Observed() -> Result<python::Observation>
  {
    if (!_open)
      return Refused("mcp: nothing is launched; call launch first");
    return _open->Observed();
  }

  auto RunHost::Report() -> Result<std::string>
  {
    Reap();
    if (!_open)
      return Refused("mcp: report: nothing is launched; call launch first");
    if (_open->BundleOf() == nullptr)
      return Refused("mcp: report: this run has no bundle to report on");

    // A job owns the trace while it runs, so neither the manifest nor the
    // flush the page usually gets can be taken: it is rendered from the
    // records already on disk, which the reader is built to end short of.
    if (!Busy())
      if (Outcome const written{ _open->WriteManifest() }; !written)
        return std::unexpected{ written.error() };
    Result<std::filesystem::path> const page{
      report::RenderReport(_open->BundleOf()->Root()) };
    if (!page)
      return std::unexpected{ page.error() };
    return std::filesystem::absolute(*page).string();
  }

  auto RunHost::Tape() -> Result<std::string>
  {
    Reap();
    if (!_open)
      return Refused("mcp: tape: nothing is launched; call launch first");
    if (std::optional<std::string> const busy{ Busy() }; busy)
      return Refused("mcp: tape: {} is running; python_status", *busy);

    Result<python::TapeWritten> const written{ _open->WriteTape() };
    if (!written)
      return std::unexpected{ written.error() };
    return std::format("{}\n{} frames", written->file.string(),
                       written->frames);
  }

  auto RunHost::Reap() -> void
  {
    if (!_job.Running())
      Wait();
  }

  auto RunHost::Wait() -> void
  {
    if (!_job.Started())
      return;
    _job.Wait();
    _lent.reset();
    // Frozen while the run the job drove is still the open one, so a later
    // launch does not take the number away with it.
    if (_open)
      _job_frames = Ran();
    if (python::ScenarioRun* const run{ Run() }; run != nullptr)
      run->Budget().End();
  }

  auto RunHost::Forget() -> void
  {
    _interpreter.Unbind();
    _open.reset();
    _opened.reset();
  }
}
