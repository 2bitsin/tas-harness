#pragma once
// Every tool this server offers, in the order tools/list prints them.

#include "tash/mcp/tool.hpp"

#include <string_view>
#include <vector>

namespace tash::mcp::detail::tool_table
{
  [[nodiscard]] auto TargetTools() -> std::vector<tool::Tool>;

  [[nodiscard]] auto PlayTools() -> std::vector<tool::Tool>;

  [[nodiscard]] auto WatchTools() -> std::vector<tool::Tool>;

  [[nodiscard]] auto AllTools() -> std::vector<tool::Tool> const&;

  [[nodiscard]] auto ToolNamed(std::string_view name) -> tool::Tool const*;
}

namespace tash::mcp
{
  using detail::tool_table::AllTools;
  using detail::tool_table::ToolNamed;
}
