#pragma once

#include "tash/utilities/outcome.hpp"

#include <filesystem>
#include <string_view>

namespace tash::report::detail::render
{
  using utilities::Result;

  inline constexpr std::string_view REPORT_NAME{ "report.html" };

  // Writes report.html into the bundle: one page that asks the network for
  // nothing and names only the files beside it.
  [[nodiscard]] auto RenderReport(std::filesystem::path const& bundle_root)
    -> Result<std::filesystem::path>;
}

namespace tash::report
{
  using detail::render::REPORT_NAME;
  using detail::render::RenderReport;
}
