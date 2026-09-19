#pragma once

#include "tash/trace/reader.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>

namespace tash::trace::detail::sqlite_export
{
  using utilities::Result;

  // Plural table names because `trigger` is a keyword SQLite would make
  // every query quote; the header lands in `meta`, the answer is the rows.
  [[nodiscard]] auto ExportToSqlite(reader::Reader& source,
                                    std::filesystem::path const& database)
    -> Result<std::uint64_t>;
}

namespace tash::trace
{
  using detail::sqlite_export::ExportToSqlite;
}
