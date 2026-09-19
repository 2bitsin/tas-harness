#pragma once

#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace tash::trace::detail::reader
{
  using utilities::Outcome;
  using utilities::Result;

  // Walks the records in order. A kind this build does not know is stepped
  // over and counted; a tail the writer never finished ends the walk with
  // Truncated() set, which is a report and not a refusal.
  class Reader
  {
  public:
    [[nodiscard]] static auto Open(std::filesystem::path const& path)
      -> Result<Reader>;

    Reader(Reader const&) = delete;
    auto operator = (Reader const&) -> Reader& = delete;
    Reader(Reader&&) = default;
    auto operator = (Reader&&) -> Reader& = default;

    ~Reader() = default;

    [[nodiscard]] auto FileHeader() const noexcept -> header::Header const&
    { return _header; }

    auto Next() -> std::optional<record::Record>;

    template <typename Visitor>
    auto ForEach(Visitor&& visitor) -> void
    {
      while (auto held = Next())
        std::visit(visitor, *held);
    }

    [[nodiscard]] auto Truncated() const noexcept -> bool
    { return _truncated; }
    [[nodiscard]] auto Records() const noexcept -> std::uint64_t
    { return _records; }
    [[nodiscard]] auto Unknown() const noexcept -> std::uint64_t
    { return _unknown; }

  private:
    explicit Reader(std::ifstream file) : _file{ std::move(file) } { }

    auto ReadHeader() -> Outcome;
    [[nodiscard]] auto ReadInto(std::size_t count) -> bool;

    std::ifstream          _file;
    header::Header         _header{ };
    std::vector<std::byte> _buffer{ };
    std::uint64_t          _records{ 0 };
    std::uint64_t          _unknown{ 0 };
    bool                   _truncated{ false };
  };
}

namespace tash::trace
{
  using detail::reader::Reader;
}
