#include "tash/trace/writer.hpp"

#include "tash/trace/_codec.hpp"

#include <oxbox/utilities/serdes.hpp>
#include <oxbox/utilities/span.hpp>

#include <array>
#include <ios>
#include <span>
#include <utility>
#include <variant>

namespace tash::trace::detail::writer
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  namespace
  {
    constexpr auto MinimumInterval(std::size_t asked) -> std::size_t
    {
      return asked == 0u ? 1u : asked;
    }
  }

  Writer::Writer(std::ofstream file, std::filesystem::path path,
                 header::Header file_header, std::size_t flush_interval)
  : _file{ std::move(file) }
  , _path{ std::move(path) }
  , _header{ std::move(file_header) }
  , _flush_interval{ MinimumInterval(flush_interval) }
  {
  }

  Writer::~Writer()
  {
    if (_file.is_open())
      _file.flush();
  }

  auto Writer::Open(std::filesystem::path const& path,
                    header::Header file_header, std::size_t flush_interval)
    -> Result<Writer>
  {
    std::ofstream file{ path, std::ios::binary | std::ios::trunc };
    if (!file)
      return Refused("trace: cannot create '{}'", path.string());

    Writer writing{ std::move(file), path, std::move(file_header),
                    flush_interval };
    if (auto step{ writing.WriteHeader() }; !step)
      return Forwarded(step);
    return writing;
  }

  auto Writer::Open(std::filesystem::path const& path, std::string producer)
    -> Result<Writer>
  {
    return Open(path, header::Header::Now(std::move(producer)));
  }

  auto Writer::Latch(std::unexpected<std::string> reason) -> Outcome
  {
    _refusal = reason.error();
    return Outcome{ std::move(reason) };
  }

  auto Writer::Status() const -> Outcome
  {
    if (_refusal.empty())
      return { };
    return Outcome{ std::unexpected{ _refusal } };
  }

  auto Writer::WriteHeader() -> Outcome
  {
    if (_header.producer.size() > format::MAXIMUM_PAYLOAD_SIZE)
      return Latch(Refused("trace: the producer string is longer than {} bytes",
                           format::MAXIMUM_PAYLOAD_SIZE));

    _buffer.assign(codec::HeaderSize(_header), std::byte{ 0 });
    oxbox::utilities::BoundedWriter into{ std::span{ _buffer } };
    codec::EncodeHeader(into, _header);
    if (!into.Whole())
      return Latch(Refused("trace: the header did not encode to its size"));

    _file.write(reinterpret_cast<char const*>(_buffer.data()),
                static_cast<std::streamsize>(_buffer.size()));
    if (!_file)
      return Latch(Refused("trace: cannot write to '{}'", _path.string()));

    // On disk before the first record, so a reader can attach to a live run.
    return Flush();
  }

  auto Writer::Emit(format::Kind kind, std::size_t payload_size) -> Outcome
  {
    std::array<char, format::RECORD_FRAME_SIZE> frame{ };
    oxbox::utilities::BoundedWriter into{
      oxbox::utilities::AsWritableBytes(frame) };
    codec::Put<std::uint8_t>(into, std::to_underlying(kind));
    codec::Put<std::uint32_t>(into, static_cast<std::uint32_t>(payload_size));

    _file.write(frame.data(), static_cast<std::streamsize>(frame.size()));
    _file.write(reinterpret_cast<char const*>(_buffer.data()),
                static_cast<std::streamsize>(_buffer.size()));
    if (!_file)
      return Latch(Refused("trace: cannot write to '{}'", _path.string()));

    ++_records;
    if (++_since_flush >= _flush_interval)
      return Flush();
    return { };
  }

  template <typename Held>
  auto Writer::Put(Held const& record) -> Outcome
  {
    if (!_refusal.empty())
      return Status();

    auto const size{ codec::PayloadSize(record) };
    if (size > format::MAXIMUM_PAYLOAD_SIZE)
      return Latch(Refused("trace: a {} record is longer than {} bytes",
                           record::NameOf(Held::KIND),
                           format::MAXIMUM_PAYLOAD_SIZE));

    _buffer.assign(size, std::byte{ 0 });
    oxbox::utilities::BoundedWriter into{ std::span{ _buffer } };
    codec::Encode(into, record);
    if (!into.Whole())
      return Latch(Refused("trace: a {} record did not encode to its size",
                           record::NameOf(Held::KIND)));
    return Emit(Held::KIND, size);
  }

  auto Writer::Write(record::FrameRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::InputRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::WatchRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::DecisionRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::VerdictRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::TriggerRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::MarkRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::RestoreRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::ResetRecord const& record) -> Outcome
  { return Put(record); }

  auto Writer::Write(record::Record const& record) -> Outcome
  {
    return std::visit([this](auto const& held) { return Put(held); }, record);
  }

  auto Writer::Flush() -> Outcome
  {
    _since_flush = 0u;
    _file.flush();
    if (!_file)
      return Latch(Refused("trace: cannot flush '{}'", _path.string()));
    return { };
  }
}
