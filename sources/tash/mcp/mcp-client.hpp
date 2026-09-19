#pragma once
// The other end of the same endpoint: the cli mirror's caller.

#include "tash/utilities/outcome.hpp"

#include <oxbox/http/fetch.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tash::mcp::detail::mcp_client
{
  using utilities::Outcome;
  using utilities::Result;

  // Server-sent events frame the body; one answer is one data line.
  [[nodiscard]] auto BodyOf(oxbox::http::TextReply const& reply)
    -> std::string;

  class McpClient
  {
  public:
    McpClient(std::string host, std::uint16_t port);

    // initialize then notifications/initialized (lifecycle 2.1, 2.2).
    [[nodiscard]] auto Initialize() -> Result<std::string>;

    // The result member of the answer, as JSON text.
    [[nodiscard]] auto Call(std::string_view method, std::string params)
      -> Result<std::string>;

    // Every block of a tools/call answer; an isError answer is the refusal.
    [[nodiscard]] auto CallTool(std::string_view name, std::string arguments)
      -> Result<std::string>;

    [[nodiscard]] auto End() -> Outcome;

    // One exchange, unread: for a caller that wants the status and headers.
    [[nodiscard]] auto Post(std::string body,
                            std::vector<oxbox::http::Header> extra = { })
      -> Result<oxbox::http::TextReply>;

    [[nodiscard]] auto Send(oxbox::http::Request request)
      -> Result<oxbox::http::TextReply>;

    [[nodiscard]] auto Session() const noexcept -> std::string const&
    { return _session; }

    [[nodiscard]] auto Url() const -> std::string;

  private:
    std::string   _host;
    std::uint16_t _port;
    std::string   _session{ };
    std::uint64_t _next{ 0 };
  };
}

namespace tash::mcp
{
  using detail::mcp_client::BodyOf;
  using detail::mcp_client::McpClient;
}
