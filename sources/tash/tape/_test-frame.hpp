#pragma once
// Frames the tests own: RGB565 pixels in a vector, a pitch that carries
// padding the picture must never see, and a bus::FrameView over them.

#include "tash/bus/frame-descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tash::tape::testing
{
  class TestFrame
  {
  public:
    TestFrame(std::uint32_t width, std::uint32_t height,
              std::uint32_t padding_bytes = 0u)
    : _width{ width }, _height{ height },
      _pitch{ width * static_cast<std::uint32_t>(bus::BYTES_PER_PIXEL)
              + padding_bytes },
      _bytes(std::size_t{ _pitch } * height, std::uint8_t{ 0xCD })
    {
    }

    auto Set(std::uint32_t x, std::uint32_t y, std::uint16_t colour) -> void
    {
      auto const offset{ std::size_t{ _pitch } * y
                         + std::size_t{ x } * bus::BYTES_PER_PIXEL };
      _bytes[offset]     = static_cast<std::uint8_t>(colour & 0xFFu);
      _bytes[offset + 1] = static_cast<std::uint8_t>(colour >> 8);
    }

    // A moving diagonal band, so one frame differs from the next.
    auto Paint(std::uint32_t step) -> void
    {
      for (std::uint32_t y{ 0 }; y < _height; ++y)
        for (std::uint32_t x{ 0 }; x < _width; ++x)
          Set(x, y, static_cast<std::uint16_t>(((x + y + step) & 0x1Fu) << 11
                                               | ((y & 0x3Fu) << 5)
                                               | (x & 0x1Fu)));
    }

    [[nodiscard]] auto View(std::uint64_t number = 0u) const -> bus::FrameView
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
