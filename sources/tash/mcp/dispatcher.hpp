#pragma once
// The methods this server answers and the order MCP's lifecycle puts them
// in: before initialize, nothing but initialize and ping is answered.

#include "tash/mcp/json-rpc.hpp"
#include "tash/mcp/tool-context.hpp"
#include "tash/mcp/tool-schema.hpp"

#include <_buildutil/reflect.hpp>

#include <optional>
#include <string>
#include <vector>

namespace tash::mcp::detail::dispatcher
{
  struct ToolsCapability
  {
    friend constexpr auto reflect_scheme(ToolsCapability*);

    _Label(listChanged) bool list_changed{ false };
  };

  struct Capabilities
  {
    friend constexpr auto reflect_scheme(Capabilities*);

    ToolsCapability tools{ };
  };

  struct ServerInfo
  {
    friend constexpr auto reflect_scheme(ServerInfo*);

    std::string name{ };
    std::string version{ };
  };

  struct InitializeResult
  {
    friend constexpr auto reflect_scheme(InitializeResult*);

    _Label(protocolVersion) std::string protocol_version{ };

    Capabilities capabilities{ };

    _Label(serverInfo) ServerInfo server_info{ };
  };

  struct InitializeParams
  {
    friend constexpr auto reflect_scheme(InitializeParams*);

    _Label(protocolVersion) std::optional<std::string> protocol_version{ };
  };

  struct ToolDescription
  {
    friend constexpr auto reflect_scheme(ToolDescription*);

    std::string name{ };
    std::string description{ };

    _Label(inputSchema) tool_schema::ToolSchema input_schema{ };
  };

  struct ToolsListResult
  {
    friend constexpr auto reflect_scheme(ToolsListResult*);

    std::vector<ToolDescription> tools{ };
  };

  struct ToolCallParams
  {
    friend constexpr auto reflect_scheme(ToolCallParams*);

    std::string                      name{ };
    std::optional<json_rpc::RpcNode> arguments{ };
  };

  // One message in, one of three things out: a result body, an error, or
  // neither, which is what a notification gets (transports 2.3.4).
  struct Answered
  {
    std::optional<std::string>        result{ };
    std::optional<json_rpc::RpcError> failed{ };
  };

  class Dispatcher
  {
  public:
    explicit Dispatcher(tool_context::ToolContext& tools) noexcept;

    [[nodiscard]] auto Handle(json_rpc::RpcMessage const& message)
      -> Answered;

    [[nodiscard]] auto Initialized() const noexcept -> bool
    { return _initialized; }

    // A DELETE ends the session, and an ended session is uninitialised.
    auto Reset() noexcept -> void
    { _initialized = false; }

    // Whether a run is open on the host the tools drive.
    [[nodiscard]] auto Serving() const -> bool;

  private:
    [[nodiscard]] auto Initialize(json_rpc::RpcMessage const& message)
      -> Answered;

    [[nodiscard]] auto ListTools() const -> Answered;

    [[nodiscard]] auto CallTool(json_rpc::RpcMessage const& message)
      -> Answered;

    tool_context::ToolContext* _tools;
    bool                       _initialized{ false };
  };
}

namespace tash::mcp
{
  using detail::dispatcher::Answered;
  using detail::dispatcher::Dispatcher;
  using detail::dispatcher::InitializeResult;
  using detail::dispatcher::ToolDescription;
  using detail::dispatcher::ToolsListResult;
}
