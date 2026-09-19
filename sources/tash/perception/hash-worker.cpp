#include "tash/perception/hash-worker.hpp"

#include "_checked-view.hpp"

#include <algorithm>
#include <utility>

namespace tash::perception::detail::hash_worker
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Forwarded;

  namespace
  {
    // The copy the worker owns: the visible pixels only, so the padding a
    // producer's pitch carries is neither stored nor hashed.
    [[nodiscard]] auto Compacted(Rgb565View const& frame)
      -> Result<std::vector<std::uint8_t>>
    {
      auto const checked{ detail::checked_view::Checked(frame) };
      if (!checked)
        return Forwarded(checked);
      std::vector<std::uint8_t> pixels(std::size_t{ frame.RowBytes() }
                                       * frame.height);
      for (std::uint32_t y{ 0 }; y < frame.height; ++y)
        std::ranges::copy(
          frame.Row(y),
          pixels.begin() + static_cast<std::ptrdiff_t>(frame.RowBytes()) * y);
      return pixels;
    }
  }

  HashWorker::HashWorker(std::size_t capacity)
  : _capacity{ capacity == 0u ? DEFAULT_QUEUE_CAPACITY : capacity },
    _thread{ [this] { Work(); } }
  {
  }

  HashWorker::~HashWorker()
  {
    {
      std::unique_lock lock{ _mutex };
      _progress.wait(lock,
                     [this] { return _jobs.empty() && _in_flight == 0u; });
      _stopping = true;
    }
    _arrival.notify_all();
    _thread.join();
  }

  auto HashWorker::Submit(std::uint64_t frame_number, Rgb565View const& frame)
    -> Outcome
  {
    auto pixels{ Compacted(frame) };
    if (!pixels)
      return Forwarded(pixels);
    {
      std::unique_lock lock{ _mutex };
      _room.wait(lock, [this] { return _jobs.size() < _capacity; });
      _jobs.push_back(
        Job{ frame_number, frame.width, frame.height, std::move(*pixels) });
    }
    _arrival.notify_one();
    return Outcome{};
  }

  auto HashWorker::TrySubmit(std::uint64_t frame_number,
                             Rgb565View const& frame) -> Result<bool>
  {
    auto pixels{ Compacted(frame) };
    if (!pixels)
      return Forwarded(pixels);
    {
      std::lock_guard lock{ _mutex };
      if (_jobs.size() >= _capacity)
        return false;
      _jobs.push_back(
        Job{ frame_number, frame.width, frame.height, std::move(*pixels) });
    }
    _arrival.notify_one();
    return true;
  }

  auto HashWorker::Drain() -> void
  {
    std::unique_lock lock{ _mutex };
    _progress.wait(lock, [this] { return _jobs.empty() && _in_flight == 0u; });
  }

  auto HashWorker::Take() -> std::optional<HashedFrame>
  {
    std::unique_lock lock{ _mutex };
    _progress.wait(lock, [this] {
      return !_results.empty() || (_jobs.empty() && _in_flight == 0u);
    });
    if (_results.empty())
      return std::nullopt;
    auto taken{ std::move(_results.front()) };
    _results.pop_front();
    return taken;
  }

  auto HashWorker::TryTake() -> std::optional<HashedFrame>
  {
    std::lock_guard lock{ _mutex };
    if (_results.empty())
      return std::nullopt;
    auto taken{ std::move(_results.front()) };
    _results.pop_front();
    return taken;
  }

  auto HashWorker::Pending() const -> std::size_t
  {
    std::lock_guard lock{ _mutex };
    return _jobs.size() + _in_flight;
  }

  auto HashWorker::Ready() const -> std::size_t
  {
    std::lock_guard lock{ _mutex };
    return _results.size();
  }

  auto HashWorker::Capacity() const noexcept -> std::size_t
  {
    return _capacity;
  }

  auto HashWorker::Viewed(Job const& job) noexcept -> Rgb565View
  {
    return Rgb565View{ job.pixels, job.width, job.height,
                       job.width * RGB565_BYTES_PER_PIXEL };
  }

  auto HashWorker::Work() -> void
  {
    for (;;)
    {
      Job job;
      {
        std::unique_lock lock{ _mutex };
        _arrival.wait(lock, [this] { return _stopping || !_jobs.empty(); });
        if (_jobs.empty())
          return;
        job = std::move(_jobs.front());
        _jobs.pop_front();
        ++_in_flight;
      }
      _room.notify_one();

      Rgb565View const view{ Viewed(job) };
      auto hashes{ HashesOf(view) };
      ChangeAmount change{};
      if (_earlier.width == job.width && _earlier.height == job.height
          && !_earlier.pixels.empty())
        if (Result<ChangeAmount> const between{
              ChangeBetween(Viewed(_earlier), view) };
            between)
          change = *between;
      std::uint64_t const number{ job.frame };
      _earlier = std::move(job);
      {
        std::lock_guard lock{ _mutex };
        _results.push_back(HashedFrame{ number, std::move(hashes), change });
        --_in_flight;
      }
      _progress.notify_all();
    }
  }
}
