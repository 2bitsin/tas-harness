#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tash::bus::detail::audio_ring
{
  // A second of stereo at 48 kHz: enough that a consumer reading once a frame
  // never loses a sample, and small enough to keep in the run's memory.
  inline constexpr std::size_t AUDIO_CAPACITY_FRAMES{ 48000 };

  inline constexpr std::size_t AUDIO_CHANNELS{ 2 };

  class AudioRing
  {
  public:
    // `interleaved` is what the core's batch callback hands over: left, right.
    auto Write(std::span<std::int16_t const> interleaved) -> void;

    // Fills `out` with the oldest samples and answers how many it wrote.
    [[nodiscard]] auto Read(std::span<std::int16_t> out) -> std::size_t;

    [[nodiscard]] auto Available() const noexcept -> std::size_t
    { return static_cast<std::size_t>(_written - _read); }

    [[nodiscard]] auto Written() const noexcept -> std::uint64_t
    { return _written; }

    [[nodiscard]] auto Dropped() const noexcept -> std::uint64_t
    { return _dropped; }

  private:
    std::vector<std::int16_t> _samples =
      std::vector<std::int16_t>(AUDIO_CAPACITY_FRAMES * AUDIO_CHANNELS);
    std::uint64_t _written{ 0 };
    std::uint64_t _read{ 0 };
    std::uint64_t _dropped{ 0 };
  };
}

namespace tash::bus
{
  using detail::audio_ring::AUDIO_CAPACITY_FRAMES;
  using detail::audio_ring::AUDIO_CHANNELS;
  using detail::audio_ring::AudioRing;
}
