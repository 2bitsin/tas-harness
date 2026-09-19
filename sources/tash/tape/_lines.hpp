#pragma once
// The line shape both blocks a segment carries are written in: a frame
// number, then words, with blanks and `#` lines dropped and every line's
// number kept for the refusal.

#include "tash/tape/names.hpp"
#include "tash/utilities/outcome.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tash::tape::detail::lines
{
  using names::COMMENT_MARK;
  using utilities::Refused;
  using utilities::Result;

  inline constexpr std::string_view BLANKS{ " \t\r" };

  // Wide enough for the frame numbers a segment reaches.
  inline constexpr int FRAME_COLUMN{ 6 };

  struct Line
  {
    std::string_view text{ };
    std::size_t      number{ 0 };
  };

  [[nodiscard]] inline auto Trimmed(std::string_view line) -> std::string_view
  {
    auto const first{ line.find_first_not_of(BLANKS) };
    if (first == std::string_view::npos)
      return { };
    return line.substr(first, line.find_last_not_of(BLANKS) + 1 - first);
  }

  [[nodiscard]] inline auto LinesOf(std::string_view text) -> std::vector<Line>
  {
    std::vector<Line> found;
    std::size_t       number{ 0 };
    std::size_t       at{ 0 };
    while (at <= text.size())
    {
      ++number;
      auto const end{ text.find('\n', at) };
      std::string_view const raw{ text.substr(
        at, end == std::string_view::npos ? std::string_view::npos
                                          : end - at) };
      at = end == std::string_view::npos ? text.size() + 1 : end + 1;

      std::string_view const trimmed{ Trimmed(raw) };
      if (trimmed.empty() || trimmed.front() == COMMENT_MARK)
        continue;
      found.push_back(Line{ trimmed, number });
    }
    return found;
  }

  [[nodiscard]] inline auto Words(std::string_view line)
    -> std::vector<std::string_view>
  {
    std::vector<std::string_view> found;
    std::size_t at{ 0 };
    while (at < line.size())
    {
      auto const start{ line.find_first_not_of(BLANKS, at) };
      if (start == std::string_view::npos)
        break;
      auto const end{ line.find_first_of(BLANKS, start) };
      found.push_back(line.substr(start, end == std::string_view::npos
                                           ? std::string_view::npos
                                           : end - start));
      at = end == std::string_view::npos ? line.size() : end;
    }
    return found;
  }

  template <typename Number>
  [[nodiscard]] auto NumberOf(std::string_view word) -> Result<Number>
  {
    Number value{ 0 };
    auto const [end, failed]{ std::from_chars(
      word.data(), word.data() + word.size(), value) };
    if (failed != std::errc{ } || end != word.data() + word.size())
      return Refused("not a number");
    return value;
  }

  [[nodiscard]] inline auto FrameOf(std::string_view word, std::size_t line)
    -> Result<std::uint64_t>
  {
    Result<std::uint64_t> const frame{ NumberOf<std::uint64_t>(word) };
    if (!frame)
      return Refused("tape: line {}: '{}' is not a frame number", line, word);
    return *frame;
  }
}
