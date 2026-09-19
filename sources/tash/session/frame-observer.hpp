#pragma once

#include "tash/bus/frame-descriptor.hpp"
#include "tash/session/line.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace tash::session::detail::frame_observer
{
  // The line a checkpoint brought: `at` is its last frame and `frame` the
  // picture there. This run ran none of them, so nothing else records it.
  struct Seed
  {
    std::uint64_t   at{ 0 };
    std::int64_t    harness_time{ 0 };
    bus::FrameView  frame{ };
  };

  // Where a restore put the run back: `at` is the frames it had made and
  // `place` where the checkpoint sits on the line it kept, nothing when the
  // state came from outside the run, which is when `line` carries one.
  struct Rewind
  {
    std::uint64_t            at{ 0 };
    std::optional<LinePlace> place{ };
    std::string_view         name{ };
    Line const*              line{ nullptr };
    std::optional<Seed>      seed{ };
  };

  // What a reset left: `at` is the frames the run had made, and no line.
  struct Reset
  {
    std::uint64_t at{ 0 };
  };

  // Called on the step thread the moment the core hands a frame over, with
  // pixels that stay valid only for the call.
  class FrameObserver
  {
  public:
    FrameObserver()                                        = default;
    virtual ~FrameObserver()                               = default;
    FrameObserver(FrameObserver const&)                    = delete;
    auto operator=(FrameObserver const&) -> FrameObserver& = delete;

    virtual auto OnFrame(bus::FrameView const& frame,
                         std::int64_t harness_time) -> void = 0;

    virtual auto OnRewind(Rewind const&) -> void { }

    virtual auto OnReset(Reset const& done) -> void
    { OnRewind(Rewind{ done.at, LinePlace{ }, "reset", nullptr, { } }); }
  };
}

namespace tash::session
{
  using detail::frame_observer::FrameObserver;
  using detail::frame_observer::Reset;
  using detail::frame_observer::Rewind;
  using detail::frame_observer::Seed;
}
