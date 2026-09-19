#pragma once
// A tape written from the pad changes as they happen: the agent or a
// scenario acts, the recorder turns the bits that moved into transitions.

#include "tash/tape/anchor.hpp"
#include "tash/tape/channel.hpp"
#include "tash/tape/names.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tape/transitions.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::recorder
{
  class Recorder
  {
  public:
    explicit Recorder(tape::TapeHeader header);

    // The pad after the change: the bits that moved become a transition each.
    auto Change(std::uint64_t frame, std::size_t port, std::uint32_t pad)
      -> void;

    // Cuts the open segment and starts one at `frame` that waits for
    // `waited`. A mark on the frame an empty segment started names that one.
    auto Mark(std::uint64_t frame, std::string name, anchor::Anchor waited)
      -> void;

    [[nodiscard]] auto Finish(std::uint64_t frame) -> tape::Tape;

    [[nodiscard]] auto Transitions() const noexcept -> std::uint64_t
    { return _transitions; }

  private:
    struct Cut
    {
      std::string                          name;
      anchor::Anchor                       waited{ };
      std::uint64_t                        base{ 0 };
      std::vector<transitions::Transition> moves{ };
    };

    auto Close(std::uint64_t frame) -> void;

    tape::TapeHeader                         _header;
    Cut                                      _open;
    std::vector<tape::Segment>               _done{ };
    std::array<std::uint32_t, session::PORTS> _pads{ };
    std::uint64_t                            _transitions{ 0 };
  };
}

namespace tash::tape
{
  using detail::recorder::Recorder;
}
