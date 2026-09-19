#pragma once
// The numbers and spellings the wire fixes: JSON-RPC 2.0 4 and 5.1 for the
// envelope and its error codes, MCP 2025-06-18 for the rest.

#include "tash/utilities/version.hpp"

#include <string_view>

namespace tash::mcp::detail::protocol
{
  using utilities::HARNESS_VERSION;

  inline constexpr std::string_view JSONRPC_VERSION{ "2.0" };

  // The revision this server implements, echoed by `initialize` when the
  // client asks for it and offered instead when it asks for another.
  inline constexpr std::string_view PROTOCOL_VERSION{ "2025-06-18" };

  // A client that sends no MCP-Protocol-Version is talking the revision
  // before the header existed (transports 2.7).
  inline constexpr std::string_view ASSUMED_VERSION{ "2025-03-26" };

  inline constexpr std::string_view SERVER_NAME{ "tash" };

  // The one endpoint path both POST and GET are served on (transports 2.2).
  inline constexpr std::string_view MCP_PATH{ "/mcp" };

  inline constexpr std::string_view JSON_MEDIA_TYPE{ "application/json" };
  inline constexpr std::string_view SSE_MEDIA_TYPE{ "text/event-stream" };

  inline constexpr std::string_view SESSION_HEADER{ "Mcp-Session-Id" };
  inline constexpr std::string_view VERSION_HEADER{ "MCP-Protocol-Version" };

  inline constexpr std::string_view METHOD_INITIALIZE{ "initialize" };
  inline constexpr std::string_view METHOD_INITIALIZED{
    "notifications/initialized" };
  inline constexpr std::string_view METHOD_PING{ "ping" };
  inline constexpr std::string_view METHOD_TOOLS_LIST{ "tools/list" };
  inline constexpr std::string_view METHOD_TOOLS_CALL{ "tools/call" };

  // JSON-RPC 2.0 5.1.
  inline constexpr int PARSE_ERROR      { -32700 };
  inline constexpr int INVALID_REQUEST  { -32600 };
  inline constexpr int METHOD_NOT_FOUND { -32601 };
  inline constexpr int INVALID_PARAMS   { -32602 };
  inline constexpr int INTERNAL_ERROR   { -32603 };
}

namespace tash::mcp
{
  using detail::protocol::ASSUMED_VERSION;
  using detail::protocol::HARNESS_VERSION;
  using detail::protocol::INTERNAL_ERROR;
  using detail::protocol::INVALID_PARAMS;
  using detail::protocol::INVALID_REQUEST;
  using detail::protocol::JSONRPC_VERSION;
  using detail::protocol::JSON_MEDIA_TYPE;
  using detail::protocol::MCP_PATH;
  using detail::protocol::METHOD_INITIALIZE;
  using detail::protocol::METHOD_INITIALIZED;
  using detail::protocol::METHOD_NOT_FOUND;
  using detail::protocol::METHOD_PING;
  using detail::protocol::METHOD_TOOLS_CALL;
  using detail::protocol::METHOD_TOOLS_LIST;
  using detail::protocol::PARSE_ERROR;
  using detail::protocol::PROTOCOL_VERSION;
  using detail::protocol::SERVER_NAME;
  using detail::protocol::SESSION_HEADER;
  using detail::protocol::SSE_MEDIA_TYPE;
  using detail::protocol::VERSION_HEADER;
}
