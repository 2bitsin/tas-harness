#include "tash/tash/core-command.hpp"

#include "tash/libretro/core.hpp"
#include "tash/tash/core-path.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <oxbox/platform/scratch-area.hpp>

#include <cmath>
#include <print>

namespace tash::cli::detail::core_command
{
  using utilities::Result;

  auto CoreCommand::info(std::string core) -> oxbox::cli::CliResult
  {
    Result<std::filesystem::path> const library{ ResolvedCore(core) };
    if (!library)
      return oxbox::cli::CliResult::Failed(1, library.error());

    Result<oxbox::platform::ScratchArea> const scratch{
      utilities::ScratchAreaOf("core-info") };
    if (!scratch)
      return oxbox::cli::CliResult::Failed(1, scratch.error());

    std::string const directory{ scratch->Path().string() };
    Result<std::unique_ptr<libretro::Core>> const loaded{
      libretro::Core::Load(
        *library, libretro::CoreDirectories{ directory, directory }) };
    if (!loaded)
      return oxbox::cli::CliResult::Failed(1, loaded.error());

    libretro::Core const& core_loaded{ **loaded };
    libretro::CoreInformation const& about{ core_loaded.Information() };
    std::print("path        {}\n", core_loaded.Path().string());
    std::print("name        {}\n", about.name);
    std::print("version     {}\n", about.version);
    std::print("extensions  {}\n", about.extensions);
    std::print("full path   {}\n", about.needs_full_path ? "yes" : "no");
    std::print("api         {}\n", core_loaded.ApiVersion());

    retro_system_av_info const av{ core_loaded.AvInfo() };
    if (std::isfinite(av.timing.fps) && av.timing.fps > 0.0)
    {
      std::print("geometry    {}x{}, up to {}x{}, aspect {:.4f}\n",
                 av.geometry.base_width, av.geometry.base_height,
                 av.geometry.max_width, av.geometry.max_height,
                 av.geometry.aspect_ratio);
      std::print("timing      {:.4f} fps, {:.0f} Hz\n", av.timing.fps,
                 av.timing.sample_rate);
    }
    else
      std::print("av info     the core answers none until a ROM is loaded\n");

    std::vector<libretro::Refusal> const refused{ core_loaded.Refusals() };
    std::print("refused     {} environment {}\n", refused.size(),
               refused.size() == 1 ? "command" : "commands");
    for (libretro::Refusal const& one : refused)
      std::print("  command {:<4} asked {} time{}\n", one.command, one.count,
                 one.count == 1 ? "" : "s");
    return {};
  }
}
