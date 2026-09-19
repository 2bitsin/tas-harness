#pragma once
// Frames the tests own: pixels in a vector, a view over them, and a pitch
// that can carry padding the picture must never see.

#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"

#include "tash/utilities/outcome.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <vector>

namespace tash::perception::testing
{
  using utilities::Result;

  inline constexpr std::uint16_t RGB565_BLACK{ 0x0000 };
  inline constexpr std::uint16_t RGB565_WHITE{ 0xFFFF };
  inline constexpr std::uint32_t RAMP_LEVELS{ 32u };

  // A refusal fails the expectation here rather than at every call site; the
  // default that follows keeps the test readable to its own end.
  template <typename Value>
  [[nodiscard]] auto Held(Result<Value> const& result) -> Value
  {
    EXPECT_TRUE(result.has_value())
      << (result ? std::string{} : result.error());
    return result.value_or(Value{});
  }

  class SyntheticFrame
  {
  public:
    SyntheticFrame(std::uint32_t width, std::uint32_t height,
                   std::uint32_t padding_bytes = 0u)
    : _width{ width }, _height{ height },
      _pitch{ width * RGB565_BYTES_PER_PIXEL + padding_bytes },
      _bytes(std::size_t{ _pitch } * height, std::uint8_t{ 0xCD })
    {
    }

    auto Fill(std::uint16_t colour) -> void
    {
      for (std::uint32_t y{ 0 }; y < _height; ++y)
        for (std::uint32_t x{ 0 }; x < _width; ++x)
          Set(x, y, colour);
    }

    auto Set(std::uint32_t x, std::uint32_t y, std::uint16_t colour) -> void
    {
      auto const offset{ std::size_t{ _pitch } * y
                         + std::size_t{ x } * RGB565_BYTES_PER_PIXEL };
      _bytes[offset]     = static_cast<std::uint8_t>(colour & 0xFFu);
      _bytes[offset + 1] = static_cast<std::uint8_t>(colour >> 8);
    }

    auto FillRectangle(Region const& region, std::uint16_t colour) -> void
    {
      for (std::uint32_t y{ 0 }; y < region.height; ++y)
        for (std::uint32_t x{ 0 }; x < region.width; ++x)
          Set(region.x + x, region.y + y, colour);
    }

    // A horizontal ramp between two 5-bit levels; grey comes out at level * 8,
    // so the direction of the ramp is the direction of the brightness.
    auto FillRamp(std::uint32_t from_level, std::uint32_t to_level) -> void
    {
      for (std::uint32_t x{ 0 }; x < _width; ++x)
      {
        auto const across{ x * (RAMP_LEVELS - 1u) / (_width - 1u) };
        auto const level{ from_level < to_level ? from_level + across
                                                : from_level - across };
        for (std::uint32_t y{ 0 }; y < _height; ++y)
          Set(x, y, GreyLevel(level));
      }
    }

    [[nodiscard]] static constexpr auto GreyLevel(std::uint32_t level) noexcept
      -> std::uint16_t
    {
      return static_cast<std::uint16_t>((level << 11) | ((level * 2u) << 5)
                                        | level);
    }

    [[nodiscard]] auto View() const -> Rgb565View
    {
      return Rgb565View{ _bytes, _width, _height, _pitch };
    }

    [[nodiscard]] auto BusView(std::uint64_t number = 0u) const
      -> bus::FrameView
    {
      return bus::FrameView{
        bus::FrameDescriptor{ number, _width, _height, _pitch, 0.0 },
        std::as_bytes(std::span{ _bytes })
      };
    }

  private:
    std::uint32_t             _width;
    std::uint32_t             _height;
    std::uint32_t             _pitch;
    std::vector<std::uint8_t> _bytes;
  };
}
