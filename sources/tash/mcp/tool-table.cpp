#include "tash/mcp/tool-table.hpp"

#include <algorithm>
#include <utility>

namespace tash::mcp::detail::tool_table
{
  namespace
  {
    auto Gathered() -> std::vector<tool::Tool>
    {
      std::vector<tool::Tool> every{ TargetTools() };
      for (std::vector<tool::Tool> const& more : { PlayTools(), WatchTools() })
        every.insert(every.end(), more.begin(), more.end());
      return every;
    }
  }

  auto AllTools() -> std::vector<tool::Tool> const&
  {
    static std::vector<tool::Tool> const every{ Gathered() };
    return every;
  }

  auto ToolNamed(std::string_view name) -> tool::Tool const*
  {
    std::vector<tool::Tool> const& every{ AllTools() };
    auto const found{ std::ranges::find(every, name, &tool::Tool::name) };
    return found == every.end() ? nullptr : &*found;
  }
}
