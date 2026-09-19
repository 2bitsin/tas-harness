#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace tash::libretro::detail::core_sink
{
  // What a running core hands out and asks for, once per frame.
  class CoreSink
  {
  public:
    virtual ~CoreSink() = default;

    virtual auto PushVideo(std::span<std::byte const> pixels, std::uint32_t width,
                       std::uint32_t height, std::size_t pitch) -> void = 0;

    virtual auto PushAudio(std::span<std::int16_t const> interleaved) -> void = 0;

    virtual auto PollInput() -> void = 0;

    virtual auto InputState(unsigned port, unsigned device, unsigned index,
                            unsigned id) -> std::int16_t = 0;
  };
}

namespace tash::libretro
{
  using detail::core_sink::CoreSink;
}
