#pragma once
// The tape's vocabulary: the words and marks its lines are written with,
// in one header because libtash writes the same lines without linking any
// of this.

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace tash::tape::detail::names
{
  // The libretro joypad ids, by their RETRO_DEVICE_ID_JOYPAD value, which is
  // the bit each one holds in a port's bitmap.
  inline constexpr std::array<std::string_view, 16> BUTTON_NAMES{
    "b", "y", "select", "start", "up", "down", "left", "right",
    "a", "x", "l", "r", "l2", "r2", "l3", "r3"
  };

  // A pointer's buttons, in the order of the bits they hold; a wheel notch
  // is a button held for the frame it turned in.
  inline constexpr std::array<std::string_view, 5> MOUSE_BUTTON_NAMES{
    "left", "right", "middle", "wheel_up", "wheel_down"
  };

  inline constexpr std::size_t MICE{ 2 };

  inline constexpr char PORT_PREFIX{ 'p' };
  inline constexpr char MOUSE_PREFIX{ 'm' };
  inline constexpr char CHANNEL_SEPARATOR{ '.' };
  inline constexpr char POINT_SEPARATOR{ ',' };
  inline constexpr char COMMENT_MARK{ '#' };

  inline constexpr std::string_view DOWN_WORD{ "down" };
  inline constexpr std::string_view UP_WORD{ "up" };

  // What the one segment of a recording that marked nothing is called.
  inline constexpr std::string_view FIRST_SEGMENT_NAME{ "start" };

  enum class Device
  {
    PAD,
    MOUSE
  };

  [[nodiscard]] constexpr auto PrefixOf(Device device) noexcept -> char
  {
    return device == Device::MOUSE ? MOUSE_PREFIX : PORT_PREFIX;
  }

  [[nodiscard]] constexpr auto ButtonsOf(Device device) noexcept
    -> std::span<std::string_view const>
  {
    return device == Device::MOUSE
             ? std::span<std::string_view const>{ MOUSE_BUTTON_NAMES }
             : std::span<std::string_view const>{ BUTTON_NAMES };
  }
}

namespace tash::tape
{
  using detail::names::BUTTON_NAMES;
  using detail::names::ButtonsOf;
  using detail::names::CHANNEL_SEPARATOR;
  using detail::names::COMMENT_MARK;
  using detail::names::Device;
  using detail::names::DOWN_WORD;
  using detail::names::FIRST_SEGMENT_NAME;
  using detail::names::MICE;
  using detail::names::MOUSE_BUTTON_NAMES;
  using detail::names::MOUSE_PREFIX;
  using detail::names::POINT_SEPARATOR;
  using detail::names::PORT_PREFIX;
  using detail::names::PrefixOf;
  using detail::names::UP_WORD;
}
