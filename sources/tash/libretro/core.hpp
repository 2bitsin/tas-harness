#pragma once

#include "tash/libretro/core-sink.hpp"
#include "tash/libretro/core-symbols.hpp"
#include "tash/libretro/libretro.h"
#include "tash/libretro/shared-library.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tash::libretro::detail::core
{
  using utilities::Outcome;
  using utilities::Result;

  struct CoreInformation
  {
    std::string name;
    std::string version;
    std::string extensions;
    bool needs_full_path{ false };
    bool block_extract{ false };
  };

  struct CoreDirectories
  {
    std::string system;
    std::string save;
  };

  struct Refusal
  {
    unsigned command{ 0 };
    std::uint64_t count{ 0 };
  };

  class Core
  {
  public:
    static auto Load(std::filesystem::path const& library,
                     CoreDirectories directories)
      -> Result<std::unique_ptr<Core>>;

    Core(SharedLibrary library, CoreSymbols symbols,
         CoreDirectories directories);
    ~Core();

    Core(Core const&) = delete;
    auto operator=(Core const&) -> Core& = delete;
    Core(Core&&) = delete;
    auto operator=(Core&&) -> Core& = delete;

    auto Attach(CoreSink* sink) noexcept -> void { _sink = sink; }

    auto LoadGame(std::filesystem::path const& rom) -> Outcome;
    auto UnloadGame() -> void;

    auto Run() -> void { _symbols.retro_run(); }
    auto Reset() -> void { _symbols.retro_reset(); }
    auto UseControllerDevice(unsigned port, unsigned device) -> void
    { _symbols.retro_set_controller_port_device(port, device); }

    [[nodiscard]] auto Information() const noexcept -> CoreInformation const&
    { return _information; }
    [[nodiscard]] auto ApiVersion() const -> unsigned
    { return _symbols.retro_api_version(); }
    [[nodiscard]] auto AvInfo() const -> retro_system_av_info;
    [[nodiscard]] auto PixelFormat() const noexcept -> retro_pixel_format
    { return _pixel_format; }
    [[nodiscard]] auto Path() const noexcept -> std::filesystem::path const&
    { return _library.Path(); }

    [[nodiscard]] auto StateSize() const -> std::size_t
    { return _symbols.retro_serialize_size(); }
    [[nodiscard]] auto SaveState() const -> Result<std::vector<std::byte>>;
    auto LoadState(std::span<std::byte const> state) -> Outcome;

    [[nodiscard]] auto Memory(unsigned id) const -> std::span<std::byte>;

    // Every environment command answered false, by command id.
    [[nodiscard]] auto Refusals() const -> std::vector<Refusal>;

  private:
    static auto OnEnvironment(unsigned command, void* data) -> bool;
    static auto OnVideo(void const* pixels, unsigned width, unsigned height,
                        std::size_t pitch) -> void;
    static auto OnAudioSample(std::int16_t left, std::int16_t right) -> void;
    static auto OnAudioBatch(std::int16_t const* interleaved,
                             std::size_t frames) -> std::size_t;
    static auto OnInputPoll() -> void;
    static auto OnInputState(unsigned port, unsigned device, unsigned index,
                             unsigned id) -> std::int16_t;
    static auto OnLog(retro_log_level level, char const* format, ...) -> void;

    auto Environment(unsigned command, void* data) -> bool;

    // A libretro callback carries no user pointer, so the trampolines above
    // reach the instance through this slot; hence one live Core per process.
    static Core* _live;

    SharedLibrary _library;
    CoreSymbols _symbols;
    CoreDirectories _directories;
    CoreInformation _information;
    CoreSink* _sink{ nullptr };
    retro_pixel_format _pixel_format{ RETRO_PIXEL_FORMAT_0RGB1555 };
    std::map<std::string, std::string> _variables;
    std::map<unsigned, std::uint64_t> _refusals;
    std::vector<std::byte> _rom;
    bool _game_loaded{ false };
  };
}

namespace tash::libretro
{
  using detail::core::Core;
  using detail::core::CoreDirectories;
  using detail::core::CoreInformation;
  using detail::core::Refusal;
}
