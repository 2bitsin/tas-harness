#include "tash/tape/recorder.hpp"

#include <utility>

namespace tash::tape::detail::recorder
{
  Recorder::Recorder(tape::TapeHeader header)
    : _header{ std::move(header) },
      _open{ std::string{ FIRST_SEGMENT_NAME }, anchor::Anchor{ }, 0, { } }
  { }

  auto Recorder::Change(std::uint64_t frame, std::size_t port,
                        std::uint32_t pad) -> void
  {
    if (port >= _pads.size())
      return;

    std::uint32_t const moved{ _pads[port] ^ pad };
    _pads[port] = pad;
    if (moved == 0u)
      return;

    std::uint64_t const at{ frame > _open.base ? frame - _open.base : 0 };
    for (unsigned button{ 0 }; button < BUTTON_NAMES.size(); ++button)
    {
      std::uint32_t const bit{ std::uint32_t{ 1 } << button };
      if ((moved & bit) == 0u)
        continue;
      _open.moves.push_back(transitions::Transition{
        at, (pad & bit) != 0u, channel::Channel{ port, button } });
      ++_transitions;
    }
  }

  auto Recorder::Mark(std::uint64_t frame, std::string name,
                      anchor::Anchor waited) -> void
  {
    if (_open.moves.empty() && frame == _open.base)
    {
      _open.name   = std::move(name);
      _open.waited = std::move(waited);
      return;
    }
    Close(frame);
    _open = Cut{ std::move(name), std::move(waited), frame, { } };
  }

  auto Recorder::Finish(std::uint64_t frame) -> tape::Tape
  {
    Close(frame);
    tape::Tape written{ _header, std::move(_done) };
    _done = { };
    _open = Cut{ std::string{ FIRST_SEGMENT_NAME }, anchor::Anchor{ }, frame,
                 { } };
    return written;
  }

  auto Recorder::Close(std::uint64_t frame) -> void
  {
    tape::Segment segment{ };
    segment.name        = _open.name;
    segment.anchor      = _open.waited;
    segment.frames      = frame > _open.base ? frame - _open.base : 0;
    segment.transitions = transitions::TextOf(_open.moves);
    _done.push_back(std::move(segment));
  }
}
