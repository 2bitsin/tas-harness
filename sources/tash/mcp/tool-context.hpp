#pragma once
// What a tool body is handed: the run every tool reads and moves.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/mcp/content.hpp"
#include "tash/mcp/session-host.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/utilities/outcome.hpp"

#include <string>

namespace tash::mcp::detail::tool_context
{
  using utilities::Outcome;
  using utilities::Result;

  class ToolContext
  {
  public:
    explicit ToolContext(session_host::SessionHost& host);

    [[nodiscard]] auto Host() noexcept -> session_host::SessionHost&
    { return *_host; }

    [[nodiscard]] auto Live() -> Result<python::ScenarioRun*>;

    // A read that does not need the run to itself: the published frame is
    // the answer while a python job steps.
    [[nodiscard]] auto Seen() -> Result<bus::FrameKept>;

    [[nodiscard]] auto Observed() -> Result<python::Observation>;

    // A tool that moved the run answers its own sentence and then the
    // observation, so the caller never spends a call on `observe`.
    [[nodiscard]] auto Moved(std::string said) -> content::ToolOutcome;

  private:
    session_host::SessionHost* _host;
  };
}

namespace tash::mcp
{
  using detail::tool_context::ToolContext;
}
