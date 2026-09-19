#include "tash/tape/channel.hpp"

#include "_lines.hpp"

#include <algorithm>
#include <format>

namespace tash::tape::detail::channel
{
  using utilities::Refused;

  namespace
  {
    [[nodiscard]] auto ButtonOf(std::string_view named, Device device)
      -> Result<unsigned>
    {
      auto const names{ ButtonsOf(device) };
      auto const found{ std::ranges::find(names, named) };
      if (found == names.end())
        return Refused("tape: '{}' is not a {} button", named,
                       device == Device::MOUSE ? "mouse" : "pad");
      return static_cast<unsigned>(found - names.begin());
    }

    [[nodiscard]] auto Ports(Device device) noexcept -> std::size_t
    {
      return device == Device::MOUSE ? MICE : session::PORTS;
    }
  }

  auto PortFrom(std::string_view named) -> Result<Port>
  {
    Device const device{ !named.empty() && named.front() == MOUSE_PREFIX
                           ? Device::MOUSE : Device::PAD };
    if (named.size() >= 2
        && (named.front() == PORT_PREFIX || named.front() == MOUSE_PREFIX))
    {
      Result<std::size_t> const number{
        lines::NumberOf<std::size_t>(named.substr(1)) };
      if (number && *number >= 1 && *number <= Ports(device))
        return Port{ *number - 1, device };
    }
    return Refused("tape: '{}' is not a port, which is p1 to p{} or m1 to m{}",
                   named, session::PORTS, MICE);
  }

  auto NameOf(Port const& port) -> std::string
  {
    return std::format("{}{}", PrefixOf(port.device), port.number + 1);
  }

  auto ChannelFrom(std::string_view named) -> Result<Channel>
  {
    auto const dot{ named.find(CHANNEL_SEPARATOR) };
    if (dot == std::string_view::npos)
      return Refused("tape: '{}' is not a channel, which is <port>.<button>",
                     named);

    Result<Port> const port{ PortFrom(named.substr(0, dot)) };
    if (!port)
      return utilities::Forwarded(port);

    Result<unsigned> const button{ ButtonOf(named.substr(dot + 1),
                                            port->device) };
    if (!button)
      return utilities::Forwarded(button);

    return Channel{ port->number, *button, port->device };
  }

  auto NameOf(Channel const& channel) -> std::string
  {
    Port const port{ channel.port, channel.device };
    auto const names{ ButtonsOf(channel.device) };
    if (channel.button >= names.size())
      return std::format("{}{}?", NameOf(port), CHANNEL_SEPARATOR);
    return std::format("{}{}{}", NameOf(port), CHANNEL_SEPARATOR,
                       names[channel.button]);
  }
}
