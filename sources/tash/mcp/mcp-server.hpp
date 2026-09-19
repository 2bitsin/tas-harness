#pragma once
// MCP over Streamable HTTP: one endpoint, POST for messages and GET for the
// stream the server may speak on (transports 2.2 to 2.5).

#include "tash/mcp/dispatcher.hpp"
#include "tash/utilities/outcome.hpp"

#include <oxbox/http/response-stream.hpp>
#include <oxbox/http/server-message.hpp>
#include <oxbox/http/server.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace tash::mcp::detail::mcp_server
{
  using utilities::Result;

  // No pause an agent leaves is this long; a dead one holds the port for it.
  inline constexpr std::chrono::seconds SESSION_IDLE{ 30 * 60 };

  struct McpOptions
  {
    // Transports' security warning: a local server binds to the loopback and
    // not to every interface.
    std::string   host{ "127.0.0.1" };
    std::uint16_t port{ 0 };

    std::chrono::seconds idle{ SESSION_IDLE };
  };

  class McpServer
  {
  public:
    // Binds while it is built, so Port() answers before anything is served.
    [[nodiscard]] static auto Open(dispatcher::Dispatcher& answering,
                                   McpOptions options)
      -> Result<std::unique_ptr<McpServer>>;

    McpServer(dispatcher::Dispatcher& answering, McpOptions options);
    ~McpServer();

    McpServer(McpServer const&)                    = delete;
    auto operator = (McpServer const&) -> McpServer& = delete;

    auto Start() -> void;

    // Runs the endpoint until Stop; a tool call runs on this thread, which
    // is what keeps one process to one core and one interpreter.
    auto Run() -> void;

    auto Stop() -> void;

    [[nodiscard]] auto Port() const -> std::uint16_t;

  private:
    struct State;

    auto Sweep() -> void;

    std::unique_ptr<State> _state;
  };
}

namespace tash::mcp
{
  using detail::mcp_server::McpOptions;
  using detail::mcp_server::McpServer;
  using detail::mcp_server::SESSION_IDLE;
}
