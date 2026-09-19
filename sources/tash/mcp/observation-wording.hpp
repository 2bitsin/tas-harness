#pragma once
// The text `observe` answers, which every moving tool appends to its own.

#include "tash/python/scenario-run.hpp"

#include <string>

namespace tash::mcp::detail::observation_wording
{
  [[nodiscard]] auto Wording(python::Observation const& seen) -> std::string;
}

namespace tash::mcp
{
  using detail::observation_wording::Wording;
}
