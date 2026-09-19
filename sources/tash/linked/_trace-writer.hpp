#pragma once
// The trace a recording leaves: the file tash/trace/ reads, written here
// without any of it, since a target links this and nothing else of tash.

#include "_byte-sink.hpp"

#include "tash/trace/format.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::linked::detail::trace_writer
{
  class TraceWriter
  {
  public:
    TraceWriter(byte_sink::ByteSink sink, std::string producer);

    auto Frame(std::uint64_t frame, std::int64_t nanoseconds,
               std::uint64_t hash) -> void;

    auto Watch(std::uint64_t frame, std::uint32_t watch, std::int64_t value)
      -> void;

    auto Event(std::uint64_t frame, std::string_view name,
               std::string_view text) -> void;

    auto Decision(std::uint64_t frame, std::string_view actor,
                  std::string_view text) -> void;

    auto Flush() -> void;

    [[nodiscard]] auto Bytes() const noexcept -> std::span<std::byte const>
    { return _sink.Bytes(); }

    [[nodiscard]] auto Refusal() const noexcept -> std::string_view
    { return _sink.Refusal(); }

    [[nodiscard]] auto Records() const noexcept -> std::uint64_t
    { return _records; }

  private:
    auto Put(trace::Kind kind) -> void;

    byte_sink::ByteSink    _sink;
    std::vector<std::byte> _payload{ };
    std::uint64_t          _records{ 0 };
    std::size_t            _since_flush{ 0 };
  };
}
