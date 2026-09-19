#pragma once
// The frames a run ended up having played: every restore in the trace folds
// the frames made since its checkpoint back off the line, and a reset folds
// the whole of it.

#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace tash::trace::detail::line
{
  using utilities::Outcome;
  using utilities::Result;

  // What one record in the trace took off the line: at `frame` the run went
  // back to the state it stood in after `to` frames of the line.
  struct Fold
  {
    std::uint64_t frame{ 0 };
    std::uint64_t to{ 0 };
  };

  class Line
  {
  public:
    [[nodiscard]] static auto Of(std::filesystem::path const& path)
      -> Result<Line>;

    [[nodiscard]] static auto In(reader::Reader& source) -> Result<Line>;

    // Restores naming a run frame instead of a line position cannot fold.
    [[nodiscard]] static auto Foldable(std::uint16_t format_version,
                                       bool restored) -> Outcome;

    // For a caller that already walked: its folds, and the frames it counted.
    [[nodiscard]] static auto Over(std::vector<Fold> folds,
                                   std::uint64_t made) -> Line;

    [[nodiscard]] auto Holds(std::uint64_t frame) const -> bool;

    [[nodiscard]] auto Frames() const -> std::uint64_t;

  private:
    struct Span
    {
      std::uint64_t from{ 0 };
      std::uint64_t to{ 0 };
    };

    auto Grew(std::uint64_t from, std::uint64_t to) -> void;
    auto FoldedTo(std::uint64_t frames) -> void;

    std::vector<Span> _spans{ };
  };
}

namespace tash::trace
{
  using detail::line::Fold;
  using detail::line::Line;
}
