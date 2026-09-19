#include "tash/tash/session-command.hpp"

#include "tash/mcp/argument-line.hpp"
#include "tash/mcp/mcp-client.hpp"
#include "tash/mcp/tool-table.hpp"
#include "tash/utilities/outcome.hpp"

#include <charconv>
#include <cstdlib>
#include <format>
#include <print>
#include <string_view>
#include <utility>

namespace tash::cli::detail::session_command
{
  using utilities::Result;

  namespace
  {
    [[nodiscard]] auto PortOf(std::uint16_t asked) -> Result<std::uint16_t>
    {
      if (asked != 0)
        return asked;
      char const* const said{ std::getenv(PORT_VARIABLE) };
      if (said == nullptr)
        return std::unexpected{ std::format(
          "--port, or {}, says which server to talk to", PORT_VARIABLE) };

      std::string_view const text{ said };
      std::uint16_t held{ 0 };
      auto const [stopped, failed]{ std::from_chars(
        text.data(), text.data() + text.size(), held) };
      if (failed != std::errc{ } || stopped != text.data() + text.size())
        return std::unexpected{ std::format("{} is not a port: '{}'",
                                            PORT_VARIABLE, text) };
      return held;
    }
  }

  auto SessionCommand::operator () (std::string tool) const
    -> oxbox::cli::CliResult
  {
    mcp::Tool const* const known{ mcp::ToolNamed(tool) };
    if (known == nullptr)
      return oxbox::cli::CliResult::UsageError(
        std::format("there is no '{}' tool", tool));

    Result<std::string> const given{
      mcp::ArgumentsFrom(known->schema(), arguments) };
    if (!given)
      return oxbox::cli::CliResult::UsageError(given.error());

    Result<std::uint16_t> const listening{ PortOf(port) };
    if (!listening)
      return oxbox::cli::CliResult::UsageError(listening.error());

    mcp::McpClient talking{ host, *listening };
    if (Result<std::string> const opened{ talking.Initialize() }; !opened)
      return oxbox::cli::CliResult::Failed(1, opened.error());

    Result<std::string> const said{ talking.CallTool(tool, *given) };

    // One call is one session: the server keeps every id it mints, so a
    // one-shot that never ended its own would leave it there for good.
    static_cast<void>(talking.End());
    if (!said)
      return oxbox::cli::CliResult::Failed(1, said.error());
    std::print("{}\n", *said);
    return {};
  }
}
