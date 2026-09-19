#include "tash/libretro/core.hpp"

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace tash::libretro::detail::core
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;

  namespace
  {
    auto DefaultOption(std::string_view description) -> std::string
    {
      // A variable reads "Label; first|second"; libretro's default is first.
      std::size_t const semicolon{ description.find(';') };
      std::string_view options{ semicolon == std::string_view::npos
                                  ? description
                                  : description.substr(semicolon + 1) };
      while (!options.empty() && options.front() == ' ')
        options.remove_prefix(1);
      std::size_t const bar{ options.find('|') };
      return std::string{ bar == std::string_view::npos
                            ? options
                            : options.substr(0, bar) };
    }

    // A core's log line, cut to what one message may say.
    constexpr std::size_t LOG_LINE_BYTES{ 1024 };

    auto Text(char const* raw) -> std::string
    {
      return raw == nullptr ? std::string{} : std::string{ raw };
    }
  }

  Core* Core::_live{ nullptr };

  auto Core::Load(std::filesystem::path const& library,
                  CoreDirectories directories) -> Result<std::unique_ptr<Core>>
  {
    Result<SharedLibrary> opened{ SharedLibrary::Open(library) };
    if (!opened)
      return std::unexpected{ opened.error() };
    Result<CoreSymbols> symbols{ CoreSymbols::Of(*opened) };
    if (!symbols)
      return std::unexpected{ symbols.error() };
    return std::make_unique<Core>(std::move(*opened), *symbols,
                                  std::move(directories));
  }

  Core::Core(SharedLibrary library, CoreSymbols symbols,
             CoreDirectories directories)
    : _library{ std::move(library) }, _symbols{ symbols },
      _directories{ std::move(directories) }
  {
    if (_live != nullptr)
      throw std::logic_error{
        "a second libretro core cannot live in this process" };
    _live = this;

    retro_system_info info{};
    _symbols.retro_get_system_info(&info);
    _information = CoreInformation{ Text(info.library_name),
                                    Text(info.library_version),
                                    Text(info.valid_extensions),
                                    info.need_fullpath, info.block_extract };

    _symbols.retro_set_environment(&Core::OnEnvironment);
    _symbols.retro_set_video_refresh(&Core::OnVideo);
    _symbols.retro_set_audio_sample(&Core::OnAudioSample);
    _symbols.retro_set_audio_sample_batch(&Core::OnAudioBatch);
    _symbols.retro_set_input_poll(&Core::OnInputPoll);
    _symbols.retro_set_input_state(&Core::OnInputState);
    _symbols.retro_init();
  }

  Core::~Core()
  {
    UnloadGame();
    _symbols.retro_deinit();
    _live = nullptr;
  }

  auto Core::LoadGame(std::filesystem::path const& rom) -> Outcome
  {
    std::string const path{ rom.string() };
    retro_game_info game{};
    game.path = path.c_str();

    if (!_information.needs_full_path)
    {
      std::ifstream file{ rom, std::ios::binary | std::ios::ate };
      if (!file)
        return Refused("cannot read {}", path);
      auto const size{ file.tellg() };
      _rom.resize(static_cast<std::size_t>(size));
      file.seekg(0);
      if (!file.read(reinterpret_cast<char*>(_rom.data()), size))
        return Refused("cannot read {} bytes of {}", _rom.size(), path);
      game.data = _rom.data();
      game.size = _rom.size();
    }

    if (!_symbols.retro_load_game(&game))
      return Refused("{} refused the content {}",
                     _information.name, path);
    _game_loaded = true;
    return {};
  }

  auto Core::UnloadGame() -> void
  {
    if (!std::exchange(_game_loaded, false))
      return;
    _symbols.retro_unload_game();
    _rom.clear();
  }

  auto Core::AvInfo() const -> retro_system_av_info
  {
    retro_system_av_info info{};
    _symbols.retro_get_system_av_info(&info);
    return info;
  }

  auto Core::SaveState() const -> Result<std::vector<std::byte>>
  {
    std::vector<std::byte> state(_symbols.retro_serialize_size());
    if (state.empty())
      return Refused("{} serializes no state", _information.name);
    if (!_symbols.retro_serialize(state.data(), state.size()))
      return Refused("{} would not serialize {} bytes", _information.name,
                     state.size());
    return state;
  }

  auto Core::LoadState(std::span<std::byte const> state) -> Outcome
  {
    if (!_symbols.retro_unserialize(state.data(), state.size()))
      return Refused("{} would not restore {} bytes", _information.name,
                     state.size());
    return {};
  }

  auto Core::Memory(unsigned id) const -> std::span<std::byte>
  {
    auto* const data{
      static_cast<std::byte*>(_symbols.retro_get_memory_data(id)) };
    std::size_t const size{ _symbols.retro_get_memory_size(id) };
    if (data == nullptr || size == 0)
      return {};
    return std::span{ data, size };
  }

  auto Core::Refusals() const -> std::vector<Refusal>
  {
    std::vector<Refusal> refusals;
    refusals.reserve(_refusals.size());
    for (auto const& [command, count] : _refusals)
      refusals.push_back(Refusal{ command, count });
    return refusals;
  }

  auto Core::Environment(unsigned command, void* data) -> bool
  {
    if (data == nullptr)
    {
      ++_refusals[command];
      return false;
    }
    switch (command)
    {
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
        auto const wanted{ *static_cast<retro_pixel_format const*>(data) };
        if (wanted != RETRO_PIXEL_FORMAT_RGB565)
          break;
        _pixel_format = wanted;
        return true;
      }
      case RETRO_ENVIRONMENT_SET_VARIABLES:
      {
        _variables.clear();
        for (auto const* entry{ static_cast<retro_variable const*>(data) };
             entry != nullptr && entry->key != nullptr; ++entry)
          _variables.emplace(entry->key, DefaultOption(Text(entry->value)));
        return true;
      }
      case RETRO_ENVIRONMENT_GET_VARIABLE:
      {
        auto* const asked{ static_cast<retro_variable*>(data) };
        if (asked->key == nullptr)
          break;
        auto const found{ _variables.find(asked->key) };
        if (found == _variables.end())
          break;
        asked->value = found->second.c_str();
        return true;
      }
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        // Nothing changes a variable mid-run, so the core reads each once.
        *static_cast<bool*>(data) = false;
        return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<char const**>(data) = _directories.system.c_str();
        return true;
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<char const**>(data) = _directories.save.c_str();
        return true;
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = &Core::OnLog;
        return true;
      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        return true;
      default:
        break;
    }
    ++_refusals[command];
    return false;
  }

  auto Core::OnEnvironment(unsigned command, void* data) -> bool
  {
    return _live != nullptr && _live->Environment(command, data);
  }

  auto Core::OnVideo(void const* pixels, unsigned width, unsigned height,
                     std::size_t pitch) -> void
  {
    if (_live == nullptr || _live->_sink == nullptr || pixels == nullptr)
      return;
    _live->_sink->PushVideo(
      std::span{ static_cast<std::byte const*>(pixels), height * pitch },
      width, height, pitch);
  }

  auto Core::OnAudioSample(std::int16_t left, std::int16_t right) -> void
  {
    std::int16_t const frame[]{ left, right };
    if (_live != nullptr && _live->_sink != nullptr)
      _live->_sink->PushAudio(std::span{ frame });
  }

  auto Core::OnAudioBatch(std::int16_t const* interleaved,
                          std::size_t frames) -> std::size_t
  {
    if (_live != nullptr && _live->_sink != nullptr && interleaved != nullptr)
      _live->_sink->PushAudio(std::span{ interleaved, frames * 2 });
    return frames;
  }

  auto Core::OnInputPoll() -> void
  {
    if (_live != nullptr && _live->_sink != nullptr)
      _live->_sink->PollInput();
  }

  auto Core::OnInputState(unsigned port, unsigned device, unsigned index,
                          unsigned id) -> std::int16_t
  {
    if (_live == nullptr || _live->_sink == nullptr)
      return 0;
    return _live->_sink->InputState(port, device, index, id);
  }

  auto Core::OnLog(retro_log_level level, char const* format, ...) -> void
  {
    if (level < RETRO_LOG_WARN)
      return;
    char line[LOG_LINE_BYTES];
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    std::fputs("core: ", stderr);
    std::fputs(line, stderr);
  }
}
