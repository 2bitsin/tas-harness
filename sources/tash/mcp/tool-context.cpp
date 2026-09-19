#include "tash/mcp/tool-context.hpp"
#include "tash/mcp/observation-wording.hpp"

#include <format>
#include <optional>
#include <string>
#include <utility>

namespace tash::mcp::detail::tool_context
{
  using utilities::Refused;

  ToolContext::ToolContext(session_host::SessionHost& host)
  : _host{ &host }
  {}

  auto ToolContext::Live() -> Result<python::ScenarioRun*>
  {
    if (std::optional<std::string> const busy{ _host->Busy() }; busy)
      return Refused("mcp: {} is running; python_status", *busy);
    python::ScenarioRun* const run{ _host->Run() };
    if (run == nullptr)
      return Refused("mcp: nothing is launched; call launch first");
    return run;
  }

  auto ToolContext::Seen() -> Result<bus::FrameKept>
  {
    if (_host->Run() == nullptr)
      return Refused("mcp: nothing is launched; call launch first");
    return _host->Seen();
  }

  auto ToolContext::Observed() -> Result<python::Observation>
  {
    if (_host->Run() == nullptr)
      return Refused("mcp: nothing is launched; call launch first");
    if (_host->Busy())
      return _host->Observed();
    return _host->Run()->Observe();
  }

  auto ToolContext::Moved(std::string said) -> content::ToolOutcome
  {
    // No frame yet is not a refusal of what the tool has just done.
    if (Result<python::ScenarioRun*> const live{ Live() }; live)
      if (Result<python::Observation> const seen{ (*live)->Observe() }; seen)
        said += std::format("\n{}", observation_wording::Wording(*seen));
    return content::Said(std::move(said));
  }
}
