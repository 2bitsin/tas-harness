#include "tash/python/scenario-run.hpp"

#include "tash/perception/colour-count.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/report/render.hpp"
#include "tash/recorder/frame-png.hpp"
#include "tash/tape/channel.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/scratch-area.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/region-order.hpp"

#include <algorithm>
#include <format>
#include <span>
#include <system_error>
#include <utility>

namespace tash::python::detail::scenario_run
{
  using utilities::Forwarded;
  using utilities::Refused;

  namespace
  {
    auto ViewOf(std::vector<std::byte> const& pixels,
                bus::FrameDescriptor const& descriptor)
      -> perception::Rgb565View
    {
      return perception::Rgb565View{
        std::span<std::uint8_t const>{
          reinterpret_cast<std::uint8_t const*>(pixels.data()),
          pixels.size() },
        descriptor.width, descriptor.height,
        static_cast<std::uint32_t>(descriptor.pitch) };
    }

    auto PortAt(std::size_t port) -> Result<std::size_t>
    {
      if (port < 1 || port > session::PORTS)
        return Refused("python: port {} is not one of 1..{}", port,
                       session::PORTS);
      return port - 1;
    }

    auto MaskOf(std::vector<std::string> const& buttons, std::size_t at)
      -> Result<std::uint32_t>
    {
      std::uint32_t mask{ 0 };
      for (std::string const& button : buttons)
      {
        Result<tape::Channel> const channel{ tape::ChannelFrom(
          button.find(tape::CHANNEL_SEPARATOR) == std::string::npos
            ? std::format("{}{}{}{}", tape::PORT_PREFIX, at + 1,
                          tape::CHANNEL_SEPARATOR, button)
            : button) };
        if (!channel)
          return utilities::Forwarded(channel);
        mask |= channel->Mask();
      }
      return mask;
    }

    auto ButtonsHeld(std::uint32_t pad) -> std::vector<std::string>
    {
      std::vector<std::string> held;
      for (unsigned button{ 0 }; button < tape::BUTTON_NAMES.size(); ++button)
        if ((pad & (std::uint32_t{ 1 } << button)) != 0u)
          held.emplace_back(tape::BUTTON_NAMES[button]);
      return held;
    }

    constexpr std::string_view RESTORE_VERDICT{
      "a restored state continues as the straight line" };

    auto AgreedText(session::RestoreDifference const& seen) -> std::string
    {
      return std::format("{} frames compared, hash for hash", seen.compared);
    }

    auto PartedText(std::string const& name,
                    session::RestoreDifference const& seen) -> std::string
    {
      return std::format("checkpoint {:?}: of {} frames compared, the lines "
                         "part at frame {}", name, seen.compared,
                         *seen.parted_at);
    }

    auto Within(std::span<std::byte const> area, std::string const& region,
                std::uint32_t address, std::size_t count) -> Outcome
    {
      if (address > area.size() || count > area.size() - address)
        return Refused("python: {} holds {} bytes, so {} of them from 0x{:x} "
                       "runs past its end", region, area.size(), count,
                       address);
      return { };
    }

    auto SameShape(bus::FrameDescriptor const& one,
                   bus::FrameDescriptor const& other) -> bool
    {
      return one.width == other.width && one.height == other.height
             && one.pitch == other.pitch;
    }
  }

  ScenarioRun::ScenarioRun(RunParts parts) : _parts{ std::move(parts) }
  {
    if (!_parts.checkpoints.empty())
      _store.emplace(_parts.checkpoints);
  }

  auto ScenarioRun::Latest() const -> Result<bus::FrameView>
  {
    std::optional<bus::FrameView> const frame{
      _parts.session->Video().Latest() };
    if (!frame)
      return Refused("python: the run has produced no frame yet");
    return *frame;
  }

  auto ScenarioRun::BundleRoot() const
    -> std::optional<std::filesystem::path>
  {
    if (_parts.bundle == nullptr)
      return std::nullopt;
    return _parts.bundle->Root();
  }

  auto ScenarioRun::ChangeSinceObserved() -> Result<perception::ChangeAmount>
  {
    Result<bus::FrameView> const frame{ Latest() };
    if (!frame)
      return std::unexpected{ frame.error() };

    perception::ChangeAmount answer{};
    if (!_observed.empty() && SameShape(_observed_at, frame->descriptor))
    {
      Result<perception::ChangeAmount> const change{
        perception::ChangeBetween(ViewOf(_observed, _observed_at),
                                  perception::ViewOf(*frame)) };
      if (!change)
        return std::unexpected{ change.error() };
      answer = *change;
    }
    _observed.assign(frame->pixels.begin(), frame->pixels.end());
    _observed_at = frame->descriptor;
    return answer;
  }

