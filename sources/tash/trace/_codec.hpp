#pragma once

// The byte layout of every payload, written and read in one place so the two
// directions cannot drift. Sizing runs first and the writer's Whole() is what
// checks the two passes agreed.

#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/record.hpp"

#include <oxbox/utilities/serdes.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace tash::trace::detail::codec
{
  using oxbox::utilities::BoundedReader;
  using oxbox::utilities::BoundedWriter;

  inline constexpr std::endian ORDER{ std::endian::little };

  template <typename Value>
  inline auto Put(BoundedWriter& into, Value value) -> void
  {
    into.Store<Value, ORDER>(value);
  }

  template <typename Value>
  [[nodiscard]] inline auto Take(BoundedReader& from) -> Value
  {
    return from.Fetch<Value, ORDER>();
  }

  [[nodiscard]] inline auto TextSize(std::string const& text) -> std::size_t
  {
    return sizeof(std::uint32_t) + text.size();
  }

  inline auto PutText(BoundedWriter& into, std::string const& text) -> void
  {
    Put<std::uint32_t>(into, static_cast<std::uint32_t>(text.size()));
    into.Text(text);
  }

  [[nodiscard]] inline auto TakeText(BoundedReader& from) -> std::string
  {
    auto const size{ Take<std::uint32_t>(from) };
    if (size > format::MAXIMUM_PAYLOAD_SIZE)
    {
      from.Refuse();
      return { };
    }
    return std::string{ from.Text(size) };
  }

  [[nodiscard]] inline auto PayloadSize(record::FrameRecord const&)
    -> std::size_t
  {
    return 6u * 8u;
  }

  inline auto Encode(BoundedWriter& into, record::FrameRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    Put<std::int64_t>(into, record.harness_time);
    Put<std::uint64_t>(into, record.hash_exact);
    Put<std::uint64_t>(into, record.hash_difference);
    Put<std::uint64_t>(into, record.hash_perceptual);
    Put<double>(into, record.change_amount);
  }

  inline auto Decode(BoundedReader& from, record::FrameRecord& record) -> void
  {
    record.frame           = Take<std::uint64_t>(from);
    record.harness_time    = Take<std::int64_t>(from);
    record.hash_exact      = Take<std::uint64_t>(from);
    record.hash_difference = Take<std::uint64_t>(from);
    record.hash_perceptual = Take<std::uint64_t>(from);
    record.change_amount   = Take<double>(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::InputRecord const&)
    -> std::size_t
  {
    return 8u + 1u + 4u;
  }

  inline auto Encode(BoundedWriter& into, record::InputRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    Put<std::uint8_t>(into, record.port);
    Put<std::uint32_t>(into, record.pad);
  }

  inline auto Decode(BoundedReader& from, record::InputRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.port  = Take<std::uint8_t>(from);
    record.pad   = Take<std::uint32_t>(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::WatchRecord const&)
    -> std::size_t
  {
    return 8u + 4u + 8u;
  }

  inline auto Encode(BoundedWriter& into, record::WatchRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    Put<std::uint32_t>(into, record.watch);
    Put<std::int64_t>(into, record.value);
  }

  inline auto Decode(BoundedReader& from, record::WatchRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.watch = Take<std::uint32_t>(from);
    record.value = Take<std::int64_t>(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::DecisionRecord const& record)
    -> std::size_t
  {
    return 8u + TextSize(record.actor) + TextSize(record.text);
  }

  inline auto Encode(BoundedWriter& into, record::DecisionRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    PutText(into, record.actor);
    PutText(into, record.text);
  }

  inline auto Decode(BoundedReader& from, record::DecisionRecord& record)
    -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.actor = TakeText(from);
    record.text  = TakeText(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::VerdictRecord const& record)
    -> std::size_t
  {
    return 8u + TextSize(record.name) + 1u + TextSize(record.text);
  }

  inline auto Encode(BoundedWriter& into, record::VerdictRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    PutText(into, record.name);
    Put<std::uint8_t>(into, record.passed ? 1u : 0u);
    PutText(into, record.text);
  }

  inline auto Decode(BoundedReader& from, record::VerdictRecord& record) -> void
  {
    record.frame  = Take<std::uint64_t>(from);
    record.name   = TakeText(from);
    record.passed = Take<std::uint8_t>(from) != 0u;
    record.text   = TakeText(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::TriggerRecord const& record)
    -> std::size_t
  {
    return 8u + TextSize(record.name) + TextSize(record.text);
  }

  inline auto Encode(BoundedWriter& into, record::TriggerRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    PutText(into, record.name);
    PutText(into, record.text);
  }

  inline auto Decode(BoundedReader& from, record::TriggerRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.name  = TakeText(from);
    record.text  = TakeText(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::MarkRecord const& record)
    -> std::size_t
  {
    return 8u + TextSize(record.text) + TextSize(record.group);
  }

  inline auto Encode(BoundedWriter& into, record::MarkRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    PutText(into, record.text);
    PutText(into, record.group);
  }

  inline auto Decode(BoundedReader& from, record::MarkRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.text  = TakeText(from);
    // A v1 mark ends here, and its payload is what bounds this reader.
    if (from.Remaining() != 0u)
      record.group = TakeText(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::RestoreRecord const& record)
    -> std::size_t
  {
    return 8u + 8u + TextSize(record.name);
  }

  inline auto Encode(BoundedWriter& into, record::RestoreRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
    Put<std::uint64_t>(into, record.to);
    PutText(into, record.name);
  }

  inline auto Decode(BoundedReader& from, record::RestoreRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
    record.to    = Take<std::uint64_t>(from);
    record.name  = TakeText(from);
  }

  [[nodiscard]] inline auto PayloadSize(record::ResetRecord const&)
    -> std::size_t
  {
    return 8u;
  }

  inline auto Encode(BoundedWriter& into, record::ResetRecord const& record)
    -> void
  {
    Put<std::uint64_t>(into, record.frame);
  }

  inline auto Decode(BoundedReader& from, record::ResetRecord& record) -> void
  {
    record.frame = Take<std::uint64_t>(from);
  }

  [[nodiscard]] inline auto HeaderSize(header::Header const& header)
    -> std::size_t
  {
    return format::FIXED_HEADER_SIZE + header.producer.size();
  }

  inline auto EncodeHeader(BoundedWriter& into, header::Header const& header)
    -> void
  {
    for (char letter : format::MAGIC)
      Put<std::uint8_t>(into, static_cast<std::uint8_t>(letter));
    Put<std::uint16_t>(into, header.format_version);
    Put<std::uint32_t>(into, static_cast<std::uint32_t>(HeaderSize(header)));
    Put<std::int64_t>(into, header.creation_time);
    Put<std::uint32_t>(into,
                       static_cast<std::uint32_t>(header.producer.size()));
    into.Text(header.producer);
  }

  struct FixedHeader
  {
    bool          magic_matched{ false };
    std::uint16_t format_version{ 0 };
    std::uint32_t header_size{ 0 };
    std::int64_t  creation_time{ 0 };
    std::uint32_t producer_size{ 0 };
  };

  [[nodiscard]] inline auto DecodeFixedHeader(BoundedReader& from)
    -> FixedHeader
  {
    FixedHeader fixed{ };
    fixed.magic_matched = true;
    for (char letter : format::MAGIC)
      if (Take<std::uint8_t>(from) != static_cast<std::uint8_t>(letter))
        fixed.magic_matched = false;
    fixed.format_version = Take<std::uint16_t>(from);
    fixed.header_size    = Take<std::uint32_t>(from);
    fixed.creation_time  = Take<std::int64_t>(from);
    fixed.producer_size  = Take<std::uint32_t>(from);
    return fixed;
  }
}
