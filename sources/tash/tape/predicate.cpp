#include "tash/tape/predicate.hpp"

#include "tash/tape/anchor.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <string>

namespace tash::tape::detail::predicate
{
  namespace
  {
    // The file name AnchorCheck::For resolves an image anchor beside,
    // standing in for the tape a typed predicate does not have.
    inline constexpr std::string_view TYPED_HERE{ "predicate" };
  }

  auto CheckFrom(std::string_view text) -> Result<anchor_check::AnchorCheck>
  {
    Result<anchor::Anchor> const asked{ anchor::AnchorFrom(text) };
    if (!asked)
      return utilities::Forwarded(asked);
    return anchor_check::AnchorCheck::For(
      *asked, std::filesystem::current_path() / TYPED_HERE);
  }

  auto JudgedOn(anchor_check::AnchorCheck const& check,
                bus::FrameView const& frame,
                watch_source::WatchSource const* watches, std::uint64_t at)
    -> Result<Judged>
  {
    Result<bool> const holds{ check.Holds(frame, watches) };
    if (!holds)
      return utilities::Forwarded(holds);
    return Judged{ *holds,
                   std::format("{} {} at frame {}", check.Wording(),
                               *holds ? "holds" : "does not hold", at) };
  }

  auto StepsUntil(anchor_check::AnchorCheck const& check,
                  session::Session& live,
                  watch_source::WatchSource const* watches,
                  std::uint64_t timeout_frames) -> Result<std::uint64_t>
  {
    std::string refusal;
    auto const holds{ [&]
    {
      std::optional<bus::FrameView> const latest{ live.Video().Latest() };
      if (!latest)
        return false;
      Result<bool> const held{ check.Holds(*latest, watches) };
      if (!held)
      {
        refusal = held.error();
        return false;
      }
      return *held;
    } };

    Result<std::uint64_t> const took{ live.RunUntil(holds, timeout_frames) };
    if (!refusal.empty())
      return std::unexpected{ refusal };
    if (!took)
      return utilities::Refused("tape: {} did not hold within {} frames",
                                check.Wording(), timeout_frames);
    return *took;
  }
}
