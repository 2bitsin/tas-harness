#pragma once
// What a segment waits for: one predicate over the frame the session last
// produced, or over a watch somebody else samples.

#include "tash/perception/colour.hpp"
#include "tash/perception/region.hpp"
#include "tash/utilities/outcome.hpp"

#include <_buildutil/reflect.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::anchor
{
  using utilities::Outcome;
  using utilities::Result;

  // The tape's own rectangle: perception::Region carries no scheme, and the
  // recorder's VerdictLine settled that the file's shape is the file's.
  struct AnchorRegion
  {
    friend constexpr auto reflect_scheme(AnchorRegion*);

    std::uint32_t x{ 0 };
    std::uint32_t y{ 0 };
    std::uint32_t width{ 0 };
    std::uint32_t height{ 0 };

    [[nodiscard]] constexpr auto Rectangle() const noexcept
      -> perception::Region
    {
      return perception::Region{ x, y, width, height };
    }

    auto operator == (AnchorRegion const&) const -> bool = default;
  };

  // The colour a count is taken of, in eight bits a channel.
  struct AnchorColour
  {
    friend constexpr auto reflect_scheme(AnchorColour*);

    std::uint32_t red{ 0 };
    std::uint32_t green{ 0 };
    std::uint32_t blue{ 0 };

    [[nodiscard]] constexpr auto Rgb() const noexcept -> perception::Colour
    {
      return perception::Colour{ red, green, blue };
    }

    auto operator == (AnchorColour const&) const -> bool = default;
  };

  enum class AnchorKind
  {
    NONE            _Label(none),
    EXACT_HASH      _Label(exact_hash),
    DIFFERENCE_HASH _Label(difference_hash),
    PERCEPTUAL_HASH _Label(perceptual_hash),
    TEMPLATE_IMAGE  _Label(template_image),
    WATCH           _Label(watch),
    COLOUR          _Label(colour),
    ANY_OF          _Label(any_of)
  };

  constexpr auto reflect_scheme(AnchorKind*);

  enum class Comparison
  {
    EQUAL            _Label(equal),
    NOT_EQUAL        _Label(not_equal),
    LESS             _Label(less),
    LESS_OR_EQUAL    _Label(less_or_equal),
    GREATER          _Label(greater),
    GREATER_OR_EQUAL _Label(greater_or_equal)
  };

  constexpr auto reflect_scheme(Comparison*);

  // Design 7's title anchor: four bits of a 64 bit perceptual hash, which is
  // a frame that differs in dithering and not in what it shows.
  inline constexpr std::uint32_t DEFAULT_MAX_DISTANCE{ 4 };

  // Normalised cross-correlation runs -1 to 1; a crop of the frame itself
  // scores 1, and anything under this is a different screen.
  inline constexpr double DEFAULT_MINIMUM_SCORE{ 0.9 };

  inline constexpr std::size_t HASH_DIGITS{ 16 };

  // A ghost's hatching is drawn from a palette rather than blended, so the
  // colour a tape names is the colour on the screen.
  inline constexpr std::uint32_t DEFAULT_MAX_DIFFERENCE{ 0 };

  inline constexpr std::uint64_t DEFAULT_LEAST_COUNT{ 1 };

  // Everything but the kind is absent from the wire unless it was written:
  // a tape is a file a human edits, and a field a kind does not use has no
  // business being in it.
  struct Anchor
  {
    friend constexpr auto reflect_scheme(Anchor*);

    AnchorKind                   kind{ AnchorKind::NONE };
    std::optional<AnchorRegion>  region{ };
    std::optional<std::string>   hash{ };
    std::optional<std::uint32_t> max_distance{ };
    std::optional<std::string>   image{ };
    std::optional<double>        minimum_score{ };
    std::optional<std::string>   watch{ };
    std::optional<Comparison>    comparison{ };
    std::optional<std::int64_t>  value{ };
    std::optional<AnchorColour>  colour{ };
    std::optional<std::uint32_t> max_difference{ };
    std::optional<std::uint64_t> count{ };
    std::optional<bool>          negated{ };

    // The alternatives an any_of holds; std::vector tolerates the anchor
    // being incomplete here, which std::optional<Anchor> would not.
    std::optional<std::vector<Anchor>> any_of{ };

    [[nodiscard]] auto HashText() const -> std::string_view
    { return hash ? std::string_view{ *hash } : std::string_view{ }; }

    [[nodiscard]] auto ImagePath() const -> std::string_view
    { return image ? std::string_view{ *image } : std::string_view{ }; }

    [[nodiscard]] auto WatchName() const -> std::string_view
    { return watch ? std::string_view{ *watch } : std::string_view{ }; }

    [[nodiscard]] auto MaxDistance() const noexcept -> std::uint32_t
    { return max_distance.value_or(DEFAULT_MAX_DISTANCE); }

    [[nodiscard]] auto MinimumScore() const noexcept -> double
    { return minimum_score.value_or(DEFAULT_MINIMUM_SCORE); }

    [[nodiscard]] auto How() const noexcept -> Comparison
    { return comparison.value_or(Comparison::EQUAL); }

    [[nodiscard]] auto Wanted() const noexcept -> std::int64_t
    { return value.value_or(0); }

    [[nodiscard]] auto WantedColour() const noexcept -> AnchorColour
    { return colour.value_or(AnchorColour{ }); }

    [[nodiscard]] auto MaxDifference() const noexcept -> std::uint32_t
    { return max_difference.value_or(DEFAULT_MAX_DIFFERENCE); }

    [[nodiscard]] auto LeastCount() const noexcept -> std::uint64_t
    { return count.value_or(DEFAULT_LEAST_COUNT); }

    [[nodiscard]] auto Negated() const noexcept -> bool
    { return negated.value_or(false); }

    auto operator == (Anchor const&) const -> bool = default;
  };

  // The words the grammar joins and negates terms with.
  inline constexpr std::string_view OR_WORD{ "or" };
  inline constexpr std::string_view NOT_WORD{ "not" };

  // An any_of says nothing a single term does not, so a tape that writes one
  // writes at least this many alternatives.
  inline constexpr std::size_t LEAST_ALTERNATIVES{ 2 };

  // The spelling `tash trace dump` prints a hash in, so an anchor is a line
  // of a dump a human copied.
  [[nodiscard]] auto HashFrom(std::string_view text) -> Result<std::uint64_t>;

  [[nodiscard]] auto HashText(std::uint64_t hash) -> std::string;

  [[nodiscard]] auto NameOf(AnchorKind kind) noexcept -> std::string_view;

  [[nodiscard]] auto NameOf(Comparison how) noexcept -> std::string_view;

  // The inverse of AnchorCheck::Wording(): the words a refusal and a trace
  // dump already print, so a predicate can be typed back in.
  [[nodiscard]] auto AnchorFrom(std::string_view text) -> Result<Anchor>;

  // Refuses an anchor whose fields do not answer the kind it claims.
  [[nodiscard]] auto Checked(Anchor const& predicate) -> Outcome;

  [[nodiscard]] constexpr auto Holds(Comparison how, std::int64_t left,
                                     std::int64_t right) noexcept -> bool
  {
    switch (how)
    {
      case Comparison::EQUAL:            return left == right;
      case Comparison::NOT_EQUAL:        return left != right;
      case Comparison::LESS:             return left <  right;
      case Comparison::LESS_OR_EQUAL:    return left <= right;
      case Comparison::GREATER:          return left >  right;
      case Comparison::GREATER_OR_EQUAL: return left >= right;
    }
    return false;
  }
}

namespace tash::tape
{
  using detail::anchor::Anchor;
  using detail::anchor::AnchorColour;
  using detail::anchor::AnchorFrom;
  using detail::anchor::AnchorKind;
  using detail::anchor::AnchorRegion;
  using detail::anchor::Checked;
  using detail::anchor::Comparison;
  using detail::anchor::DEFAULT_LEAST_COUNT;
  using detail::anchor::DEFAULT_MAX_DIFFERENCE;
  using detail::anchor::DEFAULT_MAX_DISTANCE;
  using detail::anchor::DEFAULT_MINIMUM_SCORE;
  using detail::anchor::HashFrom;
  using detail::anchor::HashText;
  using detail::anchor::LEAST_ALTERNATIVES;
  using detail::anchor::NOT_WORD;
  using detail::anchor::NameOf;
  using detail::anchor::OR_WORD;
  using detail::anchor::Holds;
}