  auto ScenarioRun::ShotsRoot() -> Result<std::filesystem::path>
  {
    if (_parts.bundle != nullptr)
      return _parts.bundle->Shots();
    if (!_scratch)
    {
      Result<oxbox::platform::ScratchArea> area{
        utilities::ScratchAreaOf(SHOTS_PURPOSE) };
      if (!area)
        return Forwarded(area);
      _scratch = std::move(*area);
    }
    return _scratch->Path();
  }

  auto ScenarioRun::NextShot() -> Result<std::filesystem::path>
  {
    Result<std::filesystem::path> const root{ ShotsRoot() };
    if (!root)
      return Forwarded(root);
    std::string const name{ std::format("{}-{:0{}}{}", SHOT_PREFIX, ++_shots,
                                        SHOT_DIGITS, SHOT_SUFFIX) };
    return *root / name;
  }

  auto ScenarioRun::Look(std::optional<std::filesystem::path> const& to,
                         std::optional<perception::Region> const& crop)
    -> Result<std::string>
  {
    Result<bus::FrameView> const frame{ Latest() };
    if (!frame)
      return std::unexpected{ frame.error() };

    Result<std::filesystem::path> const path{
      to ? Result<std::filesystem::path>{ *to } : NextShot() };
    if (!path)
      return std::unexpected{ path.error() };
    if (Outcome const written{ crop
          ? recorder::WritePng(*frame, *crop, *path)
          : recorder::WritePng(*frame, *path) }; !written)
      return std::unexpected{ written.error() };
    return path->string();
  }

  auto ScenarioRun::Colours(perception::Region const& crop,
                            perception::Colour const& wanted,
                            std::uint32_t within) const
    -> Result<std::uint64_t>
  {
    Result<bus::FrameView> const frame{ Latest() };
    if (!frame)
      return Forwarded(frame);
    return perception::ColourCount(*frame, crop, wanted, within);
  }

  auto ScenarioRun::RunUntil(std::string const& predicate,
                             std::uint64_t timeout_frames)
    -> Result<std::uint64_t>
  {
    Result<tape::AnchorCheck> const check{ tape::CheckFrom(predicate) };
    if (!check)
      return Forwarded(check);
    return tape::StepsUntil(*check, Live(), Watches(), timeout_frames);
  }

  auto ScenarioRun::Judged(std::string const& predicate)
    -> Result<tape::Judged>
  {
    Result<tape::AnchorCheck> const check{ tape::CheckFrom(predicate) };
    if (!check)
      return Forwarded(check);
    Result<bus::FrameView> const frame{ Latest() };
    if (!frame)
      return Forwarded(frame);
    return tape::JudgedOn(*check, *frame, Watches(), Live().Frames());
  }

  auto ScenarioRun::Block(std::string const& region, std::uint32_t address,
                          std::size_t count) const
    -> Result<std::span<std::byte const>>
  {
    watches::MemoryMap const map{ watches::MemoryMap::Of(*_parts.session) };
    Result<std::span<std::byte const>> const area{ map.Area(region) };
    if (!area)
      return Forwarded(area);
    if (Outcome const inside{ Within(*area, region, address, count) };
        !inside)
      return Forwarded(inside);
    return area->subspan(address, count);
  }

  auto ScenarioRun::String(std::string const& region, std::uint32_t address,
                           std::size_t count) const -> Result<std::string>
  {
    watches::MemoryMap const map{ watches::MemoryMap::Of(*_parts.session) };
    Result<std::span<std::byte const>> const area{ map.Area(region) };
    if (!area)
      return Forwarded(area);
    if (Outcome const inside{ Within(*area, region, address, count) };
        !inside)
      return Forwarded(inside);
    Result<watches::Endianness> const order{ map.Order(region) };
    if (!order)
      return Forwarded(order);
    return watches::TextIn(*area, address, count, *order);
  }

  auto ScenarioRun::Number(std::string const& region, std::uint32_t address,
                           std::uint32_t width) const -> Result<std::int64_t>
  {
    watches::MemoryMap const map{ watches::MemoryMap::Of(*_parts.session) };
    Result<std::span<std::byte const>> const area{ map.Area(region) };
    if (!area)
      return Forwarded(area);
    Result<watches::Endianness> const order{ map.Order(region) };
    if (!order)
      return Forwarded(order);
    return watches::ReadNumber(*area, address,
                               watches::NumberFormat{ width, *order, false });
  }

