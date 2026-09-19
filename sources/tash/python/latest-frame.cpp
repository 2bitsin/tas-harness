#include "tash/python/latest-frame.hpp"

#include "tash/perception/change-amount.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/tape/channel.hpp"

#include <cstddef>
#include <span>
#include <utility>

namespace tash::python::detail::latest_frame
{
  using utilities::Forwarded;
  using utilities::Refused;

  namespace
  {
    auto ButtonsHeld(std::uint32_t pad) -> std::vector<std::string>
    {
      std::vector<std::string> held;
      for (unsigned button{ 0 }; button < tape::BUTTON_NAMES.size(); ++button)
        if ((pad & (std::uint32_t{ 1 } << button)) != 0u)
          held.emplace_back(tape::BUTTON_NAMES[button]);
      return held;
    }

    auto SameShape(bus::FrameDescriptor const& one,
                   bus::FrameDescriptor const& other) -> bool
    {
      return one.width == other.width && one.height == other.height
             && one.pitch == other.pitch;
    }

    auto ChangeBetween(bus::FrameKept const& before,
                       bus::FrameKept const& after)
      -> Result<perception::ChangeAmount>
    {
      if (before.pixels.empty()
          || !SameShape(before.descriptor, after.descriptor))
        return perception::ChangeAmount{ };
      return perception::ChangeBetween(perception::ViewOf(before.View()),
                                       perception::ViewOf(after.View()));
    }
  }

  auto LatestFrame::OnFrame(bus::FrameView const& frame, std::int64_t) -> void
  {
    Watched watched;
    if (_watches != nullptr)
      for (std::string const& name : _watches->Names())
        watched.emplace_back(name, _watches->Value(name));

    std::array<std::uint32_t, session::PORTS> pads{ };
    for (std::size_t port{ 0 }; port < session::PORTS; ++port)
      pads[port] = _live->Pad(port);

    std::lock_guard const holding{ _guard };
    _latest.descriptor = frame.descriptor;
    _latest.pixels.assign(frame.pixels.begin(), frame.pixels.end());
    _pads = pads;
    _watched = std::move(watched);
  }

  auto LatestFrame::Kept() const -> Result<bus::FrameKept>
  {
    std::lock_guard const holding{ _guard };
    if (_latest.pixels.empty())
      return Refused("python: the run has produced no frame yet");
    return _latest;
  }

  auto LatestFrame::Observed() -> Result<scenario_run::Observation>
  {
    bus::FrameKept taken;
    bus::FrameKept before;
    std::array<std::uint32_t, session::PORTS> pads{ };
    Watched watched;
    {
      std::lock_guard const holding{ _guard };
      if (_latest.pixels.empty())
        return Refused("python: the run has produced no frame yet");
      taken = _latest;
      pads = _pads;
      watched = _watched;
      before = std::exchange(_observed, _latest);
    }

    Result<perception::FrameHashes> const hashes{
      perception::HashesOf(taken.View()) };
    if (!hashes)
      return Forwarded(hashes);
    Result<perception::ChangeAmount> const change{
      ChangeBetween(before, taken) };
    if (!change)
      return Forwarded(change);

    scenario_run::Observation seen;

    // Frames are numbered from zero and the run counts one once it is made,
    // so the run had made this many when this frame came off the core.
    seen.frame = taken.descriptor.number + 1;
    seen.time = taken.descriptor.harness_seconds;
    seen.width = taken.descriptor.width;
    seen.height = taken.descriptor.height;
    seen.exact = hashes->exact;
    seen.difference = hashes->difference;
    seen.perceptual = hashes->perceptual;
    seen.change = change->changed_ratio;
    for (std::size_t port{ 0 }; port < session::PORTS; ++port)
      seen.pads[port] = ButtonsHeld(pads[port]);
    seen.watches = std::move(watched);
    return seen;
  }
}
