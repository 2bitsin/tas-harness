#pragma once
// Every segment a tape player reaches, marked on the run it plays into.

#include "tash/python/scenario-run.hpp"
#include "tash/tape/mark-observer.hpp"

#include <string>
#include <string_view>

namespace tash::cli::detail::segment_marks
{
  // The clean run's time line carries what the search run cut its tape on.
  class SegmentMarks : public tape::MarkObserver
  {
  public:
    explicit SegmentMarks(python::ScenarioRun& into) noexcept
      : _into{ &into } { }

    auto OnMark(std::string_view text) -> void override;

    [[nodiscard]] auto Problem() const noexcept -> std::string const&
    { return _problem; }

  private:
    python::ScenarioRun* _into;
    std::string          _problem{ };
  };
}

namespace tash::cli
{
  using detail::segment_marks::SegmentMarks;
}
