#include "tash/session/cartridge.hpp"

#include <exception>
#include <utility>

namespace tash::session::detail::cartridge
{
  using utilities::Refused;

  Cartridge::Cartridge(oxbox::platform::MappedFile mapped)
  : _mapped{ std::move(mapped) }
  {
  }

  auto Cartridge::Of(std::filesystem::path const& rom) -> Result<Cartridge>
  {
    try
    {
      return Cartridge{ oxbox::platform::MappedFile{
        rom, oxbox::platform::MapIntent::READ_ONLY } };
    }
    catch (std::exception const& failure)
    {
      return Refused("cannot read {} as a cartridge: {}", rom.string(),
                     failure.what());
    }
  }

  auto Cartridge::Bytes() const noexcept -> std::span<std::byte const>
  {
    return { _mapped.data(), _mapped.size() };
  }
}
