#pragma once

#include "tash/utilities/outcome.hpp"

#include "oxbox/platform/scratch-area.hpp"

#include <string_view>

namespace tash::utilities::detail::scratch_area
{
  inline constexpr std::string_view SCRATCH_PROGRAM{ "tash" };

  // `purpose` is one path component, so a `/` in it is a refusal.
  [[nodiscard]] auto ScratchAreaOf(std::string_view purpose)
      -> Result<oxbox::platform::ScratchArea>;
}

namespace tash::utilities
{
  using detail::scratch_area::SCRATCH_PROGRAM;
  using detail::scratch_area::ScratchAreaOf;
}
