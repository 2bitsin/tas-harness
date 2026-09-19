#include "tash/python/checkpoint-store.hpp"

#include "tash/recorder/frame-png.hpp"

#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>
#include <utility>

namespace tash::python::detail::checkpoint_store
{
  using utilities::Forwarded;
  using utilities::Refused;

  namespace
  {
    auto Beside(std::filesystem::path const& state, std::string_view suffix)
      -> std::filesystem::path
    {
      return std::filesystem::path{ state }.replace_extension(suffix);
    }

    auto WriteState(std::filesystem::path const& file,
                    std::span<std::byte const> state) -> Outcome
    {
      std::error_code failed;
      std::filesystem::create_directories(file.parent_path(), failed);
      std::ofstream writing{ file, std::ios::binary | std::ios::trunc };
      if (!writing)
        return Refused("python: cannot write {}", file.string());
      writing.write(reinterpret_cast<char const*>(state.data()),
                    static_cast<std::streamsize>(state.size()));
      if (!writing)
        return Refused("python: cannot write {}", file.string());
      return { };
    }

    auto ReadState(std::filesystem::path const& file)
      -> Result<std::vector<std::byte>>
    {
      std::ifstream reading{ file, std::ios::binary };
      if (!reading)
        return Refused("python: no checkpoint at {}", file.string());
      std::vector<char> const held{ std::istreambuf_iterator<char>{ reading },
                                    std::istreambuf_iterator<char>{ } };
      std::vector<std::byte> state(held.size());
      std::memcpy(state.data(), held.data(), held.size());
      return state;
    }
  }

  CheckpointStore::CheckpointStore(std::filesystem::path under)
  : _under{ std::move(under) }
  {
  }

  auto CheckpointStore::FileOf(std::string_view name) const
    -> Result<std::filesystem::path>
  {
    if (name.empty() || name.find('/') != std::string_view::npos)
      return Refused("python: '{}' is not a checkpoint name", name);
    return std::filesystem::absolute(
      _under / std::format("{}{}", name, STATE_SUFFIX));
  }

  auto CheckpointStore::Write(std::string_view name,
                              CheckpointView const& kept) -> Outcome
  {
    Result<std::filesystem::path> const file{ FileOf(name) };
    if (!file)
      return Forwarded(file);
    if (Outcome const written{ WriteState(*file, kept.state) }; !written)
      return written;

    // A core does not render on unserialize, so the frame the moment was
    // taken at goes beside the state and comes back on the bus with it.
    if (!kept.frame.pixels.empty())
      if (Outcome const drawn{ recorder::WritePng(
            kept.frame, Beside(*file, FRAME_SUFFIX)) }; !drawn)
        return drawn;

    CheckpointNote note{ };
    note.frames = kept.frames;
    note.frame = kept.frame.descriptor.number;
    note.seconds = kept.seconds;
    note.line = kept.line;
    return WriteNote(note, Beside(*file, NOTE_SUFFIX));
  }

  auto CheckpointStore::Read(std::string_view name) const
    -> Result<StoredCheckpoint>
  {
    Result<std::filesystem::path> const file{ FileOf(name) };
    if (!file)
      return Forwarded(file);
    Result<std::vector<std::byte>> state{ ReadState(*file) };
    if (!state)
      return Forwarded(state);

    StoredCheckpoint kept;
    kept.state = std::move(*state);
    if (std::optional<CheckpointNote> const note{
          NoteFrom(Beside(*file, NOTE_SUFFIX)) }; note)
    {
      kept.frames = note->frames;
      kept.line = note->line;
      kept.at.number = note->frame;
      kept.at.harness_seconds = note->seconds;
    }
    if (Result<recorder::PngFrame> frame{
          recorder::ReadPng(Beside(*file, FRAME_SUFFIX)) }; frame)
    {
      kept.pixels = std::move(frame->pixels);

      // ReadPng packs the rows, so the shape is the picture's own and only
      // the note can say which frame it is.
      kept.at.width = frame->width;
      kept.at.height = frame->height;
      kept.at.pitch = std::size_t{ frame->width } * bus::BYTES_PER_PIXEL;
    }
    return kept;
  }
}
