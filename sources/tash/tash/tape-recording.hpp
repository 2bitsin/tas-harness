#pragma once
// The tape a run leaves behind: the pads it held and the marks it made, cut
// into a segment per mark and folded back whenever the run rewound.

#include "tash/recorder/bundle.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/session/line.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/mark-observer.hpp"
#include "tash/tape/tape.hpp"
#include "tash/utilities/outcome.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::cli::detail::tape_recording
{
  using utilities::Outcome;

  // A line frame's token is the harness frame that made it, counted from
  // one; above this it is the seed the line began as, which no frame of
  // this run made.
  inline constexpr std::uint64_t SEED_TOKEN{ std::uint64_t{ 1 } << 63 };

  class TapeRecording : public session::FrameObserver,
                        public session::LineSource,
                        public tape::MarkObserver
  {
  public:
    explicit TapeRecording(session::Session const& live) : _live{ &live } { }

    auto OnFrame(bus::FrameView const& frame, std::int64_t harness_time)
      -> void override;

    auto OnRewind(session::Rewind const& back) -> void override;

    auto OnReset(session::Reset const&) -> void override;

    auto OnMark(std::string_view text) -> void override;

    [[nodiscard]] auto LineUpTo(std::uint64_t frames) const
      -> std::optional<session::Line> override;

    [[nodiscard]] auto Place() const -> session::LinePlace override;

    // The frames the tape holds: what the restores and the resets left.
    [[nodiscard]] auto Frames() const noexcept -> std::uint64_t
    { return _line; }

    // Runs after the trace closed; the anchors are read from it.
    [[nodiscard]] auto Write(recorder::Bundle const& bundle,
                             tape::TapeHeader header) const -> Outcome;

  private:
    struct Change
    {
      std::uint64_t frame{ 0 };
      std::size_t   port{ 0 };
      std::uint32_t pad{ 0 };
    };

    struct Cut
    {
      std::uint64_t frame{ 0 };
      std::string   name{ };
    };

    // A frame of the line and the harness frame that produced it, run
    // together: one of these starts where the one before it was folded.
    struct Stretch
    {
      std::uint64_t harness{ 0 };
      std::uint64_t line{ 0 };
      std::uint64_t frames{ 0 };
    };

    [[nodiscard]] auto HarnessAt(std::uint64_t line) const
      -> std::optional<std::uint64_t>;
    [[nodiscard]] auto LineAt(std::uint64_t harness) const
      -> std::optional<std::uint64_t>;

    [[nodiscard]] auto TokenAt(std::uint64_t line) const -> std::uint64_t;

    // Whether the line this place was taken on is still this run's line.
    [[nodiscard]] auto Holds(session::LinePlace const& place) const -> bool;

    auto Folded(std::uint64_t frames) -> void;

    // The mirror of a fold: the line becomes this run's, pads from its changes.
    auto Seeded(session::Line const& before) -> void;

    [[nodiscard]] auto Kept(std::uint64_t frames) const -> session::Line;

    session::Session const*                    _live;
    std::vector<Change>                        _changes{ };
    std::vector<Cut>                           _marks{ };
    std::vector<Stretch>                       _stretches{ };
    std::uint64_t                              _line{ 0 };

    // The head of the line a seed brought, and which seed brought it.
    std::uint64_t                              _seeded{ 0 };
    std::uint64_t                              _seeds{ 0 };
    std::array<std::uint32_t, session::PORTS>  _pads{ };
    std::string                                _adrift{ };

    // Where on the line the run lost it, so a fold past that point recovers.
    std::optional<std::uint64_t>               _adrift_from{ };
  };
}

namespace tash::cli
{
  using detail::tape_recording::TapeRecording;
}
