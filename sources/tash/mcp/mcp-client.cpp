#include "tash/mcp/mcp-client.hpp"

#include "tash/mcp/content.hpp"
#include "tash/mcp/protocol.hpp"

#include <oxbox/http/asio.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <format>
#include <optional>
#include <utility>

namespace tash::mcp::detail::mcp_client
{
  namespace asio = boost::asio;
  namespace http = oxbox::http;

  using utilities::Forwarded;
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view ACCEPTED_TYPES{
      "application/json, text/event-stream" };
    inline constexpr std::string_view EVENT_PREFIX{ "data: " };
    inline constexpr int LOWEST_ERROR{ 400 };
  }

  auto BodyOf(http::TextReply const& reply) -> std::string
  {
    if (reply.content_type.Type() != protocol::SSE_MEDIA_TYPE)
      return reply.body;
    for (std::size_t at{ 0 }; at < reply.body.size();)
    {
      std::size_t const end{
        std::min(reply.body.find('\n', at), reply.body.size()) };
      std::string_view const line{ reply.body.data() + at, end - at };
      if (line.starts_with(EVENT_PREFIX))
        return std::string{ line.substr(EVENT_PREFIX.size()) };
      at = end + 1;
    }
    return reply.body;
  }

  McpClient::McpClient(std::string host, std::uint16_t port)
  : _host{ std::move(host) }
  , _port{ port }
  {}

  auto McpClient::Url() const -> std::string
  {
    return std::format("http://{}:{}{}", _host, _port, protocol::MCP_PATH);
  }

  auto McpClient::Send(http::Request request) -> Result<http::TextReply>
  {
    asio::io_context loop{ 1 };
    std::optional<http::TextReply> answered;
    std::string refusal;

    asio::co_spawn(
      loop,
      [&]() -> asio::awaitable<void>
      {
        try
        {
          answered = co_await http::Send(std::move(request));
        }
        catch (std::exception const& failure)
        {
          refusal = failure.what();
        }
      },
      asio::detached);
    loop.run();

    if (!refusal.empty())
      return Refused("mcp: {}", refusal);
    return std::move(*answered);
  }

  auto McpClient::Post(std::string body, std::vector<http::Header> extra)
    -> Result<http::TextReply>
  {
    http::Request request;
    request.method = http::Method::post;
    request.url = Url();
    request.payload = http::Payload{
      std::string{ protocol::JSON_MEDIA_TYPE }, std::move(body) };
    request.headers.push_back(
      http::Header{ "Accept", std::string{ ACCEPTED_TYPES } });
    request.headers.push_back(
      http::Header{ std::string{ protocol::VERSION_HEADER },
                    std::string{ protocol::PROTOCOL_VERSION } });
    if (!_session.empty())
      request.headers.push_back(
        http::Header{ std::string{ protocol::SESSION_HEADER }, _session });
    for (http::Header& header : extra)
      request.headers.push_back(std::move(header));
    return Send(std::move(request));
  }

  auto McpClient::Call(std::string_view method, std::string params)
    -> Result<std::string>
  {
    std::string const asked{ std::format(
      R"({{"jsonrpc":"{}","id":{},"method":"{}","params":{}}})",
      protocol::JSONRPC_VERSION, ++_next, method,
      params.empty() ? std::string{ "{}" } : params) };

    Result<http::TextReply> const reply{ Post(asked) };
    if (!reply)
      return Forwarded(reply);
    if (std::optional<std::string_view> const given{
          (*reply)[protocol::SESSION_HEADER] })
      _session = *given;

    std::string const body{ BodyOf(*reply) };
    if (reply->status >= LOWEST_ERROR && body.empty())
      return Refused("mcp: {} answered {}", method, reply->status);

    nlohmann::json read;
    try
    {
      read = nlohmann::json::parse(body);
    }
    catch (std::exception const& failure)
    {
      return Refused("mcp: {} answered nothing readable: {}", method,
                     failure.what());
    }
    if (read.contains("error"))
      return Refused("mcp: {}", read["error"].dump());
    if (!read.contains("result"))
      return Refused("mcp: {} answered without a result", method);
    return read["result"].dump();
  }

  auto McpClient::Initialize() -> Result<std::string>
  {
    std::string const params{ std::format(
      R"({{"protocolVersion":"{}","capabilities":{{}},)"
      R"("clientInfo":{{"name":"tash","version":"{}"}}}})",
      protocol::PROTOCOL_VERSION, protocol::PROTOCOL_VERSION) };

    Result<std::string> opened{ Call(protocol::METHOD_INITIALIZE, params) };
    if (!opened)
      return opened;

    std::string const said{ std::format(
      R"({{"jsonrpc":"{}","method":"{}"}})", protocol::JSONRPC_VERSION,
      protocol::METHOD_INITIALIZED) };
    if (Result<http::TextReply> const reply{ Post(said) }; !reply)
      return Forwarded(reply);
    return opened;
  }

  auto McpClient::CallTool(std::string_view name, std::string arguments)
    -> Result<std::string>
  {
    std::string const params{ std::format(
      R"({{"name":"{}","arguments":{}}})", name,
      arguments.empty() ? std::string{ "{}" } : arguments) };

    Result<std::string> const answered{
      Call(protocol::METHOD_TOOLS_CALL, params) };
    if (!answered)
      return answered;

    nlohmann::json const read = nlohmann::json::parse(*answered);
    std::string text;
    for (nlohmann::json const& block : read.value("content",
                                                  nlohmann::json::array()))
    {
      if (!text.empty())
        text += '\n';
      if (block.value("type", std::string{ }) == content::TEXT_BLOCK)
        text += block.value("text", std::string{ });
      else
        text += std::format("[{} {} bytes]",
                            block.value("mimeType", std::string{ }),
                            block.value("data", std::string{ }).size());
    }
    if (read.value("isError", false))
      return std::unexpected{ text };
    return text;
  }

  auto McpClient::End() -> Outcome
  {
    http::Request request;
    request.method = http::Method::delete_;
    request.url = Url();
    if (!_session.empty())
      request.headers.push_back(
        http::Header{ std::string{ protocol::SESSION_HEADER }, _session });
    Result<http::TextReply> const reply{ Send(std::move(request)) };
    if (!reply)
      return Forwarded(reply);
    _session.clear();
    return {};
  }
}
