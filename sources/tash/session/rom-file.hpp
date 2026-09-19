#pragma once

#include "tash/utilities/outcome.hpp"

#include <filesystem>

namespace tash::session::detail::rom_file
{
  using utilities::Result;

  // A zero-byte file is the published placeholder, not a cartridge.
  [[nodiscard]] auto CartridgePresent(std::filesystem::path const& rom) -> bool;

  // A core is handed a plain file: a zip is extracted into `scratch` first,
  // keeping the member's own name so the core still sees its extension.
  [[nodiscard]] auto PreparedRom(std::filesystem::path const& rom,
                                 std::filesystem::path const& scratch)
    -> Result<std::filesystem::path>;
}

namespace tash::session
{
  using detail::rom_file::CartridgePresent;
  using detail::rom_file::PreparedRom;
}