  auto ScenarioRun::Memory(std::string const& region,
                           std::uint32_t address, std::size_t count) const
    -> Result<std::vector<std::byte>>
  {
    Result<std::span<std::byte const>> const block{
      Block(region, address, count) };
    if (!block)
      return Forwarded(block);
    return std::vector<std::byte>{ block->begin(), block->end() };
  }

  auto ScenarioRun::MemoryStable(std::string const& region,
                                 std::uint32_t address, std::size_t count,
                                 std::uint64_t frames,
                                 std::uint64_t timeout_frames)
    -> Result<std::uint64_t>
  {
    Result<std::span<std::byte const>> const block{
      Block(region, address, count) };
    if (!block)
      return Forwarded(block);

    std::vector<std::byte> held{ block->begin(), block->end() };
    std::uint64_t stepped{ 0 };
    std::uint64_t still{ 0 };
    while (still < frames)
    {
      if (stepped >= timeout_frames)
        return Refused("python: {} 0x{:x}+{} never held still for {} frames,"
                       " in the {} this stepped", region, address, count,
                       frames, stepped);
      _parts.session->Step(1);
      ++stepped;
      still = std::ranges::equal(held, *block) ? still + 1 : 0;
      held.assign(block->begin(), block->end());
    }
    return stepped;
  }

  auto ScenarioRun::Checkpoint(std::string name, CheckpointKind kind)
    -> Result<CheckpointKept>
  {
    Result<std::vector<std::byte>> state{ _parts.session->SaveState() };
    if (!state)
      return std::unexpected{ state.error() };

    Checkpointed kept;
    kept.state = std::move(*state);
    kept.frames = _parts.session->Frames();
    if (_parts.line != nullptr)
    {
      kept.place = _parts.line->Place();
      kept.line = _parts.line->LineUpTo(kept.frames);
    }
    if (Result<bus::FrameView> const frame{ Latest() }; frame)
    {
      kept.pixels.assign(frame->pixels.begin(), frame->pixels.end());
      kept.at = frame->descriptor;
    }
    auto const held{ _checkpoints.insert_or_assign(std::move(name),
                                                   std::move(kept)) };
    std::string const& named{ held.first->first };
    Checkpointed const& moment{ held.first->second };

    CheckpointKept answer;
    answer.frames = moment.frames;
    answer.bytes = moment.state.size();
    if (kind == CheckpointKind::SCRATCH)
    {
      ++_probes.unprobed;
      return answer;
    }
    if (_store)
    {
      Result<std::filesystem::path> const file{ _store->FileOf(named) };
      if (!file)
        return Forwarded(file);
      if (Outcome const cached{ Cache(named, moment) }; !cached)
        return Forwarded(cached);
      std::error_code failure;
      held.first->second.cached = std::filesystem::last_write_time(*file,
                                                                  failure);
      if (failure)
        held.first->second.cached = { };
      answer.file = *file;
    }

    // Last: the probe steps and restores, so the state written above is the
    // moment itself.
    if (Outcome const probed{ Probe(named, moment) }; !probed)
      return Forwarded(probed);
    return answer;
  }

  auto ScenarioRun::Cache(std::string const& name, Checkpointed const& kept)
    -> Outcome
  {
    CheckpointView writing;
    writing.state = kept.state;
    writing.frame = bus::FrameView{ kept.at, kept.pixels };
    writing.frames = kept.frames;
    writing.seconds = _parts.session->HarnessSeconds();
    writing.line = kept.line;
    return _store->Write(name, writing);
  }

  auto ScenarioRun::CheckpointFile(std::string_view name) const
    -> Result<std::filesystem::path>
  {
    if (!_store)
      return Refused("python: this run caches no checkpoint on disk");
    return _store->FileOf(name);
  }

  auto ScenarioRun::Probe(std::string const& name, Checkpointed const& kept)
    -> Outcome
  {
    session::RestoreProbe probe{
      *_parts.session,
      session::Checkpoint{ name, kept.state, kept.pixels, kept.at,
                           kept.frames, kept.place } };
    Result<session::RestoreDifference> const seen{ probe.Run() };
    if (!seen)
      return Forwarded(seen);
    ++_probes.probed;
    if (seen->parted_at)
    {
      ++_probes.parted;
      return Judge(std::string{ RESTORE_VERDICT }, false,
                   PartedText(name, *seen));
    }

    // The first probe speaks for every probe that agrees after it, so a run
    // whose restores are all exact carries one verdict, not one a checkpoint.
    if (_probes.probed > 1u)
      return { };
    return Judge(std::string{ RESTORE_VERDICT }, true, AgreedText(*seen));
  }

