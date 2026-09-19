#pragma once
// The seam between a producer and the encoder thread. Bounded: a live
// producer at 60 Hz must never wait, so a full queue refuses the newest item
// unless the run asked it to block instead.

#include "tash/recorder/encoder-settings.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

namespace tash::recorder::detail::frame_queue
{
  struct VideoWork
  {
    std::vector<std::uint8_t> pixels;
    std::uint32_t             width{ 0 };
    std::uint32_t             height{ 0 };
    std::int64_t              at{ 0 };
  };

  struct AudioWork
  {
    std::vector<std::int16_t> samples;
    std::int64_t              position{ 0 };
  };

  using Work = std::variant<VideoWork, AudioWork>;

  class FrameQueue
  {
  public:
    explicit FrameQueue(std::size_t depth,
                        WhenFull when_full = WhenFull::DROP);

    // False when the item was refused: the queue was full under DROP, or
    // Finish() came while the push waited under WAIT.
    auto Push(Work work) -> bool;

    // Blocks until there is work; none once Finish() has drained it.
    [[nodiscard]] auto Pop() -> std::optional<Work>;

    auto Finish() -> void;

    [[nodiscard]] auto Size() const -> std::size_t;

  private:
    mutable std::mutex      _guard;
    std::condition_variable _filled;
    std::condition_variable _drained;
    std::deque<Work>        _work{ };
    std::size_t             _depth;
    WhenFull                _when_full;
    bool                    _finished{ false };
  };
}
