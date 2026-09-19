#include "_trace-writer.hpp"

#include <bit>
#include <chrono>
#include <utility>

namespace tash::linked::detail::trace_writer
{
  namespace
  {
    template <typename Number>
    auto PutNumber(std::vector<std::byte>& into, Number value) -> void
    {
      auto const bits{ static_cast<std::uint64_t>(
        static_cast<std::make_unsigned_t<Number>>(value)) };
      for (std::size_t at{ 0 }; at < sizeof(Number); ++at)
        into.push_back(static_cast<std::byte>((bits >> (8u * at)) & 0xffu));
    }

    auto PutText(std::vector<std::byte>& into, std::string_view text) -> void
    {
      PutNumber<std::uint32_t>(into, static_cast<std::uint32_t>(text.size()));
      for (char letter : text)
        into.push_back(static_cast<std::byte>(letter));
    }

    auto PutDouble(std::vector<std::byte>& into, double value) -> void
    {
      PutNumber<std::uint64_t>(into, std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] auto EpochNanoseconds() -> std::int64_t
    {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
    }
  }

  TraceWriter::TraceWriter(byte_sink::ByteSink sink, std::string producer)
    : _sink{ std::move(sink) }
  {
    std::vector<std::byte> header;
    for (char letter : trace::MAGIC)
      header.push_back(static_cast<std::byte>(letter));
    PutNumber<std::uint16_t>(header, trace::FORMAT_VERSION);
    PutNumber<std::uint32_t>(
      header, static_cast<std::uint32_t>(trace::FIXED_HEADER_SIZE
                                         + producer.size()));
    PutNumber<std::int64_t>(header, EpochNanoseconds());
    PutText(header, producer);
    _sink.Append(header);
    _sink.Flush();
  }

  auto TraceWriter::Put(trace::Kind kind) -> void
  {
    std::vector<std::byte> record;
    record.push_back(static_cast<std::byte>(std::to_underlying(kind)));
    PutNumber<std::uint32_t>(record,
                             static_cast<std::uint32_t>(_payload.size()));
    _sink.Append(record);
    _sink.Append(_payload);
    _payload.clear();

    ++_records;
    if (++_since_flush >= trace::DEFAULT_FLUSH_INTERVAL)
      Flush();
  }

  auto TraceWriter::Frame(std::uint64_t frame, std::int64_t nanoseconds,
                          std::uint64_t hash) -> void
  {
    PutNumber<std::uint64_t>(_payload, frame);
    PutNumber<std::int64_t>(_payload, nanoseconds);
    PutNumber<std::uint64_t>(_payload, hash);
    // The two perceptual hashes and the change amount are the harness's:
    // they want a downscaled grey copy, which is opencv, which a target
    // linking this does not get.
    PutNumber<std::uint64_t>(_payload, 0u);
    PutNumber<std::uint64_t>(_payload, 0u);
    PutDouble(_payload, 0.0);
    Put(trace::Kind::FRAME);
  }

  auto TraceWriter::Watch(std::uint64_t frame, std::uint32_t watch,
                          std::int64_t value) -> void
  {
    PutNumber<std::uint64_t>(_payload, frame);
    PutNumber<std::uint32_t>(_payload, watch);
    PutNumber<std::int64_t>(_payload, value);
    Put(trace::Kind::WATCH);
  }

  auto TraceWriter::Event(std::uint64_t frame, std::string_view name,
                          std::string_view text) -> void
  {
    PutNumber<std::uint64_t>(_payload, frame);
    PutText(_payload, name);
    PutText(_payload, text);
    Put(trace::Kind::TRIGGER);
  }

  auto TraceWriter::Decision(std::uint64_t frame, std::string_view actor,
                             std::string_view text) -> void
  {
    PutNumber<std::uint64_t>(_payload, frame);
    PutText(_payload, actor);
    PutText(_payload, text);
    Put(trace::Kind::DECISION);
  }

  auto TraceWriter::Flush() -> void
  {
    _since_flush = 0u;
    _sink.Flush();
  }
}
