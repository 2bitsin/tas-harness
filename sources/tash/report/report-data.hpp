#pragma once
// What one pass over a trace leaves for the page to draw.

#include <cstdint>
#include <string>
#include <vector>

namespace tash::report::detail::report_data
{
  struct Moment
  {
    std::uint64_t frame{ 0 };
    double        seconds{ 0.0 };
  };

  // One line of the trace as `tash trace dump` words it, split so the page
  // can put the frame and the kind in their own columns.
  struct TailRow
  {
    std::string frame;
    std::string kind;
    std::string details;
  };

  struct VerdictEntry
  {
    Moment               when;
    std::string          name;
    bool                 passed{ false };
    std::string          text;
    std::vector<TailRow> tail;
  };

  struct MarkEntry
  {
    Moment      when;
    std::string text;
    std::string group;
  };

  // Where a restore put the run back: `to` is the frames it had made when
  // the checkpoint it went to was taken.
  struct RestoreEntry
  {
    Moment        when;
    std::uint64_t to{ 0 };
    std::string   name;
  };

  // Where the run power-cycled, which folds the whole line off.
  struct ResetEntry
  {
    Moment when;
  };

  // A curve already reduced to the points the page draws; `lowest` and
  // `highest` are the values those points are scaled between.
  struct StripData
  {
    std::string         name;
    std::vector<double> points;
    double              lowest{ 0.0 };
    double              highest{ 0.0 };
  };

  struct TraceReport
  {
    std::string               producer;
    std::uint64_t             frames{ 0 };
    double                    seconds{ 0.0 };
    StripData                 change;
    std::vector<StripData>    watches;
    std::vector<VerdictEntry> verdicts;
    std::vector<MarkEntry>    marks;
    std::vector<RestoreEntry> restores;
    std::vector<ResetEntry>   resets;

    // What the restores and the resets left: the frames the run kept.
    std::uint64_t             kept{ 0 };
    bool                      truncated{ false };
  };
}

namespace tash::report
{
  using detail::report_data::MarkEntry;
  using detail::report_data::Moment;
  using detail::report_data::ResetEntry;
  using detail::report_data::RestoreEntry;
  using detail::report_data::StripData;
  using detail::report_data::TailRow;
  using detail::report_data::TraceReport;
  using detail::report_data::VerdictEntry;
}
