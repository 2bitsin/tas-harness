#include "tash/tape/pointer.hpp"

#include "_lines.hpp"

#include <format>

namespace tash::tape::detail::pointer
{
  using utilities::Refused;

  namespace
  {
    [[nodiscard]] auto PointOf(std::string_view word, std::size_t line)
      -> Result<PointerMove>
    {
      auto const comma{ word.find(POINT_SEPARATOR) };
      if (comma == std::string_view::npos)
        return Refused("tape: line {}: '{}' is not a point, which is <x>,<y>",
                       line, word);
      Result<std::int32_t> const x{
        lines::NumberOf<std::int32_t>(word.substr(0, comma)) };
      Result<std::int32_t> const y{
        lines::NumberOf<std::int32_t>(word.substr(comma + 1)) };
      if (!x || !y)
        return Refused("tape: line {}: '{}' is not a point, which is <x>,<y>",
                       line, word);
      return PointerMove{ 0, 0, *x, *y };
    }
  }

  auto PointerMovesFrom(std::string_view text)
    -> Result<std::vector<PointerMove>>
  {
    std::vector<PointerMove> moves;
    for (lines::Line const& line : lines::LinesOf(text))
    {
      std::vector<std::string_view> const words{ lines::Words(line.text) };
      if (words.size() != 3)
        return Refused("tape: line {}: '{}' is not "
                       "<frame> <device> <x>,<y>", line.number, line.text);

      Result<std::uint64_t> const frame{
        lines::FrameOf(words[0], line.number) };
      if (!frame)
        return utilities::Forwarded(frame);

      Result<channel::Port> const port{ channel::PortFrom(words[1]) };
      if (!port)
        return Refused("tape: line {}: {}", line.number, port.error());
      if (port->device != names::Device::MOUSE)
        return Refused("tape: line {}: '{}' is not a pointer, which is m1 to "
                       "m{}", line.number, words[1], names::MICE);

      Result<PointerMove> const point{ PointOf(words[2], line.number) };
      if (!point)
        return utilities::Forwarded(point);

      if (!moves.empty() && *frame < moves.back().frame)
        return Refused("tape: line {}: frame {} is before frame {}",
                       line.number, *frame, moves.back().frame);

      moves.push_back(PointerMove{ *frame, port->number, point->x, point->y });
    }
    return moves;
  }

  auto TextOf(std::span<PointerMove const> moves) -> std::string
  {
    std::string text;
    for (PointerMove const& move : moves)
      text += std::format("{:<{}} {:<4} {}{}{}\n", move.frame,
                          lines::FRAME_COLUMN,
                          channel::NameOf(channel::Port{
                            move.port, names::Device::MOUSE }),
                          move.x, POINT_SEPARATOR, move.y);
    return text;
  }
}
