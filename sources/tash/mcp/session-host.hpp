#pragma once
// The run the tools drive: this module owns no session and no interpreter,
// so whoever embeds the server opens one.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace tash::mcp::detail::session_host
{
  using utilities::Result;

  // A hundred minutes of video at sixty a second, not a day's runaway loop.
  inline constexpr std::uint64_t DEFAULT_FRAME_BUDGET{ 360'000 };

  // What a caller leaves out, the host fills from the run it was started on.
  struct LaunchAsked
  {
    std::optional<std::filesystem::path> profile{ };
    std::optional<std::filesystem::path> bundle{ };
    std::optional<std::string>           name{ };
    std::optional<double>                rate{ };

    std::optional<std::uint64_t>         video_stride{ };
  };

  struct Launched
  {
    std::string                          core{ };
    std::string                          core_version{ };
    std::string                          rom{ };

    // What a checkpoint on disk is filed under, across runs.
    std::string                          rom_hash{ };
    double                               fps{ 0.0 };
    std::optional<std::filesystem::path> bundle{ };
  };

  struct JobReport
  {
    std::string   name{ };
    bool          running{ false };
    std::uint64_t frames{ 0 };

    // The limit the job began under; zero is python::UNLIMITED.
    std::uint64_t budget{ 0 };
    std::string   printed{ };
    std::string   errored{ };
    std::string   answered{ };
    std::string   refusal{ };
  };

  class SessionHost
  {
  public:
    SessionHost()          = default;
    virtual ~SessionHost() = default;

    SessionHost(SessionHost const&)                     = delete;
    auto operator = (SessionHost const&) -> SessionHost& = delete;

    [[nodiscard]] virtual auto Launch(LaunchAsked const& asked)
      -> Result<Launched> = 0;

    [[nodiscard]] virtual auto Shutdown() -> Result<std::string> = 0;

    // Null until a launch: the refusal every other tool answers with.
    [[nodiscard]] virtual auto Run() noexcept -> python::ScenarioRun* = 0;

    // Both are taken from under the lock the step thread publishes a frame
    // with, so a reader answers while a python job holds the run.
    [[nodiscard]] virtual auto Seen() const -> Result<bus::FrameKept> = 0;

    [[nodiscard]] virtual auto Observed() -> Result<python::Observation> = 0;

    [[nodiscard]] virtual auto Opened() const noexcept
      -> std::optional<Launched> const& = 0;

    // A job the call waits for; a client that stops waiting loses only it.
    [[nodiscard]] virtual auto Evaluate(std::string source,
                                        std::uint64_t frame_budget)
      -> Result<JobReport> = 0;

    // Answers at once; a second job while one runs is refused.
    [[nodiscard]] virtual auto Detach(std::string source,
                                      std::uint64_t frame_budget)
      -> Result<std::string> = 0;

    // Stops a running job at its next moving call; nothing running refuses.
    [[nodiscard]] virtual auto Cancel() -> Result<std::string> = 0;

    [[nodiscard]] virtual auto Busy() const -> std::optional<std::string> = 0;

    // The last `tail` bytes of each stream the job wrote; zero asks for all.
    [[nodiscard]] virtual auto JobOf(std::size_t tail) -> Result<JobReport>
      = 0;

    [[nodiscard]] virtual auto Report() -> Result<std::string> = 0;

    [[nodiscard]] virtual auto Tape() -> Result<std::string> = 0;
  };
}

namespace tash::mcp
{
  using detail::session_host::DEFAULT_FRAME_BUDGET;
  using detail::session_host::JobReport;
  using detail::session_host::LaunchAsked;
  using detail::session_host::Launched;
  using detail::session_host::SessionHost;
}
