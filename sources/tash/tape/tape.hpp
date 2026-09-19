#pragma once
// The file itself: a header and its segments, as a human reads and edits
// them. Anchors and transitions are what a segment is made of.

#include "tash/tape/anchor.hpp"
#include "tash/utilities/outcome.hpp"

#include <_buildutil/reflect.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::tape
{
  using utilities::Outcome;
  using utilities::Result;

  enum class TimeBase
  {
    FRAME _Label(frame),
    WALL  _Label(wall)
  };

  constexpr auto reflect_scheme(TimeBase*);

  enum class Recovery
  {
    FAIL  _Label(fail),
    RETRY _Label(retry)
  };

  constexpr auto reflect_scheme(Recovery*);

  // Ten seconds at sixty frames: longer than any load a v1 target has, short
  // enough that a wrong anchor is a failed test and not a hung run.
  inline constexpr std::uint64_t DEFAULT_TIMEOUT_FRAMES{ 600 };

  inline constexpr std::uint32_t DEFAULT_RETRIES{ 1 };

  // oxbox writes a std::filesystem::path member but does not read one
  // back, so a path in a tape is a string.
  struct TapeHeader
  {
    friend constexpr auto reflect_scheme(TapeHeader*);

    std::string                  name{ };
    std::optional<std::string>   profile{ };
    std::optional<std::string>   core{ };
    std::optional<TimeBase>      time_base{ };
    std::optional<std::uint64_t> timeout_frames{ };
    std::optional<Recovery>      on_timeout{ };
    std::optional<std::uint32_t> retries{ };

    [[nodiscard]] auto Base() const noexcept -> TimeBase
    { return time_base.value_or(TimeBase::FRAME); }

    [[nodiscard]] auto Timeout() const noexcept -> std::uint64_t
    { return timeout_frames.value_or(DEFAULT_TIMEOUT_FRAMES); }

    [[nodiscard]] auto OnTimeout() const noexcept -> Recovery
    { return on_timeout.value_or(Recovery::FAIL); }

    [[nodiscard]] auto Retries() const noexcept -> std::uint32_t
    { return retries.value_or(DEFAULT_RETRIES); }

    auto operator == (TapeHeader const&) const -> bool = default;
  };

  struct Segment
  {
    friend constexpr auto reflect_scheme(Segment*);

    std::string                     name{ };
    std::optional<anchor::Anchor>   anchor{ };
    std::optional<std::uint64_t>    timeout_frames{ };
    std::optional<Recovery>         on_timeout{ };

    // How long the segment runs for; without it the segment ends one frame
    // after its last transition, which is what a recorded one carries.
    std::optional<std::uint64_t>    frames{ };

    std::optional<std::string>      transitions{ };

    // Where the pointer went, when the target has one; a pad-only tape
    // carries no such block at all.
    std::optional<std::string>      pointer{ };

    [[nodiscard]] auto Waits() const -> anchor::Anchor
    { return anchor.value_or(anchor::Anchor{ }); }

    [[nodiscard]] auto Lines() const -> std::string_view
    { return transitions ? std::string_view{ *transitions }
                         : std::string_view{ }; }

    [[nodiscard]] auto Points() const -> std::string_view
    { return pointer ? std::string_view{ *pointer }
                     : std::string_view{ }; }

    auto operator == (Segment const&) const -> bool = default;
  };

  struct Tape
  {
    friend constexpr auto reflect_scheme(Tape*);

    TapeHeader           header{ };
    std::vector<Segment> segments{ };

    auto operator == (Tape const&) const -> bool = default;
  };

  [[nodiscard]] auto TapeFrom(std::filesystem::path const& path)
    -> Result<Tape>;

  [[nodiscard]] auto WriteTape(Tape const& written,
                               std::filesystem::path const& path) -> Outcome;

  // Every segment named once, every anchor answering its kind, every
  // transitions block parsing.
  [[nodiscard]] auto Checked(Tape const& written) -> Outcome;

  [[nodiscard]] auto TimeoutOf(Tape const& written, Segment const& segment)
    noexcept -> std::uint64_t;

  [[nodiscard]] auto RecoveryOf(Tape const& written, Segment const& segment)
    noexcept -> Recovery;
}

namespace tash::tape
{
  using detail::tape::Checked;
  using detail::tape::DEFAULT_RETRIES;
  using detail::tape::DEFAULT_TIMEOUT_FRAMES;
  using detail::tape::Recovery;
  using detail::tape::RecoveryOf;
  using detail::tape::Segment;
  using detail::tape::Tape;
  using detail::tape::TapeFrom;
  using detail::tape::TapeHeader;
  using detail::tape::TimeBase;
  using detail::tape::TimeoutOf;
  using detail::tape::WriteTape;
}
