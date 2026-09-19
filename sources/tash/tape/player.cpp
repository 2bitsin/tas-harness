#include "tash/tape/player.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

namespace tash::tape::detail::player
{
  using utilities::Refused;

  namespace
  {
    [[nodiscard]] auto LengthOf(
      std::vector<transitions::Transition> const& moves,
      std::optional<std::uint64_t> asked) -> std::uint64_t
    {
      std::uint64_t const last{ moves.empty() ? 0 : moves.back().frame + 1 };
      return std::max(asked.value_or(0), last);
    }
    constexpr int SEGMENT_COLUMN{ 16 };

    auto Report(std::string_view name, std::uint64_t frames,
                PlayCounts const& before, PlayCounts const& after,
                std::ostream& report) -> void
    {
      report << std::format(
        "{:<{}}anchor after {:<6} {} transitions over {} frames",
        name, SEGMENT_COLUMN, after.waited - before.waited,
        after.transitions - before.transitions, frames);
      if (after.retries != before.retries)
        report << std::format(", {} retries", after.retries - before.retries);
      report << "\n";
    }
  }

  Player::Player(tape::TapeHeader header, std::vector<Ready> segments)
    : _header{ std::move(header) }, _segments{ std::move(segments) }
  { }

  auto Player::Of(std::filesystem::path const& path) -> Result<Player>
  {
    Result<tape::Tape> read{ tape::TapeFrom(path) };
    if (!read)
      return utilities::Forwarded(read);
    return For(std::move(*read), path);
  }

  auto Player::For(tape::Tape written, std::filesystem::path const& beside)
    -> Result<Player>
  {
    if (Outcome const sound{ tape::Checked(written) }; !sound)
      return utilities::Forwarded(sound);

    std::vector<Ready> ready;
    ready.reserve(written.segments.size());
    for (tape::Segment const& segment : written.segments)
    {
      Result<anchor_check::AnchorCheck> waiting{
        anchor_check::AnchorCheck::For(segment.Waits(), beside) };
      if (!waiting)
        return Refused("{}, in segment '{}'", waiting.error(), segment.name);

      Result<std::vector<transitions::Transition>> moves{
        transitions::TransitionsFrom(segment.Lines()) };
      if (!moves)
        return Refused("{}, in segment '{}'", moves.error(), segment.name);

      // A libretro session is held by pads alone, so a tape a linked target
      // recorded plays back where that target runs, not here.
      if (!segment.Points().empty()
          || std::ranges::any_of(*moves, [](transitions::Transition const& m)
             { return m.channel.device != channel::Device::PAD; }))
        return Refused("tape: segment '{}' moves a pointer, which no session "
                       "in v1 has", segment.name);

      std::uint64_t const frames{ LengthOf(*moves, segment.frames) };
      ready.push_back(Ready{ segment.name, std::move(*waiting),
                             std::move(*moves),
                             tape::TimeoutOf(written, segment),
                             tape::RecoveryOf(written, segment), frames });
    }
    return Player{ std::move(written.header), std::move(ready) };
  }

  auto Player::Play(session::Session& session, PlayOptions options)
    -> Result<PlayCounts>
  {
    PlayCounts counts{ };
    for (Ready const& segment : _segments)
    {
      PlayCounts const before{ counts };
      if (Outcome const reached{ Reach(segment, session, options, counts) };
          !reached)
        return utilities::Forwarded(reached);
      if (Outcome const played{ Apply(segment, session, counts) };
          !played)
        return utilities::Forwarded(played);
      ++counts.segments;
      if (options.marks != nullptr)
        options.marks->OnMark(segment.name);
      if (options.report != nullptr)
        Report(segment.name, segment.frames, before, counts,
               *options.report);
    }
    return counts;
  }

  auto Player::Reach(Ready const& segment, session::Session& session,
                     PlayOptions const& options, PlayCounts& counts)
    -> Outcome
  {
    bool const retrying{ segment.recovery == tape::Recovery::RETRY };
    std::vector<std::byte> state{ };
    if (retrying)
    {
      Result<std::vector<std::byte>> taken{ session.SaveState() };
      if (!taken)
        return Refused("tape: segment '{}' asks to retry and the core "
                       "cannot save: {}", segment.name, taken.error());
      state = std::move(*taken);
    }

    std::uint32_t const attempts{ retrying ? _header.Retries() + 1u : 1u };
    for (std::uint32_t attempt{ 0 }; attempt < attempts; ++attempt)
    {
      if (attempt > 0)
      {
        if (Outcome const back{ session.LoadState(state) }; !back)
          return Refused("tape: segment '{}' cannot go back to its start: {}",
                         segment.name, back.error());
        ++counts.retries;
      }

      Result<bool> const held{ Wait(segment, session, options, counts) };
      if (!held)
        return utilities::Forwarded(held);
      if (*held)
        return { };
    }
    return Refused("tape: segment '{}' did not see its anchor ({}) within "
                   "{} frames", segment.name, segment.anchor.Wording(),
                   segment.timeout);
  }

  auto Player::Wait(Ready const& segment, session::Session& session,
                    PlayOptions const& options, PlayCounts& counts)
    -> Result<bool>
  {
    if (segment.anchor.Immediate())
      return true;

    for (std::uint64_t waited{ 0 }; ; ++waited)
    {
      if (std::optional<bus::FrameView> const latest{
            session.Video().Latest() })
      {
        Result<bool> const holds{ segment.anchor.Holds(*latest,
                                                       options.watches) };
        if (!holds)
          return utilities::Forwarded(holds);
        if (*holds)
          return true;
      }
      if (waited >= segment.timeout)
        return false;
      session.Step(1);
      ++counts.frames;
      ++counts.waited;
    }
  }

  auto Player::Apply(Ready const& segment, session::Session& session,
                     PlayCounts& counts) -> Outcome
  {
    std::size_t next{ 0 };
    for (std::uint64_t at{ 0 }; at < segment.frames; ++at)
    {
      while (next < segment.moves.size() && segment.moves[next].frame == at)
      {
        transitions::Transition const& move{ segment.moves[next] };
        std::uint32_t const held{ session.Pad(move.channel.port) };
        std::uint32_t const pad{ move.down ? held | move.channel.Mask()
                                           : held & ~move.channel.Mask() };
        session.HoldPad(move.channel.port, pad);
        ++counts.transitions;
        ++next;
      }
      session.Step(1);
      ++counts.frames;
    }
    return { };
  }
}
