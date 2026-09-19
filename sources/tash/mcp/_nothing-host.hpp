#pragma once
// A host with nothing launched and nothing that can be: what the lifecycle
// and the session map answer with no target behind them.

#include "tash/mcp/session-host.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace tash::mcp::testing
{
  class NothingHost final : public SessionHost
  {
  public:
    auto Launch(LaunchAsked const&) -> utilities::Result<Launched> override
    { return utilities::Refused("no host"); }

    auto Shutdown() -> utilities::Result<std::string> override
    { return utilities::Refused("no host"); }

    auto Run() noexcept -> python::ScenarioRun* override
    { return nullptr; }

    auto Opened() const noexcept -> std::optional<Launched> const& override
    { return _nothing; }

    auto Evaluate(std::string, std::uint64_t)
      -> utilities::Result<JobReport> override
    { return utilities::Refused("no host"); }

    auto Detach(std::string, std::uint64_t)
      -> utilities::Result<std::string> override
    { return utilities::Refused("no host"); }

    auto Cancel() -> utilities::Result<std::string> override
    { return utilities::Refused("no host"); }

    auto Busy() const -> std::optional<std::string> override
    { return { }; }

    auto JobOf(std::size_t) -> utilities::Result<JobReport> override
    { return utilities::Refused("no host"); }

    auto Report() -> utilities::Result<std::string> override
    { return utilities::Refused("no host"); }

    auto Tape() -> utilities::Result<std::string> override
    { return utilities::Refused("no host"); }

    auto Seen() const -> utilities::Result<bus::FrameKept> override
    { return utilities::Refused("no host"); }

    auto Observed() -> utilities::Result<python::Observation> override
    { return utilities::Refused("no host"); }

  private:
    std::optional<Launched> _nothing{ };
  };
}
