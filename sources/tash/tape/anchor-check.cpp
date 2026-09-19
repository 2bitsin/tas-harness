#include "tash/tape/anchor-check.hpp"

#include "tash/perception/colour-count.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/perception/hamming-distance.hpp"
#include "tash/perception/perceptual-hash.hpp"
#include "tash/perception/template-match.hpp"

#include <format>
#include <string>
#include <utility>
#include <vector>

namespace tash::tape::detail::anchor_check
{
  using anchor::AnchorKind;
  using utilities::Refused;

  namespace
  {
    [[nodiscard]] auto Within(anchor::Anchor const& predicate,
                              bus::FrameView const& frame)
      -> perception::Region
    {
      if (predicate.region)
        return predicate.region->Rectangle();
      return perception::Region{ 0u, 0u, frame.descriptor.width,
                                 frame.descriptor.height };
    }

    [[nodiscard]] auto NearEnough(Result<std::uint64_t> const& seen,
                                  std::uint64_t wanted, std::uint32_t most)
      -> Result<bool>
    {
      if (!seen)
        return utilities::Forwarded(seen);
      return perception::HammingDistance(*seen, wanted) <= most;
    }
  }

  AnchorCheck::AnchorCheck(anchor::Anchor predicate, std::uint64_t hash,
                           std::optional<anchor_image::AnchorImage> image,
                           std::vector<AnchorCheck> alternatives)
    : _predicate{ std::move(predicate) }, _hash{ hash },
      _image{ std::move(image) },
      _alternatives{ std::move(alternatives) }
  { }

  auto AnchorCheck::For(anchor::Anchor predicate,
                        std::filesystem::path const& tape)
    -> Result<AnchorCheck>
  {
    if (utilities::Outcome const sound{ anchor::Checked(predicate) }; !sound)
      return utilities::Forwarded(sound);

    std::uint64_t hash{ 0 };
    if (predicate.kind == AnchorKind::EXACT_HASH
        || predicate.kind == AnchorKind::DIFFERENCE_HASH
        || predicate.kind == AnchorKind::PERCEPTUAL_HASH)
    {
      Result<std::uint64_t> const read{
        anchor::HashFrom(predicate.HashText()) };
      if (!read)
        return utilities::Forwarded(read);
      hash = *read;
    }

    std::optional<anchor_image::AnchorImage> image{ };
    if (predicate.kind == AnchorKind::TEMPLATE_IMAGE)
    {
      Result<anchor_image::AnchorImage> read{ anchor_image::AnchorImage::Read(
        anchor_image::ResolvedImage(tape, predicate.ImagePath())) };
      if (!read)
        return utilities::Forwarded(read);
      image = std::move(*read);
    }

    std::vector<AnchorCheck> alternatives;
    if (predicate.kind == AnchorKind::ANY_OF)
      for (anchor::Anchor const& one : *predicate.any_of)
      {
        Result<AnchorCheck> made{ AnchorCheck::For(one, tape) };
        if (!made)
          return utilities::Forwarded(made);
        alternatives.push_back(std::move(*made));
      }
    return AnchorCheck{ std::move(predicate), hash, std::move(image),
                        std::move(alternatives) };
  }

  auto AnchorCheck::Immediate() const noexcept -> bool
  {
    return _predicate.kind == AnchorKind::NONE && !_predicate.Negated();
  }

  auto AnchorCheck::Holds(bus::FrameView const& frame,
                          watch_source::WatchSource const* watches) const
    -> Result<bool>
  {
    Result<bool> const held{ Matched(frame, watches) };
    if (!held)
      return utilities::Forwarded(held);
    return _predicate.Negated() ? !*held : *held;
  }

