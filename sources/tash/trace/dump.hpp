#pragma once

#include "tash/trace/header.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"

#include <iosfwd>
#include <string>

namespace tash::trace::detail::dump
{
  // One line per record, the frame number in a fixed column and the kind
  // next to it, so a trace greps and sorts. Lines beginning with `#` are the
  // reader talking about the file rather than about a record.
  [[nodiscard]] auto FormatHeaderLine(header::Header const& file_header)
    -> std::string;

  [[nodiscard]] auto FormatRecordLine(record::Record const& record)
    -> std::string;

  auto Dump(reader::Reader& source, std::ostream& out) -> void;
}

namespace tash::trace
{
  using detail::dump::Dump;
  using detail::dump::FormatHeaderLine;
  using detail::dump::FormatRecordLine;
}
