#include "tash/mcp/dispatcher.hpp"

#include "tash/mcp/protocol.hpp"
#include "tash/mcp/tool-table.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>
#include <format>
#include <utility>

namespace tash::mcp::detail::dispatcher
{
  using utilities::Result;

  namespace
  {
    inline constexpr std::string_view EMPTY_RESULT{ "{}" };

    auto Refusal(int code, std::string message) -> Answered
    {
      return Answered{ std::nullopt,
                       json_rpc::RpcError{ code, std::move(message),
                                           std::nullopt } };
    }

    auto Answer(auto const& result) -> Answered
    {
      return Answered{ oxbox::serialization::ToJson(result), std::nullopt };
    }

    auto Described() -> ToolsListResult
    {
      ToolsListResult listed;
      for (tool::Tool const& one : tool_table::AllTools())
        listed.tools.push_back(ToolDescription{ std::string{ one.name },
                                                std::string{ one.description },
                                                one.schema() });
      return listed;
    }
  }

  Dispatcher::Dispatcher(tool_context::ToolContext& tools) noexcept
  : _tools{ &tools }
  {}

  auto Dispatcher::Serving() const -> bool
  {
    return _tools->Host().Opened().has_value();
  }

  auto Dispatcher::Handle(json_rpc::RpcMessage const& message) -> Answered
  {
    if (!message.method)
      return Refusal(protocol::INVALID_REQUEST,
                     "mcp: a message with no method is not a request");

    std::string_view const method{ *message.method };
    if (method == protocol::METHOD_INITIALIZE)
      return Initialize(message);
    if (method == protocol::METHOD_INITIALIZED)
    {
      _initialized = true;
      return Answered{ };
    }
    if (method == protocol::METHOD_PING)
      return Answered{ std::string{ EMPTY_RESULT }, std::nullopt };

    if (!_initialized)
      return Refusal(protocol::INVALID_REQUEST,
                     std::format("mcp: {} before initialize", method));

    if (method == protocol::METHOD_TOOLS_LIST)
      return ListTools();
    if (method == protocol::METHOD_TOOLS_CALL)
      return CallTool(message);

    return Refusal(protocol::METHOD_NOT_FOUND,
                   std::format("mcp: no method named {}", method));
  }

  auto Dispatcher::Initialize(json_rpc::RpcMessage const& message) -> Answered
  {
    std::string wanted{ protocol::PROTOCOL_VERSION };
    if (message.params)
    {
      try
      {
        InitializeParams const asked{
          oxbox::serialization::Uncan<InitializeParams>(*message.params) };
        if (asked.protocol_version
            && *asked.protocol_version == protocol::ASSUMED_VERSION)
          wanted = *asked.protocol_version;
      }
      catch (std::exception const& failure)
      {
        return Refusal(protocol::INVALID_PARAMS,
                       std::format("mcp: initialize: {}", failure.what()));
      }
    }

    // The notification is what completes the handshake, but a client that
    // never sends it must still be answered (lifecycle 2.1).
    _initialized = true;

    InitializeResult answered;
    answered.protocol_version = std::move(wanted);
    answered.server_info = ServerInfo{ std::string{ protocol::SERVER_NAME },
                                       std::string{
                                         protocol::HARNESS_VERSION } };
    return Answer(answered);
  }

  auto Dispatcher::ListTools() const -> Answered
  {
    return Answer(Described());
  }

  auto Dispatcher::CallTool(json_rpc::RpcMessage const& message) -> Answered
  {
    if (!message.params)
      return Refusal(protocol::INVALID_PARAMS,
                     "mcp: tools/call needs a name and its arguments");

    ToolCallParams asked;
    try
    {
      asked = oxbox::serialization::Uncan<ToolCallParams>(*message.params);
    }
    catch (std::exception const& failure)
    {
      return Refusal(protocol::INVALID_PARAMS,
                     std::format("mcp: tools/call: {}", failure.what()));
    }

    tool::Tool const* const called{ tool_table::ToolNamed(asked.name) };
    if (called == nullptr)
      return Refusal(protocol::INVALID_PARAMS,
                     std::format("mcp: no tool named {}", asked.name));

    Result<content::ToolOutcome> const ran{
      called->run(*_tools, asked.arguments ? &*asked.arguments : nullptr) };
    if (!ran)
      return Refusal(protocol::INVALID_PARAMS,
                     content::Under(called->name, ran.error()));
    return Answer(content::Named(called->name, *ran));
  }
}
