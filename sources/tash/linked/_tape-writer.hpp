#pragma once
// The tape a recording leaves: one segment holding every button the target
// moved and everywhere its pointer went, in the lines tape/README.md spells.

#include "tash/linked/tash.hpp"
#include "tash/tape/names.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace tash::linked::detail::tape_writer
{
  class TapeWriter
  {
  public:
    TapeWriter(std::string name, std::string profile);

    auto Input(std::uint64_t frame, harness::Input const& input) -> void;

    [[nodiscard]] auto Text(std::uint64_t frames) const -> std::string;

    [[nodiscard]] auto Transitions() const noexcept -> std::uint64_t
    { return _transitions; }

    [[nodiscard]] auto Moves() const noexcept -> std::uint64_t
    { return _moves; }

    // A key was held that no channel can spell: the tape has pads and
    // pointers and nothing else.
    [[nodiscard]] auto Keys() const noexcept -> bool
    { return _keys; }

  private:
    auto Buttons(std::uint64_t frame, tape::Device device, std::size_t port,
                 std::uint32_t was, std::uint32_t now) -> void;

    std::string    _name;
    std::string    _profile;
    harness::Input _held{ };
    std::string    _lines{ };
    std::string    _points{ };
    std::uint64_t  _transitions{ 0 };
    std::uint64_t  _moves{ 0 };
    bool           _keys{ false };
  };
}
