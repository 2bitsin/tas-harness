#include "tash/recorder/_frame-queue.hpp"

#include <utility>

namespace tash::recorder::detail::frame_queue
{
  FrameQueue::FrameQueue(std::size_t depth, WhenFull when_full)
  : _depth{ depth == 0u ? 1u : depth }, _when_full{ when_full }
  {
  }

  auto FrameQueue::Push(Work work) -> bool
  {
    std::unique_lock held{ _guard };
    if (_when_full == WhenFull::WAIT)
      _drained.wait(held,
                    [this] { return _finished || _work.size() < _depth; });
    if (_finished || _work.size() >= _depth)
      return false;
    _work.push_back(std::move(work));
    _filled.notify_one();
    return true;
  }

  auto FrameQueue::Pop() -> std::optional<Work>
  {
    std::unique_lock held{ _guard };
    _filled.wait(held, [this] { return _finished || !_work.empty(); });
    if (_work.empty())
      return { };
    Work taken{ std::move(_work.front()) };
    _work.pop_front();
    _drained.notify_one();
    return taken;
  }

  auto FrameQueue::Finish() -> void
  {
    std::lock_guard const held{ _guard };
    _finished = true;
    _filled.notify_all();
    _drained.notify_all();
  }

  auto FrameQueue::Size() const -> std::size_t
  {
    std::lock_guard const held{ _guard };
    return _work.size();
  }
}
