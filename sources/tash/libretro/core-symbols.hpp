#pragma once

#include "tash/libretro/libretro.h"
#include "tash/libretro/shared-library.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>

namespace tash::libretro::detail::core_symbols
{
  using utilities::Result;

  // The C entry points every libretro core exports, under their own names.
  struct CoreSymbols
  {
    void (*retro_set_environment)(retro_environment_t){ nullptr };
    void (*retro_set_video_refresh)(retro_video_refresh_t){ nullptr };
    void (*retro_set_audio_sample)(retro_audio_sample_t){ nullptr };
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t){ nullptr };
    void (*retro_set_input_poll)(retro_input_poll_t){ nullptr };
    void (*retro_set_input_state)(retro_input_state_t){ nullptr };
    void (*retro_init)(){ nullptr };
    void (*retro_deinit)(){ nullptr };
    unsigned (*retro_api_version)(){ nullptr };
    void (*retro_get_system_info)(retro_system_info*){ nullptr };
    void (*retro_get_system_av_info)(retro_system_av_info*){ nullptr };
    void (*retro_set_controller_port_device)(unsigned, unsigned){ nullptr };
    void (*retro_reset)(){ nullptr };
    void (*retro_run)(){ nullptr };
    std::size_t (*retro_serialize_size)(){ nullptr };
    bool (*retro_serialize)(void*, std::size_t){ nullptr };
    bool (*retro_unserialize)(void const*, std::size_t){ nullptr };
    bool (*retro_load_game)(retro_game_info const*){ nullptr };
    void (*retro_unload_game)(){ nullptr };
    void* (*retro_get_memory_data)(unsigned){ nullptr };
    std::size_t (*retro_get_memory_size)(unsigned){ nullptr };

    static auto Of(SharedLibrary const& library) -> Result<CoreSymbols>;
  };
}

namespace tash::libretro
{
  using detail::core_symbols::CoreSymbols;
}
