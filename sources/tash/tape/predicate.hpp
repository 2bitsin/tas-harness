#pragma once
// A predicate typed at a prompt rather than written into a tape: the same
// grammar a segment's anchor uses, and the words a verdict records it by.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/anchor-check.hpp"
#include "tash/tape/watch-source.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace tash::tape::detail::predicate
{
  using utilities::Result;

  // What one evaluation says: the answer, and the line that reports it.
  struct Judged
  {
    bool        passed{ false };
    std::string wording{ };

    auto operator == (Judged const&) const -> bool = default;
  };

  // An image anchor in a typed predicate resolves in the working directory.
  [[nodiscard]] auto CheckFrom(std::string_view text)
    -> Result<anchor_check::AnchorCheck>;

  [[nodiscard]] auto JudgedOn(anchor_check::AnchorCheck const& check,
                              bus::FrameView const& frame,
                              watch_source::WatchSource const* watches,
                              std::uint64_t at) -> Result<Judged>;

  // Steps until the check holds; a frame it cannot judge refuses first.
  [[nodiscard]] auto StepsUntil(anchor_check::AnchorCheck const& check,
                                session::Session& live,
                                watch_source::WatchSource const* watches,
                                std::uint64_t timeout_frames)
    -> Result<std::uint64_t>;
}

namespace tash::tape
{
  using detail::predicate::CheckFrom;
  using detail::predicate::Judged;
  using detail::predicate::JudgedOn;
  using detail::predicate::StepsUntil;
}
