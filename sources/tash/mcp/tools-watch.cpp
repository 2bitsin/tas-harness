#include "tash/mcp/base64.hpp"
#include "tash/mcp/observation-wording.hpp"
#include "tash/mcp/tool-arguments.hpp"
#include "tash/mcp/tool-table.hpp"

#include "tash/perception/region.hpp"
#include "tash/recorder/frame-png.hpp"
#include "tash/tape/predicate.hpp"
#include "tash/watches/watch-spec.hpp"
#include "tash/watches/watch.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::mcp::detail::tool_table
{
  using content::Failed;
  using content::Said;
  using content::Shown;
  using content::ToolOutcome;
  using observation_wording::Wording;
  using tool::Entry;
  using tool_context::ToolContext;
  using utilities::Outcome;
  using utilities::Result;

  namespace
  {
    using namespace tool_arguments;

    auto Cropped(std::optional<std::string> const& region)
      -> Result<std::optional<perception::Region>>
    {
      if (!region || region->empty())
        return std::optional<perception::Region>{ };
      Result<perception::Region> const cut{
        perception::RegionFrom(*region) };
      if (!cut)
        return utilities::Forwarded(cut);
      return std::optional{ *cut };
    }

    auto Observe(ToolContext& tools, NoArguments const&)
      -> Result<ToolOutcome>
    {
      Result<python::Observation> const seen{ tools.Observed() };
      return seen ? Said(Wording(*seen)) : Failed(seen.error());
    }

    auto Look(ToolContext& tools, LookArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<std::optional<perception::Region>> const crop{
        Cropped(asked.region) };
      if (!crop)
        return Failed(crop.error());
      Result<bus::FrameKept> const frame{ tools.Seen() };
      if (!frame)
        return Failed(frame.error());

      Result<std::vector<std::byte>> const png{
        *crop ? recorder::PngBytes(frame->View(), **crop)
              : recorder::PngBytes(frame->View()) };
      if (!png)
        return Failed(png.error());
      return Shown(Base64Of(*png),
                   std::format("frame {}",
                               frame->descriptor.number + 1));
    }

    auto Shot(ToolContext& tools, ShotArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      Result<std::optional<perception::Region>> const crop{
        Cropped(asked.region) };
      if (!crop)
        return Failed(crop.error());
      Result<std::string> const written{
        (*live)->Look(std::nullopt, *crop) };
      if (!written)
        return Failed(written.error());
      return Said(std::filesystem::absolute(*written).string());
    }

    inline constexpr std::size_t DUMP_COLUMNS{ 16 };

    auto HexDump(std::uint32_t address, std::span<std::byte const> block)
      -> std::string
    {
      std::string dump;
      for (std::size_t at{ 0 }; at < block.size(); at += DUMP_COLUMNS)
      {
        dump += std::format("{}{:08x} ", at == 0 ? "" : "\n", address + at);
        for (std::size_t step{ 0 };
             step < DUMP_COLUMNS && at + step < block.size(); ++step)
          dump += std::format(" {:02x}",
                              std::to_integer<unsigned>(block[at + step]));
      }
      return dump;
    }

    auto Peek(ToolContext& tools, PeekArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      Result<std::uint32_t> const at{ watches::AddressOf(asked.address) };
      if (!at)
        return Failed(at.error());
      if (asked.count <= 0)
        return Failed(std::format("mcp: peek reads at least one byte, not {}",
                                  asked.count));
      Result<std::vector<std::byte>> const block{ (*live)->Memory(
        asked.region.value_or(std::string{ watches::SYSTEM_RAM }), *at,
        static_cast<std::size_t>(asked.count)) };
      if (!block)
        return Failed(block.error());
      return Said(HexDump(*at, *block));
    }

    // One evaluation, two callers: `anchor` says it, `expect` judges it.
    auto Evaluated(ToolContext& tools, std::string const& predicate)
      -> Result<tape::Judged>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return utilities::Forwarded(live);
      return (*live)->Judged(predicate);
    }

    auto Held(ToolContext& tools, AnchorArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<tape::Judged> const seen{ Evaluated(tools, asked.predicate) };
      return seen ? Said(seen->wording) : Failed(seen.error());
    }

    auto Expect(ToolContext& tools, ExpectArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<tape::Judged> const seen{ Evaluated(tools, asked.predicate) };
      if (!seen)
        return Failed(seen.error());
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      if (Outcome const judged{ (*live)->Judge(
            asked.name, seen->passed, asked.text.value_or(seen->wording)) };
          !judged)
        return Failed(judged.error());
      std::string said{ std::format("{}: {}", asked.name, seen->wording) };
      return seen->passed ? Said(std::move(said)) : Failed(std::move(said));
    }

    auto Judge(ToolContext& tools, JudgeArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      if (Outcome const judged{ (*live)->Judge(
            asked.name, asked.passed, asked.text.value_or("")) }; !judged)
        return Failed(judged.error());
      return Said(std::format("{} {} at frame {}", asked.name,
                              asked.passed ? "passed" : "failed",
                              (*live)->Live().Frames()));
    }

    auto Mark(ToolContext& tools, MarkArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());
      std::uint64_t const frame{ (*live)->Live().Frames() };
      std::string const group{ asked.group.value_or("") };
      if (Outcome const marked{ (*live)->Mark(asked.name, group) }; !marked)
        return Failed(marked.error());
      if (group.empty())
        return Said(std::format("mark {} at frame {}", asked.name, frame));
      return Said(std::format("mark {} of {} at frame {}", asked.name, group,
                              frame));
    }

    auto Clip(ToolContext& tools, ClipArgs const& asked)
      -> Result<ToolOutcome>
    {
      Result<python::ScenarioRun*> const live{ tools.Live() };
      if (!live)
        return Failed(live.error());

      Result<std::uint64_t> const from{
        (*live)->ClipStart(asked.from_mark, asked.from_frame) };
      if (!from)
        return Failed(from.error());

      std::uint64_t const to{ (*live)->Live().Frames() };
      if (Outcome const cut{ (*live)->Clip(asked.label, *from) }; !cut)
        return Failed(cut.error());
      return Said(std::format("clip {} over frames {} to {}", asked.label,
                              *from, to));
    }
  }

  auto WatchTools() -> std::vector<tool::Tool>
  {
    return {
      Entry<NoArguments, &Observe>(
        "observe", "Read the latest frame as text: hashes, change, pads and "
                   "watches. Answers while a python job runs."),
      Entry<LookArgs, &Look>(
        "look", "Answer the latest frame as a PNG. Answers while a python "
                "job runs."),
      Entry<ShotArgs, &Shot>(
        "shot", "Write the latest frame into the bundle's shots."),
      Entry<PeekArgs, &Peek>(
        "peek", "Read a block of the guest's memory as a hex dump."),
      Entry<AnchorArgs, &Held>(
        "anchor", "Say whether an anchor predicate holds right now."),
      Entry<ExpectArgs, &Expect>(
        "expect", "Judge an anchor predicate and record the verdict."),
      Entry<JudgeArgs, &Judge>(
        "judge", "Record the caller's own answer as a verdict."),
      Entry<MarkArgs, &Mark>("mark", "Mark this frame in the trace."),
      Entry<ClipArgs, &Clip>(
        "clip", "Record a window the report cuts a clip from.")
    };
  }
}
