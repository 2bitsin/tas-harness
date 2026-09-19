#include "tash/report/trace-digest.hpp"

#include "tash/trace/dump.hpp"
#include "tash/trace/line.hpp"
#include "tash/trace/reader.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>
#include <utility>
#include <variant>

namespace tash::report::detail::trace_digest
{
  using utilities::Forwarded;

  namespace
  {
    constexpr double NANOSECONDS_PER_SECOND{ 1e9 };

    // `tash trace dump` writes the frame, the kind and the details in three
    // whitespace-separated fields, none of the first two ever spaced.
    auto SplitLine(std::string const& line) -> TailRow
    {
      TailRow row;
      std::string_view rest{ line };
      for (std::string* field : { &row.frame, &row.kind })
      {
        rest.remove_prefix(std::min(rest.find_first_not_of(' '),
                                    rest.size()));
        std::size_t const end{ std::min(rest.find(' '), rest.size()) };
        *field = rest.substr(0, end);
        rest.remove_prefix(end);
      }
      rest.remove_prefix(std::min(rest.find_first_not_of(' '), rest.size()));
      row.details = rest;
      return row;
    }

    auto BucketsOf(std::size_t values, std::size_t points) -> std::size_t
    {
      if (values == 0 || points == 0)
        return 0;
      return (values + points - 1) / points;
    }
  }

  TraceDigest::TraceDigest(std::vector<std::string> watch_names,
                           std::size_t points, std::size_t tail)
  : _watch_names{ std::move(watch_names) }, _points{ points }, _tail{ tail }
  {
  }

  auto TraceDigest::Of(std::filesystem::path const& path,
                       std::vector<std::string> watch_names)
    -> Result<TraceReport>
  {
    TraceDigest digest{ std::move(watch_names), STRIP_POINTS, TAIL_RECORDS };
    if (Outcome const read{ digest.Read(path) }; !read)
      return Forwarded(read);
    return digest.Take();
  }

  auto TraceDigest::Read(std::filesystem::path const& path) -> Outcome
  {
    Result<trace::Reader> source{ trace::Reader::Open(path) };
    if (!source)
      return Forwarded(source);

    _producer = source->FileHeader().producer;
    while (auto held = source->Next())
    {
      std::visit([this](auto const& record) { Took(record); }, *held);
      Remember(*held);
    }
    _truncated = source->Truncated();
    return trace::Line::Foldable(source->FileHeader().format_version,
                                 !_restores.empty());
  }

  auto TraceDigest::Took(trace::FrameRecord const& record) -> void
  {
    _seconds.push_back(static_cast<double>(record.harness_time)
                       / NANOSECONDS_PER_SECOND);
    _change.push_back(record.change_amount);
    _made = std::max(_made, record.frame + 1);
  }

  auto TraceDigest::Took(trace::WatchRecord const& record) -> void
  {
    _watched[record.watch].emplace_back(record.frame,
                                        static_cast<double>(record.value));
  }

  auto TraceDigest::Took(trace::VerdictRecord const& record) -> void
  {
    VerdictEntry entry{ At(record.frame), record.name, record.passed,
                        record.text, { } };
    if (!record.passed)
      entry.tail.assign(_recent.begin(), _recent.end());
    _verdicts.push_back(std::move(entry));
  }

  auto TraceDigest::Took(trace::MarkRecord const& record) -> void
  {
    _marks.push_back(MarkEntry{ At(record.frame), record.text,
                               record.group });
  }

  auto TraceDigest::Took(trace::RestoreRecord const& record) -> void
  {
    _restores.push_back(RestoreEntry{ At(record.frame), record.to,
                                      record.name });
    _folds.push_back(trace::Fold{ record.frame, record.to });
  }

  auto TraceDigest::Took(trace::ResetRecord const& record) -> void
  {
    _resets.push_back(ResetEntry{ At(record.frame) });
    _folds.push_back(trace::Fold{ record.frame, 0 });
  }

  auto TraceDigest::Remember(trace::Record const& record) -> void
  {
    if (_tail == 0)
      return;
    _recent.push_back(SplitLine(trace::FormatRecordLine(record)));
    if (_recent.size() > _tail)
      _recent.pop_front();
  }

  auto TraceDigest::At(std::uint64_t frame) const -> Moment
  {
    if (frame < _seconds.size())
      return Moment{ frame, _seconds[frame] };
    return Moment{ frame, _seconds.empty() ? 0.0 : _seconds.back() };
  }

  auto TraceDigest::Named(std::uint32_t watch) const -> std::string
  {
    if (watch < _watch_names.size() && !_watch_names[watch].empty())
      return _watch_names[watch];
    return std::format("watch {}", watch);
  }

  auto TraceDigest::Changes() const -> StripData
  {
    StripData strip{ "change amount", { }, 0.0, 0.0 };
    std::size_t const bucket{ BucketsOf(_change.size(), _points) };
    for (std::size_t at{ 0 }; at < _change.size(); at += bucket)
    {
      auto const last{ std::min(at + bucket, _change.size()) };
      // The peak, not the mean: a landing or a scene change is one frame
      // wide and averaging it away is averaging the signal away.
      strip.points.push_back(
        *std::max_element(_change.begin() + static_cast<std::ptrdiff_t>(at),
                          _change.begin()
                            + static_cast<std::ptrdiff_t>(last)));
    }
    if (!strip.points.empty())
      strip.highest = *std::max_element(strip.points.begin(),
                                        strip.points.end());
    return strip;
  }

  auto TraceDigest::Watched() const -> std::vector<StripData>
  {
    std::vector<StripData> strips;
    std::size_t const frames{ _seconds.size() };
    std::size_t const bucket{ BucketsOf(frames, _points) };
    for (auto const& [watch, samples] : _watched)
    {
      StripData strip{ Named(watch), { }, 0.0, 0.0 };
      double held{ samples.empty() ? 0.0 : samples.front().second };
      std::size_t taken{ 0 };
      for (std::size_t at{ 0 }; at < frames && bucket != 0; at += bucket)
      {
        auto const last{ at + bucket };
        // A watch is a step function: the bucket shows where it stood when
        // it ended, not an average of the steps inside it.
        while (taken < samples.size() && samples[taken].first < last)
          held = samples[taken++].second;
        strip.points.push_back(held);
      }
      auto const [lowest, highest]{ std::minmax_element(
        strip.points.begin(), strip.points.end()) };
      if (!strip.points.empty())
      {
        strip.lowest = *lowest;
        strip.highest = *highest;
      }
      strips.push_back(std::move(strip));
    }
    return strips;
  }

  auto TraceDigest::Take() -> TraceReport
  {
    TraceReport report;
    report.producer = std::move(_producer);
    report.frames = _seconds.size();
    report.seconds = _seconds.empty() ? 0.0 : _seconds.back();
    report.change = Changes();
    report.watches = Watched();
    report.verdicts = std::move(_verdicts);
    report.marks = std::move(_marks);
    report.restores = std::move(_restores);
    report.resets = std::move(_resets);
    report.kept
      = trace::Line::Over(std::move(_folds), _made).Frames();
    report.truncated = _truncated;
    return report;
  }
}
