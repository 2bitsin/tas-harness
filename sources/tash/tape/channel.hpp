#pragma once
// What a transition names: a button on a device, written `p1.start` or
// `m1.left`, whose mask is the bit the port's bitmap holds it in.

#include "tash/session/session.hpp"
#include "tash/tape/names.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tash::tape::detail::channel
{
  using utilities::Result;
  using names::Device;

  struct Port
  {
    std::size_t number{ 0 };
    Device      device{ Device::PAD };

    [[nodiscard]] constexpr auto operator == (Port const&) const noexcept
      -> bool = default;
  };

  struct Channel
  {
    std::size_t port{ 0 };
    unsigned    button{ 0 };
    Device      device{ Device::PAD };

    [[nodiscard]] constexpr auto Mask() const noexcept -> std::uint32_t
    {
      return std::uint32_t{ 1 } << button;
    }

    [[nodiscard]] constexpr auto operator == (Channel const&) const noexcept
      -> bool = default;
  };

  [[nodiscard]] auto PortFrom(std::string_view named) -> Result<Port>;

  [[nodiscard]] auto NameOf(Port const& port) -> std::string;

  [[nodiscard]] auto ChannelFrom(std::string_view named) -> Result<Channel>;

  [[nodiscard]] auto NameOf(Channel const& channel) -> std::string;
}

namespace tash::tape
{
  using detail::channel::Channel;
  using detail::channel::ChannelFrom;
  using detail::channel::NameOf;
  using detail::channel::Port;
  using detail::channel::PortFrom;
}
