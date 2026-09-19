#pragma once

#include <cstdint>
#include <span>

namespace tash::session::detail::audio_observer
{
  // Called on the step thread as the core hands a block over, with samples
  // that stay valid only for the call: left, right, interleaved.
  class AudioObserver
  {
  public:
    AudioObserver()                                        = default;
    virtual ~AudioObserver()                               = default;
    AudioObserver(AudioObserver const&)                    = delete;
    auto operator=(AudioObserver const&) -> AudioObserver& = delete;

    virtual auto OnAudio(std::span<std::int16_t const> interleaved) -> void = 0;
  };
}

namespace tash::session
{
  using detail::audio_observer::AudioObserver;
}
