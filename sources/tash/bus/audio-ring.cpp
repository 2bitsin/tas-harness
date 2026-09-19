#include "tash/bus/audio-ring.hpp"

#include <algorithm>

namespace tash::bus::detail::audio_ring
{
  auto AudioRing::Write(std::span<std::int16_t const> interleaved) -> void
  {
    for (std::int16_t const sample : interleaved)
    {
      if (_written - _read == _samples.size())
      {
        ++_read;
        ++_dropped;
      }
      _samples[_written % _samples.size()] = sample;
      ++_written;
    }
  }

  auto AudioRing::Read(std::span<std::int16_t> out) -> std::size_t
  {
    std::size_t const count{ std::min(out.size(), Available()) };
    for (std::size_t index{ 0 }; index < count; ++index)
      out[index] = _samples[(_read + index) % _samples.size()];
    _read += count;
    return count;
  }
}
