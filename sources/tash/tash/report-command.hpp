#pragma once

#include <oxbox/cli/command.hpp>

#include <string>

namespace tash::cli::detail::report_command
{
  struct ReportCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(ReportCommand*);

    auto operator () (std::string bundle /* the bundle directory to read */
                     ) const -> oxbox::cli::CliResult;
  };
}

namespace tash::cli
{
  using detail::report_command::ReportCommand;
}
