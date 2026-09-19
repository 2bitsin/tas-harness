#pragma once
// `--name value` pairs read into the JSON the tool's own schema says.

#include "tash/mcp/tool-schema.hpp"
#include "tash/utilities/outcome.hpp"

#include <string>
#include <vector>

namespace tash::mcp::detail::argument_line
{
  using utilities::Result;

  // A boolean alone is true; an array takes one comma separated word.
  [[nodiscard]] auto ArgumentsFrom(tool_schema::ToolSchema const& schema,
                                   std::vector<std::string> const& words)
    -> Result<std::string>;
}

namespace tash::mcp
{
  using detail::argument_line::ArgumentsFrom;
}
