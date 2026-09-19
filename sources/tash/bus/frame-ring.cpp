#include "tash/bus/frame-ring.hpp"

#include <algorithm>

namespace tash::bus::detail::frame_ring
{
  auto FrameRing::Push(FrameDescriptor const& descriptor,
                       std::span<std::byte const> pixels) -> void
  {
    Slot& slot{ _slots[_written % FRAME_SLOTS] };
    if (!slot.taken)
      ++_dropped;
    slot.descriptor = descriptor;
    slot.pixels.assign(pixels.begin(), pixels.end());
    slot.taken = false;
    ++_written;
    // The reader is this far behind at most, so what it takes next exists.
    _taken = std::max(_taken, _written - std::min<std::uint64_t>(
                                _written, FRAME_SLOTS));
  }

  auto FrameRing::ViewOf(Slot const& slot) const -> FrameView
  {
    return FrameView{ slot.descriptor, std::span{ slot.pixels } };
  }

  auto FrameRing::Latest() const -> std::optional<FrameView>
  {
    if (_written == 0)
      return std::nullopt;
    return ViewOf(_slots[(_written - 1) % FRAME_SLOTS]);
  }

  auto FrameRing::Take() -> std::optional<FrameView>
  {
    if (_taken >= _written)
      return std::nullopt;
    Slot& slot{ _slots[_taken % FRAME_SLOTS] };
    slot.taken = true;
    ++_taken;
    return ViewOf(slot);
  }
}
