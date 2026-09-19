#pragma once
// A memory hunt as one line of steps, because one cli invocation is one
// process and the candidate set only exists while the session is alive.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/memory-search.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace tash::watches::detail::search_script
{
  using utilities::Result;
  using memory_search::Comparison;

  inline constexpr std::size_t DEFAULT_LIST_LIMIT{ 20u };

  enum class StepKind : std::uint8_t
  {
    RUN,
    HOLD,
    RELEASE,
    SNAPSHOT,
    NARROW,
    LIST,
  };

  struct SearchStep
  {
    StepKind kind{ StepKind::RUN };
    std::uint64_t frames{ 0 };
    std::uint32_t pad{ 0 };
    Comparison how{ Comparison::CHANGED };
    std::int64_t against{ 0 };
    std::size_t limit{ DEFAULT_LIST_LIMIT };

    auto operator==(SearchStep const&) const -> bool = default;
  };

  [[nodiscard]] auto PadOf(std::string_view names) -> Result<std::uint32_t>;

  [[nodiscard]] auto StepsFrom(std::string_view text)
    -> Result<std::vector<SearchStep>>;

  [[nodiscard]] auto StepGrammar() noexcept -> std::string_view;
}

namespace tash::watches
{
  using detail::search_script::DEFAULT_LIST_LIMIT;
  using detail::search_script::PadOf;
  using detail::search_script::SearchStep;
  using detail::search_script::StepGrammar;
  using detail::search_script::StepKind;
  using detail::search_script::StepsFrom;
}
