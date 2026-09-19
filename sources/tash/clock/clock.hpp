#pragma once

#include <chrono>
#include <cstdint>

namespace tash::clock::detail::clock
{
  // A multiple of the emulated machine's own rate; zero is "as fast as it runs".
  inline constexpr double RATE_UNLIMITED{ 0.0 };
  inline constexpr double RATE_REAL_TIME{ 1.0 };

  enum class Pacing
  {
    Stepped,  // nothing advances but a Step or RunUntil call
    Paced,    // frames are released on a wall clock schedule
  };

  class Clock
  {
  public:
    static auto Stepped() -> Clock;
    static auto Paced(double rate) -> Clock;

    // `frame_seconds` is one emulated frame, from the core's av info.
    auto Start(double frame_seconds) -> void;

    // Returns once frame `frames_done` may be produced.
    auto Await(std::uint64_t frames_done) const -> void;

    // A new rate from `frames_done` on; the wall baseline stays where it
    // was, so what the run reports about itself still covers the run.
    auto Pace(double rate, std::uint64_t frames_done) -> void;

    [[nodiscard]] auto WallSeconds() const -> double;

    // Emulated seconds produced per wall second.
    [[nodiscard]] auto AchievedRate(std::uint64_t frames_done) const -> double;

    [[nodiscard]] auto Mode() const noexcept -> Pacing { return _pacing; }
    [[nodiscard]] auto Rate() const noexcept -> double { return _rate; }
    [[nodiscard]] auto FrameSeconds() const noexcept -> double
    { return _frame_seconds; }

  private:
    Clock(Pacing pacing, double rate);

    using Instant = std::chrono::steady_clock::time_point;

    Pacing _pacing;
    double _rate;
    double _frame_seconds{ 0.0 };
    Instant _started{ std::chrono::steady_clock::now() };
    Instant _paced_at{ _started };
    std::uint64_t _paced_from{ 0 };
  };
}

namespace tash::clock
{
  using detail::clock::Clock;
  using detail::clock::Pacing;
  using detail::clock::RATE_REAL_TIME;
  using detail::clock::RATE_UNLIMITED;
}
