#include "tash/clock/clock.hpp"

#include <thread>

namespace tash::clock::detail::clock
{
  Clock::Clock(Pacing pacing, double rate)
    : _pacing{ pacing }, _rate{ rate }
  {
  }

  auto Clock::Stepped() -> Clock
  {
    return Clock{ Pacing::Stepped, RATE_UNLIMITED };
  }

  auto Clock::Paced(double rate) -> Clock
  {
    return Clock{ Pacing::Paced, rate > 0.0 ? rate : RATE_UNLIMITED };
  }

  auto Clock::Start(double frame_seconds) -> void
  {
    _frame_seconds = frame_seconds;
    _started = std::chrono::steady_clock::now();
    _paced_at = _started;
    _paced_from = 0;
  }

  auto Clock::Await(std::uint64_t frames_done) const -> void
  {
    if (_pacing == Pacing::Stepped || _rate == RATE_UNLIMITED)
      return;
    double const due{ static_cast<double>(frames_done - _paced_from)
                      * _frame_seconds / _rate };
    Instant const at{ _paced_at
                      + std::chrono::duration_cast<Instant::duration>(
                          std::chrono::duration<double>{ due }) };
    std::this_thread::sleep_until(at);
  }

  auto Clock::Pace(double rate, std::uint64_t frames_done) -> void
  {
    _pacing = rate > 0.0 ? Pacing::Paced : Pacing::Stepped;
    _rate = rate > 0.0 ? rate : RATE_UNLIMITED;
    _paced_at = std::chrono::steady_clock::now();
    _paced_from = frames_done;
  }

  auto Clock::WallSeconds() const -> double
  {
    return std::chrono::duration<double>{
      std::chrono::steady_clock::now() - _started }.count();
  }

  auto Clock::AchievedRate(std::uint64_t frames_done) const -> double
  {
    double const wall{ WallSeconds() };
    if (wall <= 0.0 || _frame_seconds <= 0.0)
      return 0.0;
    return static_cast<double>(frames_done) * _frame_seconds / wall;
  }
}
