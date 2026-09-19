#include "tash/mcp/tool-arguments.hpp"
#include "tash/mcp/tool-table.hpp"

#include "tash/tape/player.hpp"
#include "tash/tape/predicate.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::mcp::detail::tool_table
{
  using content::Failed;
  using content::Said;
  using content::ToolOutcome;
  using tool::Entry;
  using tool_context::ToolContext;
  using utilities::Outcome;
  using utilities::Result;

  namespace
  {
    using namespace tool_arguments;

    inline constexpr std::size_t DEFAULT_PORT{ 1 };
    inline constexpr std::uint64_t DEFAULT_FRAMES{ 1 };
    inline constexpr std::uint64_t DEFAULT_TIMEOUT{ 600 };

    auto Counted(std::optional<std::int64_t> asked, std::uint64_t instead)
      -> std::uint64_t
    {
      return asked && *asked > 0 ? static_cast<std::uint64_t>(*asked)
                                 : instead;
    }

    auto Step(ToolContext& tools, StepArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      std::uint64_t const frames{ Counted(asked.frames, DEFAULT_FRAMES) };
      (*live)->Live().Step(frames);
      return tools.Moved(std::format("stepped {} frames, now at {}", frames,
                                     (*live)->Live().Frames()));
    }

    auto RunUntil(ToolContext& tools, RunUntilArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      Result<tape::AnchorCheck> const check{
        tape::CheckFrom(asked.predicate) };
      if (!check)
        return Failed(check.error());
      Result<std::uint64_t> const took{ tape::StepsUntil(
        *check, (*live)->Live(), (*live)->Watches(),
        Counted(asked.timeout_frames, DEFAULT_TIMEOUT)) };
      if (!took)
        return Failed(took.error());
      return tools.Moved(std::format("{} held after {} frames, now at {}",
                                     check->Wording(), *took,
                                     (*live)->Live().Frames()));
    }

    auto Pace(ToolContext& tools, PaceArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      (*live)->Live().Pace(asked.rate);
      return Said(asked.rate > 0.0
        ? std::format("paced at {:.3f}x real time", asked.rate)
        : std::string{ "paced as fast as the core runs" });
    }

    auto Act(ToolContext& tools, ActArgs const& asked) -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());

      std::size_t const port{
        asked.port ? static_cast<std::size_t>(*asked.port) : DEFAULT_PORT };
      std::uint64_t const frames{ Counted(asked.frames, DEFAULT_FRAMES) };

      if (asked.release)
        if (Outcome const let{ (*live)->Release(port, *asked.release) }; !let)
          return Failed(let.error());
      if (asked.hold)
        if (Outcome const down{ (*live)->Hold(port, *asked.hold) }; !down)
          return Failed(down.error());
      if (asked.tap)
        if (Outcome const down{ (*live)->Hold(port, *asked.tap) }; !down)
          return Failed(down.error());

      (*live)->Live().Step(frames);

      if (asked.tap)
        if (Outcome const up{ (*live)->Release(port, *asked.tap) }; !up)
          return Failed(up.error());

      Result<std::vector<std::string>> const held{ (*live)->Held(port) };
      if (!held)
        return Failed(held.error());
      std::string names;
      for (std::string const& button : *held)
        names += names.empty() ? button : std::format(" {}", button);
      return tools.Moved(
        std::format("ran {} frames, now at {}, p{} holds {}", frames,
                    (*live)->Live().Frames(), port,
                    names.empty() ? std::string{ "nothing" } : names));
    }

    auto Reset(ToolContext& tools, NoArguments const&)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      if (Outcome const powered{ (*live)->Reset() }; !powered)
        return Failed(powered.error());
      return tools.Moved(std::format(
        "reset to power on, the run goes on at frame {}",
        (*live)->Live().Frames()));
    }

    auto Checkpoint(ToolContext& tools, NamedArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      Result<python::CheckpointKept> const kept{
        (*live)->Checkpoint(asked.name) };
      if (!kept)
        return Failed(kept.error());
      return Said(std::format("checkpoint {} at frame {}, {} bytes, {}",
                              asked.name, kept->frames, kept->bytes,
                              kept->file
                                ? std::format("cached in {}",
                                              kept->file->string())
                                : std::string{ "in this run alone" }));
    }

    auto RestoredText(std::string const& name, python::Restored const& back)
      -> std::string
    {
      if (back.file)
        return std::format("restored {} from {}", name, back.file->string());
      if (back.passed_over)
        return std::format("restored {} from this run; {} is newer and was "
                           "not read, shutdown and launch again to take it",
                           name, back.passed_over->string());
      return std::format("restored {} from this run", name);
    }

    auto Restore(ToolContext& tools, NamedArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      Result<python::Restored> const back{ (*live)->Restore(asked.name) };
      if (!back)
        return Failed(back.error());
      return tools.Moved(RestoredText(asked.name, *back));
    }

    auto RestoreOrPlay(ToolContext& tools, RestoreOrPlayArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());

      if (Result<python::Restored> const back{
            (*live)->Restore(asked.name) }; back)
        return tools.Moved(RestoredText(asked.name, *back));

      Result<tape::PlayCounts> const played{ (*live)->Play(asked.tape) };
      if (!played)
        return Failed(played.error());
      Result<ToolOutcome> const kept{
        Checkpoint(tools, NamedArgs{ asked.name }) };
      if (!kept || kept->is_error)
        return kept;
      return tools.Moved(
        std::format("played {} ({} segments, {} frames) and checkpointed {}",
                    asked.tape, played->segments, played->frames,
                    asked.name));
    }
  }

  auto PlayTools() -> std::vector<tool::Tool>
  {
    return {
      Entry<StepArgs, &Step>("step", "Run that many frames."),
      Entry<RunUntilArgs, &RunUntil>(
        "run_until", "Run until an anchor predicate holds, or give up."),
      Entry<PaceArgs, &Pace>("pace", "Pace what follows against real time."),
      Entry<ActArgs, &Act>(
        "act", "Hold, release or tap buttons, then run frames."),
      Entry<NoArguments, &Reset>(
        "reset", "Power-cycle the core and run one frame, so what is "
                 "judged next is the new machine's; the frame count goes "
                 "on."),
      Entry<NamedArgs, &Checkpoint>(
        "checkpoint", "Save a named state, in this run and on disk."),
      Entry<NamedArgs, &Restore>(
        "restore", "Load a named state from this run or from disk."),
      Entry<RestoreOrPlayArgs, &RestoreOrPlay>(
        "restore_or_play", "Restore the checkpoint, or play the tape and "
                           "take it.")
    };
  }
}
