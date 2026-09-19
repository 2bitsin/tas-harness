#pragma once
// The one input every perception call takes: someone else's RGB565 pixels,
// borrowed. Plain data, so a frame ring, a file or a test all produce it.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/perception/region.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace tash::perception::detail::rgb565_view
{
  inline constexpr std::uint32_t RGB565_BYTES_PER_PIXEL{ 2u };

  struct Rgb565View
  {
    std::span<std::uint8_t const> bytes{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t pitch{};

    [[nodiscard]] constexpr auto RowBytes() const noexcept -> std::uint32_t
    {
      return width * RGB565_BYTES_PER_PIXEL;
    }

    [[nodiscard]] constexpr auto Empty() const noexcept -> bool
    {
      return width == 0u || height == 0u;
    }

    // False for anything a reader would run off the end of, and for an odd
    // pitch, which would put a row start on an unaligned std::uint16_t.
    [[nodiscard]] constexpr auto Valid() const noexcept -> bool
    {
      if (Empty() || pitch < RowBytes()
          || (pitch % RGB565_BYTES_PER_PIXEL) != 0u)
        return false;
      return bytes.size() >= std::size_t{ pitch } * (height - 1u) + RowBytes();
    }

    [[nodiscard]] constexpr auto Row(std::uint32_t y) const noexcept
      -> std::span<std::uint8_t const>
    {
      return bytes.subspan(std::size_t{ pitch } * y, RowBytes());
    }

    [[nodiscard]] auto Pixels(std::uint32_t y) const noexcept
      -> std::uint16_t const*
    {
      return reinterpret_cast<std::uint16_t const*>(Row(y).data());
    }

    // A window on the same pixels: same pitch, so nothing is copied and a
    // region call is the whole-frame call on the cropped view.
    [[nodiscard]] constexpr auto Crop(Region const& region) const noexcept
      -> Rgb565View
    {
      if (!region.FitsIn(width, height) || !Valid())
        return Rgb565View{};
      auto const offset{ std::size_t{ pitch } * region.y
                         + std::size_t{ region.x } * RGB565_BYTES_PER_PIXEL };
      return Rgb565View{ bytes.subspan(offset), region.width, region.height,
                         pitch };
    }
  };

  [[nodiscard]] inline auto ViewOf(std::uint16_t const* pixels,
                                   std::uint32_t width, std::uint32_t height,
                                   std::uint32_t pitch) noexcept -> Rgb565View
  {
    return Rgb565View{ std::span<std::uint8_t const>{
                         reinterpret_cast<std::uint8_t const*>(pixels),
                         std::size_t{ pitch } * height },
                       width, height, pitch };
  }

  // What the bus delivers, seen as pixels: every entry point takes one so
  // the session hands a slot straight over.
  [[nodiscard]] inline auto ViewOf(bus::FrameView const& frame) noexcept
    -> Rgb565View
  {
    return Rgb565View{ std::span<std::uint8_t const>{
                         reinterpret_cast<std::uint8_t const*>(
                           frame.pixels.data()),
                         frame.pixels.size() },
                       frame.descriptor.width, frame.descriptor.height,
                       static_cast<std::uint32_t>(frame.descriptor.pitch) };
  }
}

namespace tash::perception
{
  using detail::rgb565_view::RGB565_BYTES_PER_PIXEL;
  using detail::rgb565_view::Rgb565View;
  using detail::rgb565_view::ViewOf;
}
