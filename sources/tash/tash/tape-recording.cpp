#include "tash/tash/tape-recording.hpp"

#include "tash/tape/anchor.hpp"
#include "tash/tape/channel.hpp"
#include "tash/tape/recorder.hpp"
#include "tash/tape/transitions.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace tash::cli::detail::tape_recording
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    auto AnchorOn(std::uint64_t hash) -> tape::Anchor
    {
      tape::Anchor waited{ };
      waited.kind = tape::AnchorKind::EXACT_HASH;
      waited.hash = tape::HashText(hash);
      return waited;
    }

    auto Unique(std::set<std::string>& taken, std::string name) -> std::string
    {
      if (taken.insert(name).second)
        return name;
      for (std::uint64_t next{ 2 }; ; ++next)
      {
        std::string tried{ std::format("{}-{}", name, next) };
        if (taken.insert(tried).second)
          return tried;
      }
    }

    [[nodiscard]] auto HashesIn(std::filesystem::path const& path,
                                std::set<std::uint64_t> const& wanted)
      -> Result<std::map<std::uint64_t, std::uint64_t>>
    {
      std::map<std::uint64_t, std::uint64_t> hashes{ };
      if (wanted.empty())
        return hashes;

      Result<trace::Reader> reader{ trace::Reader::Open(path) };
      if (!reader)
        return Forwarded(reader);
      reader->ForEach([&wanted, &hashes](auto const& record) {
        using Held = std::decay_t<decltype(record)>;
        if constexpr (std::is_same_v<Held, trace::FrameRecord>)
          if (wanted.contains(record.frame))
            hashes.emplace(record.frame, record.hash_exact);
      });
      return hashes;
    }
  }

  auto TapeRecording::OnFrame(bus::FrameView const& frame, std::int64_t)
    -> void
  {
    std::uint64_t const harness{ frame.descriptor.number };
    if (_stretches.empty()
        || harness != _stretches.back().harness + _stretches.back().frames)
      _stretches.push_back(Stretch{ harness, _line, 0 });
    ++_stretches.back().frames;

    for (std::size_t port{ 0 }; port < _pads.size(); ++port)
    {
      std::uint32_t const held{ _live->Pad(port) };
      if (held == _pads[port])
        continue;
      _pads[port] = held;
      _changes.push_back(Change{ _line, port, held });
    }
    ++_line;
  }

  auto TapeRecording::OnRewind(session::Rewind const& back) -> void
  {
    if (back.place && Holds(*back.place))
    {
      Folded(back.place->frames);
      return;
    }
    if (back.line)
    {
      Seeded(*back.line);
      return;
    }
    if (_adrift.empty())
      _adrift_from = _line;
    _adrift = back.place
      ? std::format("'{}' was taken on a line this run left", back.name)
      : std::format("'{}' came from outside this run", back.name);
  }

  auto TapeRecording::OnReset(session::Reset const&) -> void
  {
    // From power on the run is one line again, whatever it had lost track of.
    _adrift.clear();
    _adrift_from.reset();
    Folded(0);
  }

  auto TapeRecording::OnMark(std::string_view text) -> void
  {
    if (!text.empty())
      _marks.push_back(Cut{ _line, std::string{ text } });
  }

  auto TapeRecording::Folded(std::uint64_t frames) -> void
  {
    frames = std::min(frames, _line);
    _seeded = std::min(_seeded, frames);
    std::erase_if(_changes, [frames](Change const& change) {
      return change.frame >= frames; });
    std::erase_if(_marks, [frames](Cut const& cut) {
      return cut.frame >= frames; });

    // A seeded run's first stretch starts where the seed's line ended, not
    // at zero, so the cut is found on each stretch's own place on the line.
    std::size_t held{ 0 };
    for (; held < _stretches.size(); ++held)
    {
      Stretch& stretch{ _stretches[held] };
      if (stretch.line + stretch.frames < frames)
        continue;
      stretch.frames = frames > stretch.line ? frames - stretch.line : 0;
      break;
    }
    if (held < _stretches.size())
      _stretches.resize(_stretches[held].frames > 0 ? held + 1 : held);
    _line = frames;

    // Cutting back past where the run lost the line puts it back on one.
    if (_adrift_from && frames <= *_adrift_from)
    {
      _adrift.clear();
      _adrift_from.reset();
    }

    // A restore puts the core back but not the pads, so the line's pads are
    // what the changes it kept last said.
    _pads = { };
    for (Change const& change : _changes)
      if (change.port < _pads.size())
        _pads[change.port] = change.pad;
  }

  auto TapeRecording::Seeded(session::Line const& before) -> void
  {
    _changes.clear();
    _marks.clear();
    _stretches.clear();
    _pads = { };
    _line = before.frames;
    _seeded = before.frames;
    ++_seeds;
    _adrift.clear();
    _adrift_from.reset();

    Result<std::vector<tape::Transition>> const moves{
      tape::TransitionsFrom(before.transitions) };
    if (!moves)
    {
      _adrift = moves.error();
      return;
    }
    for (tape::Transition const& move : *moves)
    {
      std::size_t const port{ move.channel.port };
      if (port >= _pads.size())
        continue;
      std::uint32_t const held{ move.down ? _pads[port] | move.channel.Mask()
                                          : _pads[port]
                                              & ~move.channel.Mask() };
      if (held == _pads[port])
        continue;
      _pads[port] = held;
      _changes.push_back(Change{ move.frame, port, held });
    }
    for (session::LineMark const& mark : before.marks)
      _marks.push_back(Cut{ mark.frame, mark.name });
  }

  auto TapeRecording::Kept(std::uint64_t frames) const -> session::Line
  {
    tape::Recorder written{ tape::TapeHeader{ } };
    for (Change const& change : _changes)
      if (change.frame < frames)
        written.Change(change.frame, change.port, change.pad);
    tape::Tape const whole{ written.Finish(frames) };

    session::Line line{ };
    line.frames = frames;
    if (!whole.segments.empty() && whole.segments.front().transitions)
      line.transitions = *whole.segments.front().transitions;
    for (Cut const& cut : _marks)
      if (cut.frame < frames)
        line.marks.push_back(session::LineMark{ cut.frame, cut.name });
    return line;
  }

  auto TapeRecording::LineUpTo(std::uint64_t frames) const
    -> std::optional<session::Line>
  {
    if (!_adrift.empty())
      return { };
    return Kept(LineAt(frames).value_or(_line));
  }

  auto TapeRecording::Place() const -> session::LinePlace
  {
    if (_line == 0)
      return { };
    return session::LinePlace{ _line, TokenAt(_line - 1) };
  }

  auto TapeRecording::TokenAt(std::uint64_t line) const -> std::uint64_t
  {
    if (line < _seeded)
      return SEED_TOKEN + _seeds;
    std::optional<std::uint64_t> const harness{ HarnessAt(line) };
    return harness ? *harness + 1 : 0;
  }

  auto TapeRecording::Holds(session::LinePlace const& place) const -> bool
  {
    if (place.frames == 0)
      return true;
    return place.token != 0 && place.frames <= _line
           && TokenAt(place.frames - 1) == place.token;
  }

  auto TapeRecording::HarnessAt(std::uint64_t line) const
    -> std::optional<std::uint64_t>
  {
    for (Stretch const& stretch : _stretches)
      if (line >= stretch.line && line < stretch.line + stretch.frames)
        return stretch.harness + (line - stretch.line);
    return { };
  }

  auto TapeRecording::LineAt(std::uint64_t harness) const
    -> std::optional<std::uint64_t>
  {
    for (Stretch const& stretch : _stretches)
      if (harness >= stretch.harness
          && harness < stretch.harness + stretch.frames)
        return stretch.line + (harness - stretch.harness);
    return { };
  }

  auto TapeRecording::Write(recorder::Bundle const& bundle,
                            tape::TapeHeader header) const -> Outcome
  {
    if (!_adrift.empty())
      return Refused("tape: this run cannot be written as one line: {}",
                     _adrift);

    std::set<std::uint64_t> wanted{ };
    for (Cut const& cut : _marks)
      if (cut.frame > 0)
        if (std::optional<std::uint64_t> const at{ HarnessAt(cut.frame - 1) })
          wanted.insert(*at);

    Result<std::map<std::uint64_t, std::uint64_t>> const hashes{
      HashesIn(bundle.Trace(), wanted) };
    if (!hashes)
      return Forwarded(hashes);

    tape::Recorder recording{ std::move(header) };
    std::set<std::string> taken{ };

    // A mark past frame 0 leaves the opening segment standing under its own
    // name, which a replay's first mark carries back in.
    if (_marks.empty() || _marks.front().frame > 0)
      taken.insert(std::string{ tape::FIRST_SEGMENT_NAME });
    std::size_t next{ 0 };
    for (Cut const& cut : _marks)
    {
      for (; next < _changes.size() && _changes[next].frame < cut.frame;
           ++next)
        recording.Change(_changes[next].frame, _changes[next].port,
                         _changes[next].pad);

      tape::Anchor waited{ };
      if (cut.frame > 0)
        if (std::optional<std::uint64_t> const at{ HarnessAt(cut.frame - 1) })
          if (auto const found{ hashes->find(*at) }; found != hashes->end())
            waited = AnchorOn(found->second);
      recording.Mark(cut.frame, Unique(taken, cut.name), std::move(waited));
    }
    for (; next < _changes.size(); ++next)
      recording.Change(_changes[next].frame, _changes[next].port,
                       _changes[next].pad);
    return tape::WriteTape(recording.Finish(_line), bundle.Tape());
  }
}
