#pragma once
// Frames this module's tests own: RGB565 pixels in a vector, with a pitch
// wider than the picture so the padding stays out of every number.

#include "tash/bus/frame-descriptor.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace tash::journal::testing
{
  inline constexpr std::uint32_t BYTES_PER_PIXEL{ 2u };
  inline constexpr std::uint32_t FRAME_WIDTH{ 32u };
  inline constexpr std::uint32_t FRAME_HEIGHT{ 24u };
  inline constexpr std::uint32_t FRAME_PADDING{ 8u };

  class SyntheticFrame
  {
  public:
    explicit SyntheticFrame(std::uint16_t colour)
    : _bytes(std::size_t{ _pitch } * FRAME_HEIGHT, std::uint8_t{ 0xCD })
    {
      Fill(colour);
    }

    auto Fill(std::uint16_t colour) -> void
    {
      for (std::uint32_t y{ 0 }; y < FRAME_HEIGHT; ++y)
        for (std::uint32_t x{ 0 }; x < FRAME_WIDTH; ++x)
          Set(x, y, colour);
    }

    auto Set(std::uint32_t x, std::uint32_t y, std::uint16_t colour) -> void
    {
      auto const offset{ std::size_t{ _pitch } * y
                         + std::size_t{ x } * BYTES_PER_PIXEL };
      _bytes[offset]     = static_cast<std::uint8_t>(colour & 0xFFu);
      _bytes[offset + 1] = static_cast<std::uint8_t>(colour >> 8);
    }

    [[nodiscard]] auto View(std::uint64_t number,
                            double harness_seconds) const -> bus::FrameView
    {
      return bus::FrameView{
        bus::FrameDescriptor{ number, FRAME_WIDTH, FRAME_HEIGHT, _pitch,
                              harness_seconds },
        std::as_bytes(std::span{ _bytes })
      };
    }

  private:
    std::uint32_t             _pitch{ FRAME_WIDTH * BYTES_PER_PIXEL
                                      + FRAME_PADDING };
    std::vector<std::uint8_t> _bytes;
  };
}
