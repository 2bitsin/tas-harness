#include "tash/tape/transitions.hpp"

#include "_lines.hpp"

#include <format>

namespace tash::tape::detail::transitions
{
  using utilities::Refused;

  auto TransitionsFrom(std::string_view text) -> Result<std::vector<Transition>>
  {
    std::vector<Transition> moves;
    for (lines::Line const& line : lines::LinesOf(text))
    {
      std::vector<std::string_view> const words{ lines::Words(line.text) };
      if (words.size() != 3)
        return Refused("tape: line {}: '{}' is not "
                       "<frame> <down|up> <channel>", line.number, line.text);

      Result<std::uint64_t> const frame{
        lines::FrameOf(words[0], line.number) };
      if (!frame)
        return utilities::Forwarded(frame);

      if (words[1] != DOWN_WORD && words[1] != UP_WORD)
        return Refused("tape: line {}: '{}' is not {} or {}", line.number,
                       words[1], DOWN_WORD, UP_WORD);

      Result<channel::Channel> const named{ channel::ChannelFrom(words[2]) };
      if (!named)
        return Refused("tape: line {}: {}", line.number, named.error());

      if (!moves.empty() && *frame < moves.back().frame)
        return Refused("tape: line {}: frame {} is before frame {}",
                       line.number, *frame, moves.back().frame);

      moves.push_back(Transition{ *frame, words[1] == DOWN_WORD, *named });
    }
    return moves;
  }

  auto TextOf(std::span<Transition const> moves) -> std::string
  {
    std::string text;
    for (Transition const& move : moves)
      text += std::format("{:<{}} {:<4} {}\n", move.frame,
                          lines::FRAME_COLUMN,
                          move.down ? DOWN_WORD : UP_WORD,
                          channel::NameOf(move.channel));
    return text;
  }
}
