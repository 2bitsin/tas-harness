#pragma once

#include "tash/report/report-data.hpp"
#include "tash/trace/line.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tash::report::detail::trace_digest
{
  using utilities::Outcome;
  using utilities::Result;

  // A strip is drawn at this many points whatever the run's length; a frame
  // per point would put an hour of records into the DOM.
  inline constexpr std::size_t STRIP_POINTS{ 720 };

  // What a failed verdict shows of the run that led to it.
  inline constexpr std::size_t TAIL_RECORDS{ 40 };

  // The whole trace in one pass: the curves it draws are kept per frame and
  // reduced at the end, when the run's length is known.
  class TraceDigest
  {
  public:
    TraceDigest(std::vector<std::string> watch_names, std::size_t points,
                std::size_t tail);

    [[nodiscard]] static auto Of(std::filesystem::path const& path,
                                 std::vector<std::string> watch_names)
      -> Result<TraceReport>;

    [[nodiscard]] auto Read(std::filesystem::path const& path) -> Outcome;

    [[nodiscard]] auto Take() -> TraceReport;

  private:
    auto Took(trace::FrameRecord const& record) -> void;
    auto Took(trace::WatchRecord const& record) -> void;
    auto Took(trace::VerdictRecord const& record) -> void;
    auto Took(trace::MarkRecord const& record) -> void;
    auto Took(trace::InputRecord const&) -> void { }
    auto Took(trace::DecisionRecord const&) -> void { }
    auto Took(trace::TriggerRecord const&) -> void { }
    auto Took(trace::RestoreRecord const& record) -> void;
    auto Took(trace::ResetRecord const& record) -> void;

    auto Remember(trace::Record const& record) -> void;
    [[nodiscard]] auto At(std::uint64_t frame) const -> Moment;
    [[nodiscard]] auto Named(std::uint32_t watch) const -> std::string;
    [[nodiscard]] auto Changes() const -> StripData;
    [[nodiscard]] auto Watched() const -> std::vector<StripData>;

    std::vector<std::string> _watch_names;
    std::size_t              _points;
    std::size_t              _tail;

    std::string              _producer;
    std::vector<double>      _seconds;
    std::vector<double>      _change;
    std::map<std::uint32_t, std::vector<std::pair<std::uint64_t, double>>>
                             _watched;
    std::vector<VerdictEntry> _verdicts;
    std::vector<MarkEntry>    _marks;
    std::vector<RestoreEntry> _restores;
    std::vector<ResetEntry>   _resets;
    std::vector<trace::Fold>  _folds;
    std::uint64_t             _made{ 0 };
    std::deque<TailRow>       _recent;
    bool                      _truncated{ false };
  };
}

namespace tash::report
{
  using detail::trace_digest::STRIP_POINTS;
  using detail::trace_digest::TAIL_RECORDS;
  using detail::trace_digest::TraceDigest;
}
