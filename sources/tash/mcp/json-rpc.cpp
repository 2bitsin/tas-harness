#include "tash/mcp/json-rpc.hpp"

#include "tash/mcp/protocol.hpp"

#include <oxbox/serialization/io.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <format>
#include <variant>

namespace tash::mcp::detail::json_rpc
{
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view NO_ID{ "null" };
  }

  auto MessageFrom(std::string_view body) -> Result<RpcMessage>
  {
    RpcMessage message;
    try
    {
      message = oxbox::serialization::FromJson<RpcMessage>(body);
    }
    catch (std::exception const& failure)
    {
      return Refused("mcp: that is not a json-rpc message: {}",
                     failure.what());
    }
    if (message.jsonrpc != protocol::JSONRPC_VERSION)
      return Refused("mcp: json-rpc {} is the version, not '{}'",
                     protocol::JSONRPC_VERSION, message.jsonrpc);
    return message;
  }

  auto IdText(std::optional<RpcNode> const& id) -> std::string
  {
    if (!id)
      return std::string{ NO_ID };
    nlohmann::json const* const written{ std::get_if<nlohmann::json>(&*id) };
    if (written == nullptr || written->is_null())
      return std::string{ NO_ID };
    return written->dump();
  }

  auto ResultMessage(std::string_view id, std::string_view result)
    -> std::string
  {
    return std::format(R"({{"jsonrpc":"{}","id":{},"result":{}}})",
                       protocol::JSONRPC_VERSION, id, result);
  }

  auto ErrorMessage(std::string_view id, RpcError const& failed) -> std::string
  {
    return std::format(R"({{"jsonrpc":"{}","id":{},"error":{}}})",
                       protocol::JSONRPC_VERSION, id,
                       oxbox::serialization::ToJson(failed));
  }
}
