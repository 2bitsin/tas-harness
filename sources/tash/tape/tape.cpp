#include "tash/tape/tape.hpp"

#include "tash/tape/pointer.hpp"
#include "tash/tape/transitions.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>
#include <set>

namespace tash::tape::detail::tape
{
  using utilities::Refused;

  auto TapeFrom(std::filesystem::path const& path) -> Result<Tape>
  {
    Tape read{ };
    try
    {
      read = oxbox::serialization::DeserializeFrom<Tape>(path);
    }
    catch (std::exception const& failure)
    {
      return Refused("tape: cannot read '{}': {}", path.string(),
                     failure.what());
    }
    if (Outcome const sound{ Checked(read) }; !sound)
      return Refused("{} in '{}'", sound.error(), path.string());
    return read;
  }

  auto WriteTape(Tape const& written, std::filesystem::path const& path)
    -> Outcome
  {
    if (Outcome const sound{ Checked(written) }; !sound)
      return sound;
    try
    {
      oxbox::serialization::SerializeTo(written, path);
    }
    catch (std::exception const& failure)
    {
      return Refused("tape: cannot write '{}': {}", path.string(),
                     failure.what());
    }
    return { };
  }

  auto Checked(Tape const& written) -> Outcome
  {
    if (written.header.Base() != TimeBase::FRAME)
      return Refused("tape: only the frame time base plays in v1");
    if (written.segments.empty())
      return Refused("tape: '{}' has no segments", written.header.name);

    std::set<std::string> named;
    for (Segment const& segment : written.segments)
    {
      if (segment.name.empty())
        return Refused("tape: a segment has no name");
      if (!named.insert(segment.name).second)
        return Refused("tape: segment '{}' is named twice", segment.name);

      if (Outcome const sound{ anchor::Checked(segment.Waits()) }; !sound)
        return Refused("{}, in segment '{}'", sound.error(), segment.name);

      Result<std::vector<transitions::Transition>> const moves{
        transitions::TransitionsFrom(segment.Lines()) };
      if (!moves)
        return Refused("{}, in segment '{}'", moves.error(), segment.name);

      Result<std::vector<pointer::PointerMove>> const points{
        pointer::PointerMovesFrom(segment.Points()) };
      if (!points)
        return Refused("{}, in segment '{}'", points.error(), segment.name);
    }
    return { };
  }

  auto TimeoutOf(Tape const& written, Segment const& segment) noexcept
    -> std::uint64_t
  {
    return segment.timeout_frames.value_or(written.header.Timeout());
  }

  auto RecoveryOf(Tape const& written, Segment const& segment) noexcept
    -> Recovery
  {
    return segment.on_timeout.value_or(written.header.OnTimeout());
  }
}
