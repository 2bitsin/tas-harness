#pragma once
// Frames made, counted where a second thread may read the number while the
// first is still making them: a detached job's progress.

#include "tash/session/frame-observer.hpp"

#include <atomic>
#include <cstdint>

namespace tash::cli::detail::frame_count
{
  class FrameCount final : public session::FrameObserver
  {
  public:
    auto OnFrame(bus::FrameView const&, std::int64_t) -> void override
    { _frames.fetch_add(1, std::memory_order_relaxed); }

    // Never rewound: a restore puts the run back, not the work it did.
    [[nodiscard]] auto Made() const noexcept -> std::uint64_t
    { return _frames.load(std::memory_order_relaxed); }

  private:
    std::atomic<std::uint64_t> _frames{ 0 };
  };
}

namespace tash::cli
{
  using detail::frame_count::FrameCount;
}
