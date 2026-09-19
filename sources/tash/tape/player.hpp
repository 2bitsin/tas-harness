#pragma once
// A tape played into a session in frame base: wait for each segment's
// anchor, then hold the pads the segment's transitions ask for, frame by
// frame.

#include "tash/session/session.hpp"
#include "tash/tape/anchor-check.hpp"
#include "tash/tape/mark-observer.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tape/transitions.hpp"
#include "tash/tape/watch-source.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace tash::tape::detail::player
{
  using utilities::Outcome;
  using utilities::Result;

  struct PlayOptions
  {
    // Needed only by a tape with a watch anchor in it.
    watch_source::WatchSource const* watches{ nullptr };

    // One line per segment as it plays, for a run that wants a transcript.
    std::ostream*                    report{ nullptr };

    // Told each segment's name as the player reaches it, so a recorded
    // replay carries the marks the tape was cut on.
    MarkObserver*                    marks{ nullptr };
  };

  struct PlayCounts
  {
    std::uint64_t segments{ 0 };
    std::uint64_t frames{ 0 };
    std::uint64_t waited{ 0 };
    std::uint64_t transitions{ 0 };
    std::uint64_t retries{ 0 };

    auto operator == (PlayCounts const&) const -> bool = default;
  };

  class Player
  {
  public:
    [[nodiscard]] static auto Of(std::filesystem::path const& path)
      -> Result<Player>;

    // `beside` is the tape's own path, which is what an image anchor and a
    // retry's save state are relative to.
    [[nodiscard]] static auto For(tape::Tape written,
                                  std::filesystem::path const& beside)
      -> Result<Player>;

    [[nodiscard]] auto Play(session::Session& session,
                            PlayOptions options = { }) -> Result<PlayCounts>;

    [[nodiscard]] auto Segments() const noexcept -> std::size_t
    { return _segments.size(); }

  private:
    struct Ready
    {
      std::string                          name;
      anchor_check::AnchorCheck            anchor;
      std::vector<transitions::Transition> moves;
      std::uint64_t                        timeout{ 0 };
      tape::Recovery                       recovery{ tape::Recovery::FAIL };
      std::uint64_t                        frames{ 0 };
    };

    Player(tape::TapeHeader header, std::vector<Ready> segments);

    [[nodiscard]] auto Reach(Ready const& segment, session::Session& session,
                             PlayOptions const& options, PlayCounts& counts)
      -> Outcome;

    [[nodiscard]] auto Wait(Ready const& segment, session::Session& session,
                            PlayOptions const& options, PlayCounts& counts)
      -> Result<bool>;

    [[nodiscard]] auto Apply(Ready const& segment, session::Session& session,
                             PlayCounts& counts) -> Outcome;

    tape::TapeHeader   _header;
    std::vector<Ready> _segments;
  };
}

namespace tash::tape
{
  using detail::player::PlayCounts;
  using detail::player::Player;
  using detail::player::PlayOptions;
}
