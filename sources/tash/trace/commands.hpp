#pragma once

#include <oxbox/cli/command.hpp>

#include <span>
#include <string>
#include <string_view>

namespace tash::trace::detail::commands
{
  struct TraceCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(TraceCommand*);

    auto dump(std::string file /* the trace file to read */
             ) -> oxbox::cli::CliResult; /* one line per record, and a `#` line for what the reader has to say about the file itself */

    auto sqlite(std::string file     /* the trace file to read */,
                std::string database /* the SQLite file to write, which must not exist yet */
               ) -> oxbox::cli::CliResult; /* one table per record kind and the file header in `meta`, so sqlite3 can query a run */
  };

  [[nodiscard]] auto TraceCommands() -> TraceCommand&;

  auto RunTraceCommands(std::span<std::string_view const> arguments)
    -> oxbox::cli::CliResult;
}

namespace tash::trace
{
  using detail::commands::RunTraceCommands;
  using detail::commands::TraceCommand;
  using detail::commands::TraceCommands;
}
