#pragma once

#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace tash::trace::detail::writer
{
  using utilities::Outcome;
  using utilities::Result;

  // Append-only. The first failure latches and every later call answers the
  // same refusal, so a frame path can write and check once at the end; the
  // file is flushed every `flush_interval` records and again when it goes.
  class Writer
  {
  public:
    [[nodiscard]] static auto Open(
      std::filesystem::path const& path, header::Header file_header,
      std::size_t flush_interval = format::DEFAULT_FLUSH_INTERVAL)
      -> Result<Writer>;

    [[nodiscard]] static auto Open(std::filesystem::path const& path,
                                   std::string producer) -> Result<Writer>;

    Writer(Writer const&) = delete;
    auto operator = (Writer const&) -> Writer& = delete;
    Writer(Writer&&) = default;
    auto operator = (Writer&&) -> Writer& = default;

    ~Writer();

    auto Write(record::FrameRecord const& record) -> Outcome;
    auto Write(record::InputRecord const& record) -> Outcome;
    auto Write(record::WatchRecord const& record) -> Outcome;
    auto Write(record::DecisionRecord const& record) -> Outcome;
    auto Write(record::VerdictRecord const& record) -> Outcome;
    auto Write(record::TriggerRecord const& record) -> Outcome;
    auto Write(record::MarkRecord const& record) -> Outcome;
    auto Write(record::RestoreRecord const& record) -> Outcome;
    auto Write(record::ResetRecord const& record) -> Outcome;
    auto Write(record::Record const& record) -> Outcome;

    auto Flush() -> Outcome;

    [[nodiscard]] auto Status() const -> Outcome;
    [[nodiscard]] auto Records() const noexcept -> std::uint64_t
    { return _records; }
    [[nodiscard]] auto FileHeader() const noexcept -> header::Header const&
    { return _header; }

  private:
    Writer(std::ofstream file, std::filesystem::path path,
           header::Header file_header, std::size_t flush_interval);

    template <typename Held>
    auto Put(Held const& record) -> Outcome;

    auto WriteHeader() -> Outcome;
    auto Emit(format::Kind kind, std::size_t payload_size) -> Outcome;
    auto Latch(std::unexpected<std::string> reason) -> Outcome;

    std::ofstream          _file;
    std::filesystem::path  _path;
    header::Header         _header;
    std::vector<std::byte> _buffer{ };
    std::size_t            _flush_interval;
    std::size_t            _since_flush{ 0 };
    std::uint64_t          _records{ 0 };
    std::string            _refusal{ };
  };
}

namespace tash::trace
{
  using detail::writer::Writer;
}
