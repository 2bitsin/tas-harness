#include "tash/tape/anchor.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace tash::tape::detail::anchor
{
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view HEX_PREFIX{ "0x" };
    inline constexpr int HEX_BASE{ 16 };

    // colour <r,g,b> within <n> over <x,y,w,h> at least <count>
    inline constexpr std::size_t COLOUR_WORDS{ 8 };
  }

  auto HashFrom(std::string_view text) -> Result<std::uint64_t>
  {
    std::string_view digits{ text };
    if (digits.starts_with(HEX_PREFIX))
      digits.remove_prefix(HEX_PREFIX.size());
    if (digits.empty() || digits.size() > HASH_DIGITS)
      return Refused("tape: '{}' is not a hash, which is {} hex digits", text,
                     HASH_DIGITS);

    std::uint64_t hash{ 0 };
    auto const [end, failed]{ std::from_chars(
      digits.data(), digits.data() + digits.size(), hash, HEX_BASE) };
    if (failed != std::errc{ } || end != digits.data() + digits.size())
      return Refused("tape: '{}' is not a hash, which is {} hex digits", text,
                     HASH_DIGITS);
    return hash;
  }

  auto HashText(std::uint64_t hash) -> std::string
  {
    return std::format("{:016x}", hash);
  }

  auto NameOf(AnchorKind kind) noexcept -> std::string_view
  {
    switch (kind)
    {
      case AnchorKind::NONE:            return "none";
      case AnchorKind::EXACT_HASH:      return "exact_hash";
      case AnchorKind::DIFFERENCE_HASH: return "difference_hash";
      case AnchorKind::PERCEPTUAL_HASH: return "perceptual_hash";
      case AnchorKind::TEMPLATE_IMAGE:  return "template_image";
      case AnchorKind::WATCH:           return "watch";
      case AnchorKind::COLOUR:          return "colour";
      case AnchorKind::ANY_OF:          return "any_of";
    }
    return "unknown";
  }

  auto NameOf(Comparison how) noexcept -> std::string_view
  {
    switch (how)
    {
      case Comparison::EQUAL:            return "equal";
      case Comparison::NOT_EQUAL:        return "not_equal";
      case Comparison::LESS:             return "less";
      case Comparison::LESS_OR_EQUAL:    return "less_or_equal";
      case Comparison::GREATER:          return "greater";
      case Comparison::GREATER_OR_EQUAL: return "greater_or_equal";
    }
    return "unknown";
  }

  namespace
  {
    auto WordsOf(std::string_view text) -> std::vector<std::string_view>
    {
      std::vector<std::string_view> words;
      for (std::size_t at{ 0 }; at < text.size(); )
      {
        std::size_t const start{ text.find_first_not_of(' ', at) };
        if (start == std::string_view::npos)
          break;
        std::size_t const end{ std::min(text.find(' ', start), text.size()) };
        words.push_back(text.substr(start, end - start));
        at = end;
      }
      return words;
    }

    auto KindFrom(std::string_view word) -> Result<AnchorKind>
    {
      for (AnchorKind const kind : { AnchorKind::NONE, AnchorKind::EXACT_HASH,
                                     AnchorKind::DIFFERENCE_HASH,
                                     AnchorKind::PERCEPTUAL_HASH,
                                     AnchorKind::TEMPLATE_IMAGE,
                                     AnchorKind::WATCH,
                                     AnchorKind::COLOUR,
                                     AnchorKind::ANY_OF })
        if (NameOf(kind) == word)
          return kind;
      return Refused("tape: '{}' is not an anchor kind", word);
    }

    auto ComparisonFrom(std::string_view word) -> Result<Comparison>
    {
      for (Comparison const how : { Comparison::EQUAL, Comparison::NOT_EQUAL,
                                    Comparison::LESS,
                                    Comparison::LESS_OR_EQUAL,
                                    Comparison::GREATER,
                                    Comparison::GREATER_OR_EQUAL })
        if (NameOf(how) == word)
          return how;
      return Refused("tape: '{}' is not a comparison", word);
    }

    template <typename Number>
    auto NumberFrom(std::string_view word) -> Result<Number>
    {
      Number value{ };
      auto const [end, failed]{ std::from_chars(
        word.data(), word.data() + word.size(), value) };
      if (failed != std::errc{ } || end != word.data() + word.size())
        return Refused("tape: '{}' is not a number", word);
      return value;
    }

    // How many words a kind takes after its name, `within`/`at` tails read.
    auto WordsTaken(AnchorKind kind,
                    std::vector<std::string_view> const& words,
                    std::size_t at) -> std::size_t
    {
      auto const tail{ [&](std::string_view marker) -> std::size_t
      {
        return at + 2 < words.size() && words[at + 2] == marker ? 3u : 1u;
      } };
      switch (kind)
      {
        case AnchorKind::NONE:            return 0;
        case AnchorKind::EXACT_HASH:
        case AnchorKind::DIFFERENCE_HASH:
        case AnchorKind::PERCEPTUAL_HASH: return tail("within");
        case AnchorKind::TEMPLATE_IMAGE:  return tail("at");
        case AnchorKind::WATCH:           return 3;
        case AnchorKind::COLOUR:          return COLOUR_WORDS;
        case AnchorKind::ANY_OF:          return 0;
      }
      return 0;
    }

    auto HashTermFrom(Anchor& term,
                      std::vector<std::string_view> const& words,
                      std::size_t at, std::size_t given) -> Outcome
    {
      term.hash = std::string{ words[at + 1] };
      if (given == 1)
      {
        if (at + 2 < words.size() && words[at + 2] != OR_WORD)
          return Refused("tape: a distance is 'within <n>', not '{}'",
                         words[at + 2]);
        return { };
      }
      Result<std::uint32_t> const distance{
        NumberFrom<std::uint32_t>(words[at + 3]) };
      if (!distance)
        return utilities::Forwarded(distance);
      term.max_distance = *distance;
      return { };
    }

    auto ImageTermFrom(Anchor& term,
                       std::vector<std::string_view> const& words,
                       std::size_t at, std::size_t given) -> Outcome
    {
      term.image = std::string{ words[at + 1] };
      if (given == 1)
      {
        if (at + 2 < words.size() && words[at + 2] != OR_WORD)
          return Refused("tape: a score is 'at <n>', not '{}'",
                         words[at + 2]);
        return { };
      }
      Result<double> const score{ NumberFrom<double>(words[at + 3]) };
      if (!score)
        return utilities::Forwarded(score);
      term.minimum_score = *score;
      return { };
    }

    auto WatchTermFrom(Anchor& term,
                       std::vector<std::string_view> const& words,
                       std::size_t at) -> Outcome
    {
      term.watch = std::string{ words[at + 1] };
      Result<Comparison> const how{ ComparisonFrom(words[at + 2]) };
      if (!how)
        return utilities::Forwarded(how);
      term.comparison = *how;
      Result<std::int64_t> const wanted{
        NumberFrom<std::int64_t>(words[at + 3]) };
      if (!wanted)
        return utilities::Forwarded(wanted);
      term.value = *wanted;
      return { };
    }

    auto ColourTermFrom(Anchor& term,
                        std::vector<std::string_view> const& words,
                        std::size_t at, std::string_view text) -> Outcome
    {
      if (words[at + 2] != "within" || words[at + 4] != "over"
          || words[at + 6] != "at" || words[at + 7] != "least")
        return Refused("tape: 'colour <r,g,b> within <n> over <x,y,w,h> at "
                       "least <count>', not '{}'", text);

      Result<perception::Colour> const wanted{
        perception::ColourFrom(words[at + 1]) };
      if (!wanted)
        return Refused("tape: {}", wanted.error());
      term.colour = AnchorColour{ wanted->red, wanted->green, wanted->blue };

      Result<std::uint32_t> const difference{
        NumberFrom<std::uint32_t>(words[at + 3]) };
      if (!difference)
        return utilities::Forwarded(difference);
      term.max_difference = *difference;

      Result<perception::Region> const over{
        perception::RegionFrom(words[at + 5]) };
      if (!over)
        return Refused("tape: {}", over.error());
      term.region = AnchorRegion{ over->x, over->y, over->width,
                                  over->height };

      Result<std::uint64_t> const least{
        NumberFrom<std::uint64_t>(words[at + 8]) };
      if (!least)
        return utilities::Forwarded(least);
      term.count = *least;
      return { };
    }

    // The form a kind is refused by when the words run out under it.
    auto FormOf(AnchorKind kind, std::string_view text) -> std::string
    {
      switch (kind)
      {
        case AnchorKind::EXACT_HASH:
        case AnchorKind::DIFFERENCE_HASH:
        case AnchorKind::PERCEPTUAL_HASH:
          return std::format("tape: '{} <hash> [within <distance>]', not '{}'",
                             NameOf(kind), text);
        case AnchorKind::TEMPLATE_IMAGE:
          return std::format("tape: '{} <image> [at <score>]', not '{}'",
                             NameOf(kind), text);
        case AnchorKind::WATCH:
          return std::format("tape: '{} <name> <comparison> <value>', not"
                             " '{}'", NameOf(kind), text);
        case AnchorKind::COLOUR:
          return std::format("tape: 'colour <r,g,b> within <n> over"
                             " <x,y,w,h> at least <count>', not '{}'", text);
        case AnchorKind::NONE:
        case AnchorKind::ANY_OF:
          break;
      }
      return std::format("tape: '{}' is not a {} anchor", text, NameOf(kind));
    }

    // Reads one term and leaves `at` on the word after it.
    auto TermFrom(std::vector<std::string_view> const& words, std::size_t& at,
                  std::string_view text) -> Result<Anchor>
    {
      Result<AnchorKind> const kind{ KindFrom(words[at]) };
      if (!kind)
        return utilities::Forwarded(kind);
      if (*kind == AnchorKind::ANY_OF)
        return Refused("tape: an any_of is typed as '<predicate> {} "
                       "<predicate>'", OR_WORD);

      std::size_t const given{ WordsTaken(*kind, words, at) };
      if (at + given >= words.size())
        return std::unexpected{ FormOf(*kind, text) };

      Anchor term;
      term.kind = *kind;
      Outcome read{ };
      switch (*kind)
      {
        case AnchorKind::NONE:
          break;
        case AnchorKind::EXACT_HASH:
        case AnchorKind::DIFFERENCE_HASH:
        case AnchorKind::PERCEPTUAL_HASH:
          read = HashTermFrom(term, words, at, given);
          break;
        case AnchorKind::TEMPLATE_IMAGE:
          read = ImageTermFrom(term, words, at, given);
          break;
        case AnchorKind::WATCH:
          read = WatchTermFrom(term, words, at);
          break;
        case AnchorKind::COLOUR:
          read = ColourTermFrom(term, words, at, text);
          break;
        case AnchorKind::ANY_OF:
          break;
      }
      if (!read)
        return utilities::Forwarded(read);
      at += given + 1;
      return term;
    }
  }

  auto AnchorFrom(std::string_view text) -> Result<Anchor>
  {
    std::vector<std::string_view> const words{ WordsOf(text) };
    if (words.empty())
      return Refused("tape: '{}' says nothing to wait for", text);

    std::vector<Anchor> terms;
    for (std::size_t at{ 0 }; ; )
    {
      bool negated{ false };
      while (at < words.size() && words[at] == NOT_WORD)
      {
        negated = !negated;
        ++at;
      }
      if (at >= words.size())
        return Refused("tape: '{}' negates nothing in '{}'", NOT_WORD, text);

      Result<Anchor> term{ TermFrom(words, at, text) };
      if (!term)
        return utilities::Forwarded(term);
      if (negated)
        term->negated = true;
      terms.push_back(std::move(*term));

      if (at >= words.size())
        break;
      if (words[at] != OR_WORD)
        return Refused("tape: '{}' is not '{}', which is how two predicates"
                       " join", words[at], OR_WORD);
      ++at;
      if (at >= words.size())
        return Refused("tape: '{}' has nothing after it in '{}'", OR_WORD,
                       text);
    }

    Anchor predicate;
    if (terms.size() == 1)
      predicate = std::move(terms.front());
    else
    {
      predicate.kind = AnchorKind::ANY_OF;
      predicate.any_of = std::move(terms);
    }

    if (Outcome const sound{ Checked(predicate) }; !sound)
      return utilities::Forwarded(sound);
    return predicate;
  }

  auto Checked(Anchor const& predicate) -> Outcome
  {
    switch (predicate.kind)
    {
      case AnchorKind::NONE:
        return { };

      case AnchorKind::EXACT_HASH:
      case AnchorKind::DIFFERENCE_HASH:
      case AnchorKind::PERCEPTUAL_HASH:
        if (Result<std::uint64_t> const hash{
              HashFrom(predicate.HashText()) }; !hash)
          return utilities::Forwarded(hash);
        return { };

      case AnchorKind::TEMPLATE_IMAGE:
        if (predicate.ImagePath().empty())
          return Refused("tape: a {} anchor needs an image",
                         NameOf(predicate.kind));
        return { };

      case AnchorKind::WATCH:
        if (predicate.WatchName().empty())
          return Refused("tape: a {} anchor needs a watch name",
                         NameOf(predicate.kind));
        return { };

      case AnchorKind::ANY_OF:
        if (!predicate.any_of
            || predicate.any_of->size() < LEAST_ALTERNATIVES)
          return Refused("tape: an {} anchor holds {} alternatives or more",
                         NameOf(predicate.kind), LEAST_ALTERNATIVES);
        for (Anchor const& one : *predicate.any_of)
          if (Outcome const sound{ Checked(one) }; !sound)
            return sound;
        return { };

      case AnchorKind::COLOUR:
        if (!predicate.colour || !predicate.count)
          return Refused("tape: a {} anchor needs a colour and a count",
                         NameOf(predicate.kind));
        if (!predicate.region)
          return Refused("tape: a {} anchor counts over a crop, so it needs"
                         " one", NameOf(predicate.kind));
        if (!predicate.WantedColour().Rgb().InRange())
          return Refused("tape: a channel of a {} anchor runs 0 to {}",
                         NameOf(predicate.kind), perception::CHANNEL_MAX);
        return { };
    }
    return Refused("tape: an anchor has no kind");
  }
}
