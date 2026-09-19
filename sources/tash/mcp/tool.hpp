#pragma once
// One tool: what tools/list prints, the struct its arguments arrive in, and
// the body that runs it. The struct is the schema, so there is one
// declaration and never two.

#include "tash/mcp/content.hpp"
#include "tash/mcp/json-rpc.hpp"
#include "tash/mcp/tool-context.hpp"
#include "tash/mcp/tool-schema.hpp"
#include "tash/utilities/outcome.hpp"

#include <oxbox/serialization/canned-value.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <string_view>

namespace tash::mcp::detail::tool
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  using ToolBody = auto (*)(tool_context::ToolContext&,
                            json_rpc::RpcNode const*)
    -> Result<content::ToolOutcome>;

  struct Tool
  {
    std::string_view          name{ };
    std::string_view          description{ };
    tool_schema::ToolSchema (*schema)(){ nullptr };
    ToolBody                  run{ nullptr };
  };

  template <typename Args>
  [[nodiscard]] auto ArgumentsOf(json_rpc::RpcNode const* given)
    -> Result<Args>
  {
    if constexpr (!oxbox::serialization::HasScheme<Args>)
      return Args{ };
    else
    {
      json_rpc::RpcNode const none{ nlohmann::json::object() };
      try
      {
        return oxbox::serialization::Uncan<Args>(given ? *given : none);
      }
      catch (std::exception const& failure)
      {
        return Refused("mcp: those arguments do not fit: {}",
                       failure.what());
      }
    }
  }

  template <typename Args, auto Body>
  [[nodiscard]] auto Called(tool_context::ToolContext& tools,
                            json_rpc::RpcNode const* given)
    -> Result<content::ToolOutcome>
  {
    Result<Args> const read{ ArgumentsOf<Args>(given) };
    if (!read)
      return Forwarded(read);
    return Body(tools, *read);
  }

  template <typename Args, auto Body>
  [[nodiscard]] auto Entry(std::string_view name, std::string_view description)
    -> Tool
  {
    return Tool{ name, description, &tool_schema::SchemaOf<Args>,
                 &Called<Args, Body> };
  }
}

namespace tash::mcp
{
  using detail::tool::Tool;
  using detail::tool::ToolBody;
}
