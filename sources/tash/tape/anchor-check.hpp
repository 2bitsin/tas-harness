#pragma once
// An anchor made ready to ask: the hash parsed, the crop loaded, and one
// call per frame that answers whether the segment may start.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/tape/anchor-image.hpp"
#include "tash/tape/anchor.hpp"
#include "tash/tape/watch-source.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tash::tape::detail::anchor_check
{
  using utilities::Result;

  class AnchorCheck
  {
  public:
    // `tape` is the tape's own path: an image anchor is relative to it.
    [[nodiscard]] static auto For(anchor::Anchor predicate,
                                  std::filesystem::path const& tape)
      -> Result<AnchorCheck>;

    [[nodiscard]] auto Holds(bus::FrameView const& frame,
                             watch_source::WatchSource const* watches) const
      -> Result<bool>;

    // True for an anchor that never waits, which is how a tape starts from
    // power on.
    [[nodiscard]] auto Immediate() const noexcept -> bool;

    // The predicate as a line of text, for the refusal a timeout writes.
    [[nodiscard]] auto Wording() const -> std::string;

  private:
    AnchorCheck(anchor::Anchor predicate, std::uint64_t hash,
                std::optional<anchor_image::AnchorImage> image,
                std::vector<AnchorCheck> alternatives);

    // The term's own answer, before `not` turns it over.
    [[nodiscard]] auto Matched(bus::FrameView const& frame,
                               watch_source::WatchSource const* watches) const
      -> Result<bool>;

    [[nodiscard]] auto TermWording() const -> std::string;

    anchor::Anchor                           _predicate;
    std::uint64_t                            _hash{ 0 };
    std::optional<anchor_image::AnchorImage> _image{ };
    std::vector<AnchorCheck>                 _alternatives{ };
  };
}

namespace tash::tape
{
  using detail::anchor_check::AnchorCheck;
}
