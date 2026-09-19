#pragma once
// Plays a step script against a live session, narrowing as it goes and
// writing one line per step so the run is its own transcript.

#include "tash/session/session.hpp"
#include "tash/utilities/outcome.hpp"
#include "tash/watches/memory-search.hpp"
#include "tash/watches/search-script.hpp"

#include <cstddef>
#include <ostream>
#include <vector>

namespace tash::watches::detail::search_driver
{
  using utilities::Outcome;
  using memory_search::MemorySearch;
  using search_script::SearchStep;

  // The port a search presses, since a search drives one player.
  inline constexpr std::size_t SEARCH_PORT{ 0u };

  class SearchDriver
  {
  public:
    SearchDriver(session::Session& run, MemorySearch& search);

    auto Play(std::vector<SearchStep> const& steps, std::ostream& report)
      -> Outcome;

  private:
    auto List(std::size_t limit, std::ostream& report) const -> void;

    session::Session& _run;
    MemorySearch& _search;
  };
}

namespace tash::watches
{
  using detail::search_driver::SEARCH_PORT;
  using detail::search_driver::SearchDriver;
}
