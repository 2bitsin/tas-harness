#pragma once
// The named states a run leaves behind: `<root>/<name>.state`, the frame it
// was taken at beside it, and the note that says where the run stood.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/python/checkpoint-note.hpp"
#include "tash/session/line.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace tash::python::detail::checkpoint_store
{
  using utilities::Outcome;
  using utilities::Result;

  // A checkpoint outlives its run, so it lives beside the bundles.
  inline constexpr std::string_view CHECKPOINT_ROOT{ "_checkpoints" };
  inline constexpr std::string_view STATE_SUFFIX{ ".state" };
  inline constexpr std::string_view FRAME_SUFFIX{ ".png" };

  // A moment on its way to disk; the state and the frame are borrowed.
  struct CheckpointView
  {
    std::span<std::byte const> state{ };
    bus::FrameView             frame{ };
    std::uint64_t              frames{ 0 };
    double                     seconds{ 0.0 };
    std::optional<session::Line> line{ };
  };

  // The same moment read back, which the run then restores from.
  struct StoredCheckpoint
  {
    std::vector<std::byte>       state{ };
    std::vector<std::byte>       pixels{ };
    bus::FrameDescriptor         at{ };
    std::uint64_t                frames{ 0 };
    std::optional<session::Line> line{ };
  };

  class CheckpointStore
  {
  public:
    explicit CheckpointStore(std::filesystem::path under);

    // Absolute, because a path a tool answers with is the client's to open.
    [[nodiscard]] auto FileOf(std::string_view name) const
      -> Result<std::filesystem::path>;

    [[nodiscard]] auto Write(std::string_view name,
                             CheckpointView const& kept) -> Outcome;

    [[nodiscard]] auto Read(std::string_view name) const
      -> Result<StoredCheckpoint>;

  private:
    std::filesystem::path _under;
  };
}

namespace tash::python
{
  using detail::checkpoint_store::CHECKPOINT_ROOT;
  using detail::checkpoint_store::CheckpointStore;
  using detail::checkpoint_store::CheckpointView;
  using detail::checkpoint_store::StoredCheckpoint;
}
