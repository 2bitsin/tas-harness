#include "tash/tash/segment-marks.hpp"

#include "tash/utilities/outcome.hpp"

#include <string>
#include <string_view>

namespace tash::cli::detail::segment_marks
{
  using utilities::Outcome;

  auto SegmentMarks::OnMark(std::string_view text) -> void
  {
    if (Outcome const made{ _into->Mark(std::string{ text },
                                        std::string{ }) };
        !made && _problem.empty())
      _problem = made.error();
  }
}
