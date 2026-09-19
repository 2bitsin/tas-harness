#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tash::bus::detail::frame_descriptor
{
  // What the video callback delivers: RGB565 rows, as wide as `pitch` and as
  // many as `height`, of which `width` pixels per row are the picture.
  struct FrameDescriptor
  {
    std::uint64_t number{ 0 };
    std::uint32_t width{ 0 };
    std::uint32_t height{ 0 };
    std::size_t pitch{ 0 };
    double harness_seconds{ 0.0 };
  };

  struct FrameView
  {
    FrameDescriptor descriptor;
    std::span<std::byte const> pixels;
  };

  // A frame that owns its pixels: what a reader carries away from the ring
  // its producer keeps refilling.
  struct FrameKept
  {
    FrameDescriptor        descriptor{ };
    std::vector<std::byte> pixels{ };

    [[nodiscard]] auto View() const -> FrameView
    { return FrameView{ descriptor, pixels }; }
  };

  inline constexpr std::size_t BYTES_PER_PIXEL{ 2 };
}

namespace tash::bus
{
  using detail::frame_descriptor::BYTES_PER_PIXEL;
  using detail::frame_descriptor::FrameDescriptor;
  using detail::frame_descriptor::FrameKept;
  using detail::frame_descriptor::FrameView;
}
