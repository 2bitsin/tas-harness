#include "tash/trace/reader.hpp"

#include "tash/trace/_codec.hpp"
#include "tash/trace/format.hpp"

#include <oxbox/utilities/serdes.hpp>

#include <array>
#include <ios>
#include <span>

namespace tash::trace::detail::reader
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  auto Reader::Open(std::filesystem::path const& path) -> Result<Reader>
  {
    std::ifstream file{ path, std::ios::binary };
    if (!file)
      return Refused("trace: cannot open '{}'", path.string());

    Reader reading{ std::move(file) };
    if (auto step{ reading.ReadHeader() }; !step)
      return Forwarded(step);
    return reading;
  }

  auto Reader::ReadInto(std::size_t count) -> bool
  {
    _buffer.assign(count, std::byte{ 0 });
    if (count == 0u)
      return true;
    _file.read(reinterpret_cast<char*>(_buffer.data()),
               static_cast<std::streamsize>(count));
    return static_cast<std::size_t>(_file.gcount()) == count;
  }

  auto Reader::ReadHeader() -> Outcome
  {
    if (!ReadInto(format::FIXED_HEADER_SIZE))
      return Refused("trace: the file is shorter than a trace header");

    oxbox::utilities::BoundedReader from{ std::span<std::byte const>{
      _buffer } };
    auto const fixed{ codec::DecodeFixedHeader(from) };
    if (!fixed.magic_matched)
      return Refused("trace: the file does not start with the trace magic");
    if (fixed.format_version > format::FORMAT_VERSION)
      return Refused("trace: format version {} is newer than {}",
                     fixed.format_version, format::FORMAT_VERSION);
    if (fixed.producer_size > format::MAXIMUM_PAYLOAD_SIZE
        || fixed.header_size < format::FIXED_HEADER_SIZE + fixed.producer_size)
      return Refused("trace: the header contradicts itself");

    if (!ReadInto(fixed.producer_size))
      return Refused("trace: the header is cut short");
    _header.format_version = fixed.format_version;
    _header.creation_time  = fixed.creation_time;
    _header.producer.assign(reinterpret_cast<char const*>(_buffer.data()),
                            _buffer.size());

    // Whatever a later version put between the producer and the first record
    // belongs to nobody here, and header_size is how it is stepped over.
    auto const trailing{ fixed.header_size - format::FIXED_HEADER_SIZE
                         - fixed.producer_size };
    if (trailing != 0u && !ReadInto(trailing))
      return Refused("trace: the header is cut short");
    return { };
  }

  auto Reader::Next() -> std::optional<record::Record>
  {
    while (!_truncated)
    {
      std::array<std::byte, format::RECORD_FRAME_SIZE> frame{ };
      _file.read(reinterpret_cast<char*>(frame.data()),
                 static_cast<std::streamsize>(frame.size()));
      auto const got{ static_cast<std::size_t>(_file.gcount()) };
      if (got == 0u)
        return { };
      if (got != frame.size())
      {
        _truncated = true;
        return { };
      }

      oxbox::utilities::BoundedReader heading{ std::span<std::byte const>{
        frame } };
      auto const kind{ codec::Take<std::uint8_t>(heading) };
      auto const size{ codec::Take<std::uint32_t>(heading) };
      if (size > format::MAXIMUM_PAYLOAD_SIZE || !ReadInto(size))
      {
        _truncated = true;
        return { };
      }

      record::Record held{ };
      switch (static_cast<format::Kind>(kind))
      {
        case format::Kind::FRAME:    held = record::FrameRecord{ };    break;
        case format::Kind::INPUT:    held = record::InputRecord{ };    break;
        case format::Kind::WATCH:    held = record::WatchRecord{ };    break;
        case format::Kind::DECISION: held = record::DecisionRecord{ }; break;
        case format::Kind::VERDICT:  held = record::VerdictRecord{ };  break;
        case format::Kind::TRIGGER:  held = record::TriggerRecord{ };  break;
        case format::Kind::MARK:     held = record::MarkRecord{ };     break;
        case format::Kind::RESTORE:  held = record::RestoreRecord{ };  break;
        case format::Kind::RESET:    held = record::ResetRecord{ };    break;
        default:
          ++_unknown;
          continue;
      }

      oxbox::utilities::BoundedReader from{ std::span<std::byte const>{
        _buffer } };
      std::visit([&from](auto& into) { codec::Decode(from, into); }, held);
      if (!from.Sound())
      {
        _truncated = true;
        return { };
      }
      ++_records;
      return held;
    }
    return { };
  }
}
