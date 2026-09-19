#include "tash/tash/report-command.hpp"

#include "tash/report/render.hpp"

#include <filesystem>
#include <print>

namespace tash::cli::detail::report_command
{
  using utilities::Result;

  auto ReportCommand::operator () (std::string bundle) const
    -> oxbox::cli::CliResult
  {
    Result<std::filesystem::path> const page{
      report::RenderReport(std::filesystem::path{ bundle }) };
    if (!page)
      return oxbox::cli::CliResult::Failed(1, page.error());

    std::print("report {}\n", page->string());
    return { };
  }
}
