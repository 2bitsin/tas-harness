#include "_tape-writer.hpp"

#include <cctype>
#include <format>
#include <string_view>
#include <utility>

namespace tash::linked::detail::tape_writer
{
  namespace
  {
    static_assert(harness::MICE == tape::MICE,
                  "the tape's pointers and libtash's are the same two");

    constexpr int FRAME_COLUMN{ 6 };
    constexpr std::string_view BLOCK_INDENT{ "      " };

    // What goes into a yaml scalar unquoted, which is what a name a target
    // chose has to be cut down to.
    [[nodiscard]] auto Plain(std::string_view text) -> std::string
    {
      std::string kept;
      for (char letter : text)
        kept += (std::isalnum(static_cast<unsigned char>(letter)) != 0
                 || letter == '.' || letter == '_' || letter == '-'
                 || letter == '/') ? letter : '-';
      return kept;
    }

    [[nodiscard]] auto Indented(std::string_view block) -> std::string
    {
      std::string text;
      std::size_t at{ 0 };
      while (at < block.size())
      {
        auto const end{ block.find('\n', at) };
        text += BLOCK_INDENT;
        text += block.substr(at, end == std::string_view::npos
                                   ? std::string_view::npos : end + 1 - at);
        if (end == std::string_view::npos)
          break;
        at = end + 1;
      }
      return text;
    }
  }

  TapeWriter::TapeWriter(std::string name, std::string profile)
    : _name{ std::move(name) }, _profile{ std::move(profile) }
  { }

  auto TapeWriter::Buttons(std::uint64_t frame, tape::Device device,
                           std::size_t port, std::uint32_t was,
                           std::uint32_t now) -> void
  {
    std::uint32_t const moved{ was ^ now };
    if (moved == 0u)
      return;

    auto const names{ tape::ButtonsOf(device) };
    for (std::size_t button{ 0 }; button < names.size(); ++button)
    {
      std::uint32_t const bit{ std::uint32_t{ 1 } << button };
      if ((moved & bit) == 0u)
        continue;
      _lines += std::format("{:<{}} {:<4} {}{}{}{}\n", frame, FRAME_COLUMN,
                            (now & bit) != 0u ? tape::DOWN_WORD
                                              : tape::UP_WORD,
                            tape::PrefixOf(device), port + 1,
                            tape::CHANNEL_SEPARATOR, names[button]);
      ++_transitions;
    }
  }

  auto TapeWriter::Input(std::uint64_t frame, harness::Input const& input)
    -> void
  {
    for (std::size_t port{ 0 }; port < harness::PADS; ++port)
      Buttons(frame, tape::Device::PAD, port, _held.pads[port].buttons,
              input.pads[port].buttons);

    for (std::size_t port{ 0 }; port < harness::MICE; ++port)
    {
      Buttons(frame, tape::Device::MOUSE, port, _held.mice[port].buttons,
              input.mice[port].buttons);
      if (input.mice[port].x != _held.mice[port].x
          || input.mice[port].y != _held.mice[port].y)
      {
        _points += std::format("{:<{}} {}{:<3} {}{}{}\n", frame, FRAME_COLUMN,
                               tape::MOUSE_PREFIX, port + 1,
                               input.mice[port].x, tape::POINT_SEPARATOR,
                               input.mice[port].y);
        ++_moves;
      }
    }

    for (std::uint64_t word : input.keys)
      _keys = _keys || word != 0u;

    _held = input;
  }

  auto TapeWriter::Text(std::uint64_t frames) const -> std::string
  {
    std::string text{ std::format(
      "# What libtash recorded: one segment, from the target's first frame.\n"
      "header:\n  name: {}\n", Plain(_name)) };
    if (!_profile.empty())
      text += std::format("  profile: {}\n", Plain(_profile));
    text += std::format("  time_base: frame\nsegments:\n  - name: {}\n"
                        "    frames: {}\n", tape::FIRST_SEGMENT_NAME, frames);
    if (!_lines.empty())
      text += std::format("    transitions: |\n{}", Indented(_lines));
    if (!_points.empty())
      text += std::format("    pointer: |\n{}", Indented(_points));
    return text;
  }
}
