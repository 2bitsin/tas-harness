#pragma once

#include "tash/trace/format.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace tash::trace::detail::record
{
  using format::Kind;

  struct FrameRecord
  {
    static constexpr Kind KIND{ Kind::FRAME };

    std::uint64_t frame{ 0 };
    std::int64_t  harness_time{ 0 };  // nanoseconds since the run started
    std::uint64_t hash_exact{ 0 };
    std::uint64_t hash_difference{ 0 };
    std::uint64_t hash_perceptual{ 0 };
    double        change_amount{ 0.0 };

    auto operator == (FrameRecord const&) const -> bool = default;
  };

  struct InputRecord
  {
    static constexpr Kind KIND{ Kind::INPUT };

    std::uint64_t frame{ 0 };
    std::uint8_t  port{ 0 };
    std::uint32_t pad{ 0 };

    auto operator == (InputRecord const&) const -> bool = default;
  };

  struct WatchRecord
  {
    static constexpr Kind KIND{ Kind::WATCH };

    std::uint64_t frame{ 0 };
    std::uint32_t watch{ 0 };
    std::int64_t  value{ 0 };

    auto operator == (WatchRecord const&) const -> bool = default;
  };

  struct DecisionRecord
  {
    static constexpr Kind KIND{ Kind::DECISION };

    std::uint64_t frame{ 0 };
    std::string   actor{ };
    std::string   text{ };

    auto operator == (DecisionRecord const&) const -> bool = default;
  };

  struct VerdictRecord
  {
    static constexpr Kind KIND{ Kind::VERDICT };

    std::uint64_t frame{ 0 };
    std::string   name{ };
    bool          passed{ false };
    std::string   text{ };

    auto operator == (VerdictRecord const&) const -> bool = default;
  };

  struct TriggerRecord
  {
    static constexpr Kind KIND{ Kind::TRIGGER };

    std::uint64_t frame{ 0 };
    std::string   name{ };
    std::string   text{ };

    auto operator == (TriggerRecord const&) const -> bool = default;
  };

  struct MarkRecord
  {
    static constexpr Kind KIND{ Kind::MARK };

    std::uint64_t frame{ 0 };
    std::string   text{ };
    std::string   group{ };

    auto operator == (MarkRecord const&) const -> bool = default;
  };

  // Where a restore put the line back: `frame` is the harness frame it
  // happened at and `to` the frames of the line the checkpoint was taken on,
  // which is what the line folds back to and not a harness frame at all.
  struct RestoreRecord
  {
    static constexpr Kind KIND{ Kind::RESTORE };

    std::uint64_t frame{ 0 };
    std::uint64_t to{ 0 };
    std::string   name{ };

    auto operator == (RestoreRecord const&) const -> bool = default;
  };

  // Where the run power-cycled: the core went back to how it boots and the
  // line the run had played is gone.
  struct ResetRecord
  {
    static constexpr Kind KIND{ Kind::RESET };

    std::uint64_t frame{ 0 };

    auto operator == (ResetRecord const&) const -> bool = default;
  };

  using Record = std::variant<FrameRecord, InputRecord, WatchRecord,
                              DecisionRecord, VerdictRecord, TriggerRecord,
                              MarkRecord, RestoreRecord, ResetRecord>;

  [[nodiscard]] inline auto KindOf(Record const& record) noexcept -> Kind
  {
    return std::visit([](auto const& held) { return held.KIND; }, record);
  }

  [[nodiscard]] inline auto FrameOf(Record const& record) noexcept
    -> std::uint64_t
  {
    return std::visit([](auto const& held) { return held.frame; }, record);
  }

  [[nodiscard]] constexpr auto NameOf(Kind kind) noexcept -> char const*
  {
    switch (kind)
    {
      case Kind::FRAME:    return "frame";
      case Kind::INPUT:    return "input";
      case Kind::WATCH:    return "watch";
      case Kind::DECISION: return "decision";
      case Kind::VERDICT:  return "verdict";
      case Kind::TRIGGER:  return "trigger";
      case Kind::MARK:     return "mark";
      case Kind::RESTORE:  return "restore";
      case Kind::RESET:    return "reset";
    }
    return "unknown";
  }
}

namespace tash::trace
{
  using detail::record::DecisionRecord;
  using detail::record::FrameOf;
  using detail::record::FrameRecord;
  using detail::record::InputRecord;
  using detail::record::KindOf;
  using detail::record::MarkRecord;
  using detail::record::NameOf;
  using detail::record::Record;
  using detail::record::ResetRecord;
  using detail::record::RestoreRecord;
  using detail::record::TriggerRecord;
  using detail::record::VerdictRecord;
  using detail::record::WatchRecord;
}
