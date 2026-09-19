#pragma once
// The mcp module's session seam, filled with the run `tash run` assembles:
// one session, one bundle, one sampler and one interpreter per process.

#include "tash/mcp/session-host.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/python-job.hpp"
#include "tash/tash/scenario-runner.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace tash::cli::detail::run_host
{
  using utilities::Result;

  class RunHost final : public mcp::SessionHost
  {
  public:
    RunHost()           = default;
    ~RunHost() override;

    // What `launch` opens for every field the caller leaves out.
    auto Remember(mcp::LaunchAsked asked) -> void
    { _known = std::move(asked); }

    // Where every run this host opens caches its named checkpoints.
    auto KeepCheckpointsUnder(std::filesystem::path under) -> void
    { _checkpoints = std::move(under); }

    [[nodiscard]] auto Launch(mcp::LaunchAsked const& asked)
      -> Result<mcp::Launched> override;

    [[nodiscard]] auto Shutdown() -> Result<std::string> override;

    [[nodiscard]] auto Run() noexcept -> python::ScenarioRun* override
    { return _open ? &_open->ScenarioOf() : nullptr; }

    [[nodiscard]] auto Seen() const -> Result<bus::FrameKept> override;

    [[nodiscard]] auto Observed() -> Result<python::Observation> override;

    [[nodiscard]] auto Opened() const noexcept
      -> std::optional<mcp::Launched> const& override
    { return _opened; }

    [[nodiscard]] auto Evaluate(std::string source,
                                std::uint64_t frame_budget)
      -> Result<mcp::JobReport> override;

    [[nodiscard]] auto Detach(std::string source,
                              std::uint64_t frame_budget)
      -> Result<std::string> override;

    [[nodiscard]] auto Cancel() -> Result<std::string> override;

    [[nodiscard]] auto Busy() const -> std::optional<std::string> override;

    [[nodiscard]] auto JobOf(std::size_t tail)
      -> Result<mcp::JobReport> override;

    [[nodiscard]] auto Report() -> Result<std::string> override;

    [[nodiscard]] auto Tape() -> Result<std::string> override;

  private:
    // Lends the interpreter to a worker running that source; the name.
    [[nodiscard]] auto Started(std::string source, std::uint64_t frame_budget)
      -> Result<std::string>;

    // The budget the job about to run keeps, and the frames it has made.
    auto Begins(std::uint64_t frame_budget) -> void;
    [[nodiscard]] auto Ran() const -> std::uint64_t;

    // True when `asked` is the run that is already open rather than another.
    [[nodiscard]] auto Reopens(mcp::LaunchAsked const& asked) const -> bool;
    auto Forget() -> void;

    // Takes the interpreter back from a job that has answered; Wait blocks
    // for one that has not.
    auto Reap() -> void;
    auto Wait() -> void;

    ScenarioRunner               _interpreter{ };
    mcp::LaunchAsked             _known{ };
    std::filesystem::path        _checkpoints{ };
    std::unique_ptr<OpenedRun>   _open{ };
    std::optional<mcp::Launched> _opened{ };
    std::uint64_t                _jobs{ 0 };
    std::uint64_t                _job_from{ 0 };
    std::uint64_t                _job_frames{ 0 };
    std::uint64_t                _job_budget{ 0 };

    // Declared last, and in this order: the worker is joined before the
    // interpreter comes back to this thread, and both before the run they
    // were driving.
    std::optional<pybind11::gil_scoped_release> _lent{ };
    PythonJob                                   _job{ };
  };
}

namespace tash::cli
{
  using detail::run_host::RunHost;
}
