#include "tash/python/checkpoint-note.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>

namespace tash::python::detail::checkpoint_note
{
  using utilities::Refused;

  auto WriteNote(CheckpointNote const& note,
                 std::filesystem::path const& path) -> Outcome
  {
    try
    {
      oxbox::serialization::SerializeTo(note, path);
    }
    catch (std::exception const& failure)
    {
      return Refused("python: cannot write {}: {}", path.string(),
                     failure.what());
    }
    return { };
  }

  auto NoteFrom(std::filesystem::path const& path)
    -> std::optional<CheckpointNote>
  {
    if (!std::filesystem::is_regular_file(path))
      return { };
    try
    {
      return oxbox::serialization::DeserializeFrom<CheckpointNote>(path);
    }
    catch (std::exception const&)
    {
      return { };
    }
  }
}
