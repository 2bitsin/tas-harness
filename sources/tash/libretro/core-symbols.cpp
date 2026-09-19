#include "tash/libretro/core-symbols.hpp"

#include <string>
#include <type_traits>

namespace tash::libretro::detail::core_symbols
{
  using utilities::Result;

  auto CoreSymbols::Of(SharedLibrary const& library) -> Result<CoreSymbols>
  {
    CoreSymbols symbols;
    std::string failure;
    auto const bind = [&](char const* name, auto& slot)
    {
      if (!failure.empty())
        return;
      Result<void*> const found{ library.Symbol(name) };
      if (!found)
      {
        failure = found.error();
        return;
      }
      slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(*found);
    };

    bind("retro_set_environment", symbols.retro_set_environment);
    bind("retro_set_video_refresh", symbols.retro_set_video_refresh);
    bind("retro_set_audio_sample", symbols.retro_set_audio_sample);
    bind("retro_set_audio_sample_batch", symbols.retro_set_audio_sample_batch);
    bind("retro_set_input_poll", symbols.retro_set_input_poll);
    bind("retro_set_input_state", symbols.retro_set_input_state);
    bind("retro_init", symbols.retro_init);
    bind("retro_deinit", symbols.retro_deinit);
    bind("retro_api_version", symbols.retro_api_version);
    bind("retro_get_system_info", symbols.retro_get_system_info);
    bind("retro_get_system_av_info", symbols.retro_get_system_av_info);
    bind("retro_set_controller_port_device",
         symbols.retro_set_controller_port_device);
    bind("retro_reset", symbols.retro_reset);
    bind("retro_run", symbols.retro_run);
    bind("retro_serialize_size", symbols.retro_serialize_size);
    bind("retro_serialize", symbols.retro_serialize);
    bind("retro_unserialize", symbols.retro_unserialize);
    bind("retro_load_game", symbols.retro_load_game);
    bind("retro_unload_game", symbols.retro_unload_game);
    bind("retro_get_memory_data", symbols.retro_get_memory_data);
    bind("retro_get_memory_size", symbols.retro_get_memory_size);

    if (!failure.empty())
      return std::unexpected{ failure };
    return symbols;
  }
}
