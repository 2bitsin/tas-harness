#pragma once
// The cartridge's own bytes, mapped read-only from the file the profile
// names: the one region libretro never hands over.

#include "tash/utilities/outcome.hpp"

#include <oxbox/platform/mapped-file.hpp>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

namespace tash::session::detail::cartridge
{
  using utilities::Result;

  inline constexpr std::string_view CARTRIDGE{ "cartridge" };

  class Cartridge
  {
  public:
    Cartridge() = default;

    [[nodiscard]] static auto Of(std::filesystem::path const& rom)
      -> Result<Cartridge>;

    // In the file's own order, which is the order the 68000 reads it in.
    [[nodiscard]] auto Bytes() const noexcept -> std::span<std::byte const>;

  private:
    explicit Cartridge(oxbox::platform::MappedFile mapped);

    oxbox::platform::MappedFile _mapped{};
  };
}

namespace tash::session
{
  using detail::cartridge::CARTRIDGE;
  using detail::cartridge::Cartridge;
}