  auto ScenarioRun::Restore(std::string const& name) -> Result<Restored>
  {
    if (auto const held{ _checkpoints.find(name) };
        held != _checkpoints.end())
    {
      session::Checkpoint back{ name,
                                held->second.state,
                                held->second.pixels,
                                held->second.at,
                                held->second.frames,
                                held->second.place };
      if (held->second.line)
        back.line = &*held->second.line;
      if (Outcome const loaded{ _parts.session->Restore(back) }; !loaded)
        return Forwarded(loaded);
      return Restored{ { }, Overwritten(name, held->second) };
    }
    if (!_store)
      return Refused("python: no checkpoint is named {}", name);

    Result<StoredCheckpoint> const kept{ _store->Read(name) };
    if (!kept)
      return Forwarded(kept);
    session::Checkpoint back{ };
    back.name = name;
    back.state = kept->state;
    back.pixels = kept->pixels;
    back.at = kept->at;
    back.frames = kept->frames;
    if (kept->line)
      back.line = &*kept->line;
    if (Outcome const loaded{ _parts.session->Restore(back) }; !loaded)
      return Forwarded(loaded);
    Result<std::filesystem::path> const file{ _store->FileOf(name) };
    if (!file)
      return Forwarded(file);
    return Restored{ *file, { } };
  }

  auto ScenarioRun::Overwritten(std::string const& name,
                                Checkpointed const& kept) const
    -> std::optional<std::filesystem::path>
  {
    if (!_store || !kept.cached)
      return { };
    Result<std::filesystem::path> const file{ _store->FileOf(name) };
    if (!file)
      return { };
    std::error_code failure;
    auto const written{ std::filesystem::last_write_time(*file, failure) };
    if (failure || written <= *kept.cached)
      return { };
    return *file;
  }

  auto ScenarioRun::Forget(std::string const& name) -> void
  {
    _checkpoints.erase(name);
  }

  auto ScenarioRun::Play(std::filesystem::path const& file)
    -> Result<tape::PlayCounts>
  {
    session::Session& live{ *_parts.session };
    if (live.Frames() > live.PoweredOnAt())
      return Refused("python: a tape plays from power on and this run stands "
                     "at frame {}; reset() powers the machine on again, and "
                     "restore(name) is how a run goes back to a state it has "
                     "already been in", live.Frames());

    Result<tape::Player> player{ tape::Player::Of(file) };
    if (!player)
      return std::unexpected{ player.error() };

    tape::PlayOptions options;
    options.watches = _parts.watches;
    options.report = _parts.report;
    return player->Play(*_parts.session, options);
  }

  auto ScenarioRun::Report() -> Result<std::filesystem::path>
  {
    if (_parts.bundle == nullptr)
      return Refused("python: this run has no bundle to report on; "
                     "`tash run --bundle <root>` gives it one");
    if (Outcome const flushed{ _parts.trace->Flush() }; !flushed)
      return Forwarded(flushed);
    return report::RenderReport(_parts.bundle->Root());
  }

  auto ScenarioRun::LineFrames() const -> Result<std::optional<std::uint64_t>>
  {
    if (_parts.line == nullptr)
      return Refused("python: this run keeps no tape; "
                     "`tash run --bundle <root>` gives it one");
    std::optional<session::Line> const line{
      _parts.line->LineUpTo(_parts.session->Frames()) };
    if (!line)
      return std::optional<std::uint64_t>{ };
    return std::optional<std::uint64_t>{ line->frames };
  }

  auto ScenarioRun::Tape() -> Result<TapeWritten>
  {
    if (_parts.tape == nullptr)
      return Refused("python: this run keeps no tape; "
                     "`tash run --bundle <root>` gives it one");
    return _parts.tape->WriteTape();
  }

  auto ScenarioRun::Mark(std::string text, std::string group) -> Outcome
  {
    if (_parts.marks != nullptr)
      _parts.marks->OnMark(text);
    _marks.insert_or_assign(text, _parts.session->Frames());
    trace::MarkRecord const record{ _parts.session->Frames(),
                                    std::move(text), std::move(group) };
    if (_parts.trace == nullptr)
      return {};
    return _parts.trace->Write(record);
  }

