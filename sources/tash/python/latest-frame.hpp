#pragma once
// The latest completed frame, published under a lock: what a reader takes
// while the step thread of a detached python job is still making frames.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/python/scenario-run.hpp"
#include "tash/python/watch-values.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/session/session.hpp"
#include "tash/utilities/outcome.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tash::python::detail::latest_frame
{
  using utilities::Result;

  class LatestFrame final : public session::FrameObserver
  {
  public:
    LatestFrame(session::Session const& live, WatchValues const* watches)
    : _live{ &live }, _watches{ watches } { }

    auto OnFrame(bus::FrameView const& frame, std::int64_t) -> void override;

    [[nodiscard]] auto Kept() const -> Result<bus::FrameKept>;

    // Its change is measured against the frame the call before answered,
    // which is the reader's own run of observations, not the scenario's.
    [[nodiscard]] auto Observed() -> Result<scenario_run::Observation>;

  private:
    using Watched
      = std::vector<std::pair<std::string, std::optional<std::int64_t>>>;

    session::Session const*                   _live;
    WatchValues const*                        _watches;

    // Everything below is written by the step thread and read by whoever
    // asks, so neither touches it outside this lock.
    mutable std::mutex                        _guard{ };
    bus::FrameKept                            _latest{ };
    std::array<std::uint32_t, session::PORTS> _pads{ };
    Watched                                   _watched{ };
    bus::FrameKept                            _observed{ };
  };
}

namespace tash::python
{
  using detail::latest_frame::LatestFrame;
}
