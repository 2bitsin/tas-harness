#pragma once
// The line a run kept: its pad changes in the tape's own transition text and
// the marks that cut it, as a checkpoint hands them to the run after.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tash::session::detail::line
{
  struct LineMark
  {
    friend constexpr auto reflect_scheme(LineMark*);

    std::uint64_t frame{ 0 };
    std::string   name{ };

    auto operator == (LineMark const&) const -> bool = default;
  };

  // Where a checkpoint sits on the line: the frames before it, and the token
  // its source minted for the last of them; zero is a place from elsewhere.
  struct LinePlace
  {
    std::uint64_t frames{ 0 };
    std::uint64_t token{ 0 };

    auto operator == (LinePlace const&) const -> bool = default;
  };

  struct Line
  {
    friend constexpr auto reflect_scheme(Line*);

    std::uint64_t         frames{ 0 };
    std::string           transitions{ };
    std::vector<LineMark> marks{ };

    auto operator == (Line const&) const -> bool = default;
  };

  // Only the run's tape recording knows the line, and it lives above whoever
  // writes a checkpoint down, so the writer asks through this.
  class LineSource
  {
  public:
    LineSource()                                       = default;
    virtual ~LineSource()                              = default;
    LineSource(LineSource const&)                      = delete;
    auto operator = (LineSource const&) -> LineSource& = delete;

    // Nothing when the run cannot write what it played as one line.
    [[nodiscard]] virtual auto LineUpTo(std::uint64_t frames) const
      -> std::optional<Line> = 0;

    // Where a checkpoint taken now sits; the frame counter cannot say.
    [[nodiscard]] virtual auto Place() const -> LinePlace = 0;
  };
}

namespace tash::session
{
  using detail::line::Line;
  using detail::line::LineMark;
  using detail::line::LinePlace;
  using detail::line::LineSource;
}
