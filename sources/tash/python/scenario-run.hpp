#pragma once
// What `tash.run` is: the session the cli opened, plus the places a
// scenario's shots, checkpoints, marks and verdicts go.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/perception/change-amount.hpp"
#include "tash/perception/colour.hpp"
#include "tash/perception/region.hpp"
#include "tash/python/checkpoint-store.hpp"
#include "tash/python/frame-budget.hpp"
#include "tash/python/tape-sink.hpp"
#include "tash/python/watch-values.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/encoder-settings.hpp"
#include "tash/recorder/verdicts-writer.hpp"
#include "tash/session/line.hpp"
#include "tash/session/restore-probe.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/mark-observer.hpp"
#include "tash/tape/player.hpp"
#include "tash/tape/predicate.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/outcome.hpp"

#include <oxbox/platform/scratch-area.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::python::detail::scenario_run
{
  using utilities::Outcome;
  using utilities::Result;

  inline constexpr std::string_view SHOT_PREFIX{ "shot" };
  inline constexpr std::string_view SHOT_SUFFIX{ ".png" };
  inline constexpr int SHOT_DIGITS{ 4 };

  inline constexpr std::uint64_t STABLE_TIMEOUT_FRAMES{ 600 };

  inline constexpr std::string_view SHOTS_PURPOSE{ "scenario-shots" };

  // A scratch checkpoint is consumed inside the call that takes it.
  enum class CheckpointKind { NAMED, SCRATCH };

  struct CheckpointKept
  {
    std::uint64_t frames{ 0 };
    std::size_t   bytes{ 0 };

    // Nothing when the run keeps its checkpoints in the process alone.
    std::optional<std::filesystem::path> file{ };
  };

  struct Restored
  {
    std::optional<std::filesystem::path> file{ };         // read from disk
    std::optional<std::filesystem::path> passed_over{ };  // newer, not read
  };

  // What one observe() answers, as the bindings and the mcp module both
  // need it: a dict on one side and a text block on the other, read once.
  struct Observation
  {
    std::uint64_t frame{ 0 };
    double        time{ 0.0 };

    // The latest frame's own geometry, which is what a crop must fit in.
    std::uint32_t width{ 0 };
    std::uint32_t height{ 0 };
    std::uint64_t exact{ 0 };
    std::uint64_t difference{ 0 };
    std::uint64_t perceptual{ 0 };
    double        change{ 0.0 };

    std::array<std::vector<std::string>, session::PORTS> pads{ };
    std::vector<std::pair<std::string, std::optional<std::int64_t>>>
                  watches{ };
  };

  struct RunParts
  {
    session::Session*         session{ nullptr };

    trace::Writer*            trace{ nullptr };
    recorder::Bundle const*   bundle{ nullptr };
    recorder::VerdictsWriter* verdicts{ nullptr };
    WatchValues const*        watches{ nullptr };

    // Where a played tape reports its segments, the cli's --tape stream.
    std::ostream*             report{ nullptr };

    // Told of every mark, so a tape cuts on the line, not on the trace.
    tape::MarkObserver*       marks{ nullptr };

    // Asked for the line a checkpoint written to disk carries with it.
    session::LineSource*      line{ nullptr };

    TapeSink*                 tape{ nullptr };

    // `<root>/<rom hash>`; empty keeps them in the process alone.
    std::filesystem::path     checkpoints{ };

    recorder::Quality         quality{ recorder::Quality::RECORD };
  };

  class ScenarioRun
  {
  public:
    explicit ScenarioRun(RunParts parts);

    ScenarioRun(ScenarioRun const&)                    = delete;
    auto operator = (ScenarioRun const&) -> ScenarioRun& = delete;

    [[nodiscard]] auto Live() const noexcept -> session::Session&
    { return *_parts.session; }

    [[nodiscard]] auto Watches() const noexcept -> WatchValues const*
    { return _parts.watches; }

    [[nodiscard]] auto LineOf() const noexcept -> session::LineSource*
    { return _parts.line; }

    [[nodiscard]] auto Probes() const noexcept -> session::ProbeCounts
    { return _probes; }

    [[nodiscard]] auto Budget() noexcept -> FrameBudget& { return _budget; }

    // A number of its own for each search, so nesting keeps its seeds.
    [[nodiscard]] auto NextSearch() noexcept -> std::uint64_t
    { return ++_searches; }

    // Refuses when a call that may run that many frames has no budget left.
    [[nodiscard]] auto Allowed(std::uint64_t frames) const -> Outcome
    { return _budget.Room(_parts.session->Frames(), frames); }

    [[nodiscard]] auto Latest() const -> Result<bus::FrameView>;

    // Where the run's artifacts go, when the cli was given somewhere.
    [[nodiscard]] auto BundleRoot() const
      -> std::optional<std::filesystem::path>;

    // The change since the call before this one, which is what a scenario
    // means by "has anything happened"; the first call answers zero.
    [[nodiscard]] auto ChangeSinceObserved()
      -> Result<perception::ChangeAmount>;

    [[nodiscard]] auto Look(std::optional<std::filesystem::path> const& to,
                            std::optional<perception::Region> const& crop
                              = std::nullopt) -> Result<std::string>;

    [[nodiscard]] auto Colours(perception::Region const& crop,
                               perception::Colour const& wanted,
                               std::uint32_t within) const
      -> Result<std::uint64_t>;

    // Ports are numbered as a caller says them, 1 and 2.
    [[nodiscard]] auto Hold(std::size_t port,
                            std::vector<std::string> const& buttons)
      -> Outcome;

    // An empty list lets go of everything held on that port.
    [[nodiscard]] auto Release(std::size_t port,
                               std::vector<std::string> const& buttons)
      -> Outcome;

    [[nodiscard]] auto Held(std::size_t port) const
      -> Result<std::vector<std::string>>;

    [[nodiscard]] auto Observe() -> Result<Observation>;

    // The anchor predicate the mcp tools take, over this run's frames.
    [[nodiscard]] auto RunUntil(std::string const& predicate,
                                std::uint64_t timeout_frames)
      -> Result<std::uint64_t>;

    [[nodiscard]] auto Judged(std::string const& predicate)
      -> Result<tape::Judged>;

    // A block of a libretro region, where a watch names one number in it.
    [[nodiscard]] auto Memory(std::string const& region,
                              std::uint32_t address, std::size_t count) const
      -> Result<std::vector<std::byte>>;

    // A null-terminated string in that region, in the core's byte order.
    [[nodiscard]] auto String(std::string const& region,
                              std::uint32_t address, std::size_t count) const
      -> Result<std::string>;

    // An unsigned number of that width, in the region's own byte order.
    [[nodiscard]] auto Number(std::string const& region,
                              std::uint32_t address,
                              std::uint32_t width) const
      -> Result<std::int64_t>;

    [[nodiscard]] auto MemoryStable(std::string const& region,
                                    std::uint32_t address, std::size_t count,
                                    std::uint64_t frames,
                                    std::uint64_t timeout_frames)
      -> Result<std::uint64_t>;

    // Named: probed and cached; either way it carries its frame and place.
    [[nodiscard]] auto Checkpoint(std::string name,
                                  CheckpointKind kind = CheckpointKind::NAMED)
      -> Result<CheckpointKept>;

    // The copy this run holds wins; a newer file on disk is named, not read.
    [[nodiscard]] auto Restore(std::string const& name) -> Result<Restored>;

    // Where a named checkpoint is cached; a run without a store refuses.
    [[nodiscard]] auto CheckpointFile(std::string_view name) const
      -> Result<std::filesystem::path>;

    [[nodiscard]] auto Reset() -> Outcome { return _parts.session->Reset(); }

    // Drops a checkpoint; a name that was never taken is not a refusal.
    auto Forget(std::string const& name) -> void;

    [[nodiscard]] auto Play(std::filesystem::path const& file)
      -> Result<tape::PlayCounts>;

    // The bundle's report.html as it stands, for a scenario that wants to
    // look at the run before it ends.
    [[nodiscard]] auto Report() -> Result<std::filesystem::path>;

    // The bundle's tape.yaml as the line stands, filmed without closing.
    [[nodiscard]] auto Tape() -> Result<TapeWritten>;

    // The frames the line holds, which a restore folds back; nothing adrift.
    [[nodiscard]] auto LineFrames() const
      -> Result<std::optional<std::uint64_t>>;

    [[nodiscard]] auto Mark(std::string text, std::string group) -> Outcome;

    [[nodiscard]] auto MarkedAt(std::string const& name) const
      -> std::optional<std::uint64_t>;

    [[nodiscard]] auto ClipStart(std::optional<std::string> const& from_mark,
                                 std::optional<std::int64_t> from_frame) const
      -> Result<std::uint64_t>;

    // Design 11 cuts a clip out of the recorded video, so what a run records
    // is the window: a trigger record naming it, `<from> <to>` in frames.
    [[nodiscard]] auto Clip(std::string label, std::uint64_t from) -> Outcome;
    [[nodiscard]] auto Judge(std::string name, bool passed, std::string text)
      -> Outcome;

  private:
    struct Checkpointed
    {
      std::vector<std::byte> state{ };
      std::vector<std::byte> pixels{ };
      bus::FrameDescriptor   at{ };
      std::uint64_t          frames{ 0 };
      std::optional<session::LinePlace> place{ };  // none without a line
      std::optional<session::Line>      line{ };   // what a fold cannot give

      // When this run wrote the state, so a restore knows a later writer.
      std::optional<std::filesystem::file_time_type> cached{ };
    };

    // One verdict a run, or one for every checkpoint whose probe parted.
    [[nodiscard]] auto Probe(std::string const& name,
                             Checkpointed const& kept) -> Outcome;

    [[nodiscard]] auto Cache(std::string const& name,
                             Checkpointed const& kept) -> Outcome;

    [[nodiscard]] auto Overwritten(std::string const& name,
                                   Checkpointed const& kept) const
      -> std::optional<std::filesystem::path>;

    [[nodiscard]] auto NextShot() -> Result<std::filesystem::path>;

    // The bundle's shots, or a directory of the run's own, made on demand.
    [[nodiscard]] auto ShotsRoot() -> Result<std::filesystem::path>;

    // The core keeps its pointers all session, so a span outlives a wait.
    [[nodiscard]] auto Block(std::string const& region, std::uint32_t address,
                             std::size_t count) const
      -> Result<std::span<std::byte const>>;

    RunParts _parts;
    std::optional<CheckpointStore> _store{ };
    std::optional<oxbox::platform::ScratchArea> _scratch{ };
    std::map<std::string, Checkpointed> _checkpoints;
    std::map<std::string, std::uint64_t> _marks;
    std::vector<std::byte> _observed;
    bus::FrameDescriptor   _observed_at{};
    std::uint64_t          _shots{ 0 };
    std::uint64_t          _searches{ 0 };
    session::ProbeCounts   _probes{ };
    FrameBudget            _budget{ };
  };
}

namespace tash::python
{
  using detail::scenario_run::CheckpointKept;
  using detail::scenario_run::CheckpointKind;
  using detail::scenario_run::Observation;
  using detail::scenario_run::Restored;
  using detail::scenario_run::RunParts;
  using detail::scenario_run::ScenarioRun;
}
