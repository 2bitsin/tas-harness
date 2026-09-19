#include "tash/trace/line.hpp"

#include "tash/trace/format.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace tash::trace::detail::line
{
  using utilities::Forwarded;
  using utilities::Refused;

  auto Line::Of(std::filesystem::path const& path) -> Result<Line>
  {
    Result<reader::Reader> source{ reader::Reader::Open(path) };
    if (!source)
      return Forwarded(source);
    return In(*source);
  }

  auto Line::In(reader::Reader& source) -> Result<Line>
  {
    std::vector<Fold> folds{ };
    std::uint64_t made{ 0 };
    bool restored{ false };
    source.ForEach([&folds, &made, &restored](auto const& record) {
      using Held = std::decay_t<decltype(record)>;
      if constexpr (std::is_same_v<Held, record::FrameRecord>)
        made = std::max(made, record.frame + 1);
      else if constexpr (std::is_same_v<Held, record::RestoreRecord>)
      {
        restored = true;
        folds.push_back(Fold{ record.frame, record.to });
      }
      else if constexpr (std::is_same_v<Held, record::ResetRecord>)
        folds.push_back(Fold{ record.frame, 0 });
    });

    if (Outcome const sound{ Foldable(source.FileHeader().format_version,
                                      restored) }; !sound)
      return Forwarded(sound);
    return Over(std::move(folds), made);
  }

  auto Line::Foldable(std::uint16_t format_version, bool restored) -> Outcome
  {
    if (!restored || format_version >= format::EARLIEST_FOLDABLE_VERSION)
      return { };
    return Refused(
      "trace: format {} wrote a restore's target as a frame of the run and "
      "not a position on the line, so what it kept cannot be folded out of "
      "it; the bundle has to be run again to be reported or replayed",
      format_version);
  }

  auto Line::Over(std::vector<Fold> folds, std::uint64_t made) -> Line
  {
    // A frame record can land after a restore taken while it was hashing, so
    // the order the folds happened in is the frames they happened at.
    std::ranges::sort(folds, { }, &Fold::frame);

    Line played{ };
    std::uint64_t from{ 0 };
    for (Fold const& fold : folds)
    {
      played.Grew(from, fold.frame);
      played.FoldedTo(fold.to);
      from = fold.frame;
    }
    played.Grew(from, made);
    return played;
  }

  auto Line::Holds(std::uint64_t frame) const -> bool
  {
    return std::ranges::any_of(_spans, [frame](Span const& span) {
      return frame >= span.from && frame < span.to; });
  }

  auto Line::Frames() const -> std::uint64_t
  {
    std::uint64_t counted{ 0 };
    for (Span const& span : _spans)
      counted += span.to - span.from;
    return counted;
  }

  auto Line::Grew(std::uint64_t from, std::uint64_t to) -> void
  {
    if (to > from)
      _spans.push_back(Span{ from, to });
  }

  auto Line::FoldedTo(std::uint64_t frames) -> void
  {
    std::uint64_t kept{ 0 };
    for (std::size_t which{ 0 }; which < _spans.size(); ++which)
    {
      std::uint64_t const length{ _spans[which].to - _spans[which].from };
      if (kept + length < frames)
      {
        kept += length;
        continue;
      }
      _spans[which].to = _spans[which].from + (frames - kept);
      _spans.resize(_spans[which].to > _spans[which].from ? which + 1 : which);
      return;
    }
  }
}
