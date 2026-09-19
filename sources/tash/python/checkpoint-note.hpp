#pragma once
// What a state file cannot say about itself: where the run stood when it was
// taken. The png beside it carries the frame, and its shape.

#include "tash/session/line.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tash::python::detail::checkpoint_note
{
  using utilities::Outcome;

  inline constexpr std::string_view NOTE_SUFFIX{ ".yaml" };

  struct CheckpointNote
  {
    friend constexpr auto reflect_scheme(CheckpointNote*);

    std::uint64_t frames{ 0 };
    std::uint64_t frame{ 0 };
    double        seconds{ 0.0 };

    // Absent for a note written before notes carried one, and for a state
    // taken on a line its own run could not write.
    std::optional<session::Line> line{ };

    auto operator == (CheckpointNote const&) const -> bool = default;
  };

  [[nodiscard]] auto WriteNote(CheckpointNote const& note,
                               std::filesystem::path const& path) -> Outcome;

  // Nothing, rather than a refusal, for a state written before notes were.
  [[nodiscard]] auto NoteFrom(std::filesystem::path const& path)
    -> std::optional<CheckpointNote>;
}

namespace tash::python
{
  using detail::checkpoint_note::CheckpointNote;
  using detail::checkpoint_note::NoteFrom;
  using detail::checkpoint_note::NOTE_SUFFIX;
  using detail::checkpoint_note::WriteNote;
}
