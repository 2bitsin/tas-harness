#include "tash/trace/dump.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <ostream>
#include <variant>

namespace tash::trace::detail::dump
{
  namespace
  {
    constexpr std::uint64_t NANOSECONDS_PER_SECOND{ 1'000'000'000 };
    constexpr std::uint64_t NANOSECONDS_PER_MICROSECOND{ 1'000 };

    // Integer arithmetic, so the printed digits do not depend on a rounding
    // mode the golden strings cannot see.
    auto Seconds(std::int64_t nanoseconds) -> std::string
    {
      auto const negative{ nanoseconds < 0 };
      auto const magnitude{
        negative ? 0u - static_cast<std::uint64_t>(nanoseconds)
                 : static_cast<std::uint64_t>(nanoseconds) };
      return std::format("{}{}.{:06}", negative ? "-" : "",
                         magnitude / NANOSECONDS_PER_SECOND,
                         (magnitude % NANOSECONDS_PER_SECOND)
                           / NANOSECONDS_PER_MICROSECOND);
    }

    auto Details(record::FrameRecord const& record) -> std::string
    {
      return std::format(
        "time={} exact={:016x} dhash={:016x} phash={:016x} change={:.6f}",
        Seconds(record.harness_time), record.hash_exact,
        record.hash_difference, record.hash_perceptual, record.change_amount);
    }

    auto Details(record::InputRecord const& record) -> std::string
    {
      return std::format("port={} pad={:#010x}", record.port, record.pad);
    }

    auto Details(record::WatchRecord const& record) -> std::string
    {
      return std::format("watch={} value={}", record.watch, record.value);
    }

    auto Details(record::DecisionRecord const& record) -> std::string
    {
      return std::format("actor={:?} text={:?}", record.actor, record.text);
    }

    auto Details(record::VerdictRecord const& record) -> std::string
    {
      return std::format("name={:?} outcome={} text={:?}", record.name,
                         record.passed ? "pass" : "fail", record.text);
    }

    auto Details(record::TriggerRecord const& record) -> std::string
    {
      return std::format("name={:?} text={:?}", record.name, record.text);
    }

    auto Details(record::MarkRecord const& record) -> std::string
    {
      if (record.group.empty())
        return std::format("text={:?}", record.text);
      return std::format("text={:?} group={:?}", record.text, record.group);
    }

    auto Details(record::RestoreRecord const& record) -> std::string
    {
      return std::format("name={:?} to line frame {}", record.name,
                         record.to);
    }

    auto Details(record::ResetRecord const&) -> std::string
    {
      return "to power on";
    }
  }

  auto FormatHeaderLine(header::Header const& file_header) -> std::string
  {
    std::chrono::sys_time<std::chrono::nanoseconds> const created{
      std::chrono::nanoseconds{ file_header.creation_time } };
    return std::format("# tash trace v{} created={:%FT%T}Z producer={:?}",
                       file_header.format_version, created,
                       file_header.producer);
  }

  auto FormatRecordLine(record::Record const& record) -> std::string
  {
    return std::format("{:>10} {:<8} {}", record::FrameOf(record),
                       record::NameOf(record::KindOf(record)),
                       std::visit([](auto const& held) {
                         return Details(held); }, record));
  }

  auto Dump(reader::Reader& source, std::ostream& out) -> void
  {
    out << FormatHeaderLine(source.FileHeader()) << '\n';
    while (auto held = source.Next())
      out << FormatRecordLine(*held) << '\n';

    out << std::format("# {} records, {} skipped\n", source.Records(),
                       source.Unknown());
    if (source.Truncated())
      out << "# truncated: the last record is incomplete\n";
  }
}
