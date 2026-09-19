#include "tash/tash/profile.hpp"

#include "tash/libretro/libretro.h"
#include "tash/tash/core-path.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>
#include <string_view>
#include <vector>

namespace tash::cli::detail::profile
{
  using utilities::Forwarded;
  using utilities::Result;
  using utilities::Refused;

  constexpr std::string_view PROFILE_FILE{ "profile.yaml" };

  auto ProfileFileAt(std::filesystem::path const& path)
    -> Result<std::filesystem::path>
  {
    if (std::filesystem::is_directory(path))
    {
      std::filesystem::path const inside{ path / PROFILE_FILE };
      if (!std::filesystem::is_regular_file(inside))
        return Refused("no {} in {}", PROFILE_FILE, path.string());
      return inside;
    }
    if (!std::filesystem::is_regular_file(path))
      return Refused("no profile at {}", path.string());
    return path;
  }

  auto ProfileFrom(std::filesystem::path const& path) -> Result<RunProfile>
  {
    Result<std::filesystem::path> const file{ ProfileFileAt(path) };
    if (!file)
      return Forwarded(file);
    try
    {
      return oxbox::serialization::DeserializeFrom<RunProfile>(*file);
    }
    catch (std::exception const& failure)
    {
      return Refused("{} is not a run profile: {}", file->string(),
                     failure.what());
    }
  }

  auto DeviceOf(std::string_view named) -> Result<unsigned>
  {
    if (named == "joypad")
      return RETRO_DEVICE_JOYPAD;
    if (named == "none")
      return RETRO_DEVICE_NONE;
    return Refused("{} is not a device this step drives; say joypad or none",
                   named);
  }

  auto SettingsFrom(RunProfile const& profile)
    -> Result<session::SessionSettings>
  {
    Result<std::filesystem::path> const core{ ResolvedCore(profile.core) };
    if (!core)
      return Forwarded(core);

    session::SessionSettings settings;
    settings.core = *core;
    settings.rom = profile.rom;
    settings.system_directory = profile.system_dir;
    settings.save_directory = profile.save_dir;
    settings.devices = { RETRO_DEVICE_JOYPAD, RETRO_DEVICE_NONE };
    for (std::size_t port{ 0 }; port < profile.ports.size(); ++port)
    {
      if (port >= session::PORTS)
        return Refused("this step drives {} ports, and the profile names {}",
                       session::PORTS, profile.ports.size());
      Result<unsigned> const device{ DeviceOf(profile.ports[port]) };
      if (!device)
        return Forwarded(device);
      settings.devices[port] = *device;
    }
    return settings;
  }

  auto WatchesFrom(RunProfile const& profile) -> Result<watches::WatchSet>
  {
    if (!profile.watches)
      return watches::WatchSet{};
    return watches::WatchSet::From(*profile.watches);
  }
}
