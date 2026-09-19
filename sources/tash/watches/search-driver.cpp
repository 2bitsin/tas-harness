#include "tash/watches/search-driver.hpp"

#include <algorithm>
#include <format>

namespace tash::watches::detail::search_driver
{
  using utilities::Forwarded;
  using utilities::Outcome;

  namespace
  {
    constexpr int STEP_COLUMN{ 22 };

    [[nodiscard]] auto Said(SearchStep const& step) -> std::string
    {
      switch (step.kind)
      {
        case search_script::StepKind::RUN:
          return std::format("run {}", step.frames);
        case search_script::StepKind::HOLD:
          return std::format("hold {:#x}", step.pad);
        case search_script::StepKind::RELEASE: return "release";
        case search_script::StepKind::SNAPSHOT: return "snapshot";
        case search_script::StepKind::LIST:
          return std::format("list {}", step.limit);
        case search_script::StepKind::NARROW:
          return step.how == memory_search::Comparison::VALUE
                   ? std::format("value {}", step.against)
                   : std::string{ memory_search::NameOf(step.how) };
      }
      return "unknown";
    }
  }

  SearchDriver::SearchDriver(session::Session& run, MemorySearch& search)
  : _run{ run }, _search{ search }
  {
  }

  auto SearchDriver::Play(std::vector<SearchStep> const& steps,
                          std::ostream& report) -> Outcome
  {
    for (SearchStep const& step : steps)
    {
      switch (step.kind)
      {
        case search_script::StepKind::RUN:
          _run.Step(step.frames);
          break;
        case search_script::StepKind::HOLD:
          _run.HoldPad(SEARCH_PORT, step.pad);
          break;
        case search_script::StepKind::RELEASE:
          _run.HoldPad(SEARCH_PORT, 0u);
          break;
        case search_script::StepKind::SNAPSHOT:
          if (Outcome const taken{ _search.Snapshot() }; !taken)
            return Forwarded(taken);
          break;
        case search_script::StepKind::NARROW:
          if (Outcome const narrowed{ _search.Narrow(step.how, step.against) };
              !narrowed)
            return Forwarded(narrowed);
          break;
        case search_script::StepKind::LIST:
          break;
      }

      report << std::format("{:<{}}frame {:<8}", Said(step), STEP_COLUMN,
                            _run.Frames());
      if (_search.Seeded())
        report << std::format(" {} candidates", _search.Count());
      report << "\n";
      if (step.kind == search_script::StepKind::LIST)
        List(step.limit, report);
    }
    return {};
  }

  auto SearchDriver::List(std::size_t limit, std::ostream& report) const
    -> void
  {
    auto const shown{ std::min(limit, _search.Count()) };
    for (std::size_t at{ 0 }; at < shown; ++at)
    {
      memory_search::Candidate const& candidate{ _search.Candidates()[at] };
      report << std::format("  {:<8} {:#08x} {}\n",
                            _search.AreaName(candidate.area),
                            candidate.address, candidate.value);
    }
    if (shown < _search.Count())
      report << std::format("  ... {} more\n", _search.Count() - shown);
  }
}