  auto ScenarioRun::MarkedAt(std::string const& name) const
    -> std::optional<std::uint64_t>
  {
    auto const found{ _marks.find(name) };
    return found == _marks.end() ? std::nullopt
                                 : std::optional{ found->second };
  }

  auto ScenarioRun::ClipStart(std::optional<std::string> const& from_mark,
                              std::optional<std::int64_t> from_frame) const
    -> Result<std::uint64_t>
  {
    if (from_mark)
    {
      std::optional<std::uint64_t> const at{ MarkedAt(*from_mark) };
      if (!at)
        return Refused("python: no mark named {} in this run", *from_mark);
      return *at;
    }
    if (from_frame && *from_frame >= 0)
      return static_cast<std::uint64_t>(*from_frame);
    return Refused("python: a clip starts at a mark or at a frame");
  }

  auto ScenarioRun::Clip(std::string label, std::uint64_t from) -> Outcome
  {
    if (_parts.quality == recorder::Quality::FILM)
      return Refused("python: this run records a film, whose video carries a "
                     "keyframe every {} seconds; a clip cannot be cut from "
                     "it", recorder::FILM_KEYFRAME_SECONDS);
    std::uint64_t const to{ _parts.session->Frames() };
    if (from > to)
      return Refused("python: a clip cannot start at frame {} and end at {}",
                     from, to);
    trace::TriggerRecord const record{
      to, std::move(label), std::format("{} {}", from, to) };
    if (_parts.trace == nullptr)
      return { };
    return _parts.trace->Write(record);
  }

  auto ScenarioRun::Judge(std::string name, bool passed, std::string text)
    -> Outcome
  {
    trace::VerdictRecord const record{ _parts.session->Frames(),
                                       std::move(name), passed,
                                       std::move(text) };
    if (_parts.trace != nullptr)
      if (Outcome const written{ _parts.trace->Write(record) }; !written)
        return written;
    if (_parts.verdicts != nullptr)
      return _parts.verdicts->Append(record);
    return {};
  }
}

namespace tash::python::detail::scenario_run
{
  auto ScenarioRun::Hold(std::size_t port,
                         std::vector<std::string> const& buttons) -> Outcome
  {
    Result<std::size_t> const at{ PortAt(port) };
    if (!at)
      return utilities::Forwarded(at);
    Result<std::uint32_t> const mask{ MaskOf(buttons, *at) };
    if (!mask)
      return utilities::Forwarded(mask);
    Live().HoldPad(*at, Live().Pad(*at) | *mask);
    return { };
  }

  auto ScenarioRun::Release(std::size_t port,
                            std::vector<std::string> const& buttons)
    -> Outcome
  {
    Result<std::size_t> const at{ PortAt(port) };
    if (!at)
      return utilities::Forwarded(at);
    if (buttons.empty())
    {
      Live().HoldPad(*at, 0u);
      return { };
    }
    Result<std::uint32_t> const mask{ MaskOf(buttons, *at) };
    if (!mask)
      return utilities::Forwarded(mask);
    Live().HoldPad(*at, Live().Pad(*at) & ~*mask);
    return { };
  }

  auto ScenarioRun::Held(std::size_t port) const
    -> Result<std::vector<std::string>>
  {
    Result<std::size_t> const at{ PortAt(port) };
    if (!at)
      return utilities::Forwarded(at);
    return ButtonsHeld(Live().Pad(*at));
  }

  auto ScenarioRun::Observe() -> Result<Observation>
  {
    Result<bus::FrameView> const frame{ Latest() };
    if (!frame)
      return utilities::Forwarded(frame);
    Result<perception::FrameHashes> const hashes{
      perception::HashesOf(*frame) };
    if (!hashes)
      return utilities::Forwarded(hashes);
    Result<perception::ChangeAmount> const change{ ChangeSinceObserved() };
    if (!change)
      return utilities::Forwarded(change);

    Observation seen;
    seen.frame = Live().Frames();
    seen.time = Live().HarnessSeconds();
    seen.width = frame->descriptor.width;
    seen.height = frame->descriptor.height;
    seen.exact = hashes->exact;
    seen.difference = hashes->difference;
    seen.perceptual = hashes->perceptual;
    seen.change = change->changed_ratio;
    for (std::size_t port{ 0 }; port < session::PORTS; ++port)
      seen.pads[port] = ButtonsHeld(Live().Pad(port));
    if (WatchValues const* const values{ Watches() }; values != nullptr)
      for (std::string const& name : values->Names())
        seen.watches.emplace_back(name, values->Value(name));
    return seen;
  }
}
