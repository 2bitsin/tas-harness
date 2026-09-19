#include "tash/trace/commands.hpp"

#include "tash/trace/dump.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/sqlite-export.hpp"

#include <oxbox/cli/main.hpp>

#include <filesystem>
#include <format>
#include <iostream>

namespace tash::trace::detail::commands
{
  auto TraceCommand::dump(std::string file) -> oxbox::cli::CliResult
  {
    auto source{ reader::Reader::Open(std::filesystem::path{ file }) };
    if (!source)
      return oxbox::cli::CliResult::Failed(1, source.error());

    dump::Dump(*source, std::cout);
    return { };
  }

  auto TraceCommand::sqlite(std::string file, std::string database)
    -> oxbox::cli::CliResult
  {
    auto source{ reader::Reader::Open(std::filesystem::path{ file }) };
    if (!source)
      return oxbox::cli::CliResult::Failed(1, source.error());

    auto const rows{ sqlite_export::ExportToSqlite(
      *source, std::filesystem::path{ database }) };
    if (!rows)
      return oxbox::cli::CliResult::Failed(1, rows.error());

    std::cout << std::format("{} rows into {}\n", *rows, database);
    if (source->Truncated())
      std::cout << "the last record is incomplete\n";
    return { };
  }

  auto TraceCommands() -> TraceCommand&
  {
    return oxbox::cli::Command::Get<TraceCommand>();
  }

  auto RunTraceCommands(std::span<std::string_view const> arguments)
    -> oxbox::cli::CliResult
  {
    return oxbox::cli::Main(TraceCommands(), arguments, "tash trace");
  }
}
