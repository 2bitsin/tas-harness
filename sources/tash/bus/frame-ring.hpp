#pragma once

#include "tash/bus/frame-descriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tash::bus::detail::frame_ring
{
  // design 2: eight slots, so a consumer may hold one frame and still look
  // back a few without the producer waiting on it.
  inline constexpr std::size_t FRAME_SLOTS{ 8 };

  class FrameRing
  {
  public:
    auto Push(FrameDescriptor const& descriptor,
              std::span<std::byte const> pixels) -> void;

    [[nodiscard]] auto Latest() const -> std::optional<FrameView>;

    // The oldest frame not taken yet; the ring is empty once it answers none.
    [[nodiscard]] auto Take() -> std::optional<FrameView>;

    [[nodiscard]] auto Written() const noexcept -> std::uint64_t
    { return _written; }

    // Frames overwritten before anything took them.
    [[nodiscard]] auto Dropped() const noexcept -> std::uint64_t
    { return _dropped; }

  private:
    struct Slot
    {
      FrameDescriptor descriptor;
      std::vector<std::byte> pixels;
      bool taken{ true };
    };

    [[nodiscard]] auto ViewOf(Slot const& slot) const -> FrameView;

    std::vector<Slot> _slots{ FRAME_SLOTS };
    std::uint64_t _written{ 0 };
    std::uint64_t _taken{ 0 };
    std::uint64_t _dropped{ 0 };
  };
}

namespace tash::bus
{
  using detail::frame_ring::FRAME_SLOTS;
  using detail::frame_ring::FrameRing;
}