  auto AnchorCheck::Matched(bus::FrameView const& frame,
                            watch_source::WatchSource const* watches) const
    -> Result<bool>
  {
    perception::Region const within{ Within(_predicate, frame) };
    switch (_predicate.kind)
    {
      case AnchorKind::NONE:
        return true;

      case AnchorKind::EXACT_HASH:
      {
        Result<std::uint64_t> const seen{ perception::ExactHash(frame,
                                                                within) };
        if (!seen)
          return utilities::Forwarded(seen);
        return *seen == _hash;
      }

      case AnchorKind::DIFFERENCE_HASH:
        return NearEnough(perception::DifferenceHash(frame, within), _hash,
                          _predicate.MaxDistance());

      case AnchorKind::PERCEPTUAL_HASH:
        return NearEnough(perception::PerceptualHash(frame, within), _hash,
                          _predicate.MaxDistance());

      case AnchorKind::TEMPLATE_IMAGE:
      {
        Result<perception::TemplateMatch> const found{
          perception::BestMatch(frame, _image->View(), within) };
        if (!found)
          return utilities::Forwarded(found);
        return found->score >= _predicate.MinimumScore();
      }

      case AnchorKind::WATCH:
      {
        if (watches == nullptr)
          return Refused("tape: the watch '{}' has nobody to ask",
                         _predicate.WatchName());
        std::optional<std::int64_t> const value{
          watches->Value(_predicate.WatchName()) };
        if (!value)
          return Refused("tape: nobody samples the watch '{}'",
                         _predicate.WatchName());
        return anchor::Holds(_predicate.How(), *value, _predicate.Wanted());
      }

      case AnchorKind::COLOUR:
      {
        Result<std::uint64_t> const counted{ perception::ColourCount(
          frame, within, _predicate.WantedColour().Rgb(),
          _predicate.MaxDifference()) };
        if (!counted)
          return utilities::Forwarded(counted);
        return *counted >= _predicate.LeastCount();
      }

      case AnchorKind::ANY_OF:
      {
        // Every alternative is asked of this one frame, so which of them
        // holds first never changes the answer.
        bool any{ false };
        for (AnchorCheck const& one : _alternatives)
        {
          Result<bool> const held{ one.Holds(frame, watches) };
          if (!held)
            return utilities::Forwarded(held);
          any = any || *held;
        }
        return any;
      }
    }
    return Refused("tape: an anchor has no kind");
  }

  auto AnchorCheck::Wording() const -> std::string
  {
    std::string const said{ TermWording() };
    return _predicate.Negated() ? std::format("{} {}", anchor::NOT_WORD, said)
                                : said;
  }

  auto AnchorCheck::TermWording() const -> std::string
  {
    std::string_view const kind{ anchor::NameOf(_predicate.kind) };
    switch (_predicate.kind)
    {
      case AnchorKind::NONE:
        return std::string{ kind };
      case AnchorKind::EXACT_HASH:
        return std::format("{} {}", kind, anchor::HashText(_hash));
      case AnchorKind::DIFFERENCE_HASH:
      case AnchorKind::PERCEPTUAL_HASH:
        return std::format("{} {} within {}", kind, anchor::HashText(_hash),
                           _predicate.MaxDistance());
      case AnchorKind::TEMPLATE_IMAGE:
        return std::format("{} {} at {}", kind, _predicate.ImagePath(),
                           _predicate.MinimumScore());
      case AnchorKind::WATCH:
        return std::format("{} {} {} {}", kind, _predicate.WatchName(),
                           anchor::NameOf(_predicate.How()),
                           _predicate.Wanted());
      case AnchorKind::COLOUR:
      {
        anchor::AnchorColour const wanted{ _predicate.WantedColour() };
        anchor::AnchorRegion const over{
          _predicate.region.value_or(anchor::AnchorRegion{ }) };
        return std::format("{} {},{},{} within {} over {},{},{},{} at least"
                           " {}", kind, wanted.red, wanted.green, wanted.blue,
                           _predicate.MaxDifference(), over.x, over.y,
                           over.width, over.height, _predicate.LeastCount());
      }
      case AnchorKind::ANY_OF:
      {
        std::string said;
        for (AnchorCheck const& one : _alternatives)
          said += said.empty()
            ? one.Wording()
            : std::format(" {} {}", anchor::OR_WORD, one.Wording());
        return said;
      }
    }
    return std::string{ kind };
  }
}
