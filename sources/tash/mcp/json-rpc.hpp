#pragma once
// The envelope JSON-RPC 2.0 4 and 5 draw.

#include "tash/utilities/outcome.hpp"

#include <oxbox/serialization/canned-value.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace tash::mcp::detail::json_rpc
{
  using utilities::Result;

  // Decode-only (oxbox serialization README), so nothing going out holds one.
  using RpcNode = oxbox::serialization::Canned;

  // One shape for all three; no id means a notification (JSON-RPC 2.0 4.1).
  struct RpcMessage
  {
    friend constexpr auto reflect_scheme(RpcMessage*);

    std::string                jsonrpc;
    std::optional<RpcNode>     id;
    std::optional<std::string> method;
    std::optional<RpcNode>     params;
  };

  struct RpcError
  {
    friend constexpr auto reflect_scheme(RpcError*);

    int                        code{ 0 };
    std::string                message;
    std::optional<std::string> data;
  };

  [[nodiscard]] auto MessageFrom(std::string_view body) -> Result<RpcMessage>;

  // As the wire spelled it: a quoted 1 and a bare 1 are different ids.
  [[nodiscard]] auto IdText(std::optional<RpcNode> const& id) -> std::string;

  [[nodiscard]] auto ResultMessage(std::string_view id,
                                   std::string_view result) -> std::string;

  [[nodiscard]] auto ErrorMessage(std::string_view id, RpcError const& failed)
    -> std::string;
}

namespace tash::mcp
{
  using detail::json_rpc::ErrorMessage;
  using detail::json_rpc::IdText;
  using detail::json_rpc::MessageFrom;
  using detail::json_rpc::ResultMessage;
  using detail::json_rpc::RpcError;
  using detail::json_rpc::RpcMessage;
  using detail::json_rpc::RpcNode;
}
