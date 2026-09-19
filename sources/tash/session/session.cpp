#include "tash/session/session.hpp"

#include "tash/session/rom-file.hpp"
#include "tash/trace/record.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace tash::session::detail::session
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;

  namespace
  {
    // The frames of the line a restore goes back to: the place the run's own
    // recording gave the checkpoint, the line a foreign one carries with it.
    auto LineFramesOf(Checkpoint const& kept) -> std::uint64_t
    {
      if (kept.place)
        return kept.place->frames;
      return kept.line != nullptr ? kept.line->frames : 0;
    }

    // A checkpoint with a line and no place on this run's own seeds the line
    // with frames this run never ran, the last of which is the state it is
    // restoring: nothing else records what that line ends as.
    auto SeedOf(Checkpoint const& kept) -> std::optional<Seed>
    {
      if (kept.place || kept.line == nullptr || kept.line->frames == 0
          || kept.pixels.empty())
        return { };
      Seed brought{ };
      brought.at = kept.line->frames - 1;
      brought.harness_time = std::llround(kept.at.harness_seconds
                                          * NANOSECONDS_PER_SECOND);
      brought.frame = bus::FrameView{ kept.at, kept.pixels };
      return brought;
    }

    auto DirectoriesOf(SessionSettings const& settings)
      -> libretro::CoreDirectories
    {
      return libretro::CoreDirectories{ settings.system_directory.string(),
                                        settings.save_directory.string() };
    }

    auto Made(std::filesystem::path const& directory) -> Outcome
    {
      std::error_code failure;
      std::filesystem::create_directories(directory, failure);
      if (failure)
        return Refused("cannot make {}: {}", directory.string(),
                       failure.message());
      return {};
    }
  }

  auto Session::Open(SessionSettings settings)
    -> Result<std::unique_ptr<Session>>
  {
    Result<oxbox::platform::ScratchArea> scratch{
      utilities::ScratchAreaOf("session") };
    if (!scratch)
      return std::unexpected{ scratch.error() };

    if (settings.system_directory.empty())
      settings.system_directory = scratch->File("system");
    if (settings.save_directory.empty())
      settings.save_directory = scratch->File("save");
    for (auto const& directory : { settings.system_directory,
                                   settings.save_directory })
      if (Outcome const made{ Made(directory) }; !made)
        return std::unexpected{ made.error() };

    Result<std::unique_ptr<libretro::Core>> core{
      libretro::Core::Load(settings.core, DirectoriesOf(settings)) };
    if (!core)
      return std::unexpected{ core.error() };

    auto opened{ std::make_unique<Session>(std::move(*scratch),
                                           std::move(*core),
                                           std::move(settings)) };
    if (Outcome const begun{ opened->Begin() }; !begun)
      return std::unexpected{ begun.error() };
    return opened;
  }

  Session::Session(oxbox::platform::ScratchArea scratch,
                   std::unique_ptr<libretro::Core> core,
                   SessionSettings settings)
    : _scratch{ std::move(scratch) }, _core{ std::move(core) },
      _settings{ std::move(settings) },
      _clock{ _settings.pacing == clock::Pacing::Stepped
                ? clock::Clock::Stepped()
                : clock::Clock::Paced(_settings.rate) }
  {
  }

  Session::~Session() = default;

  auto Session::Begin() -> Outcome
  {
    Result<std::filesystem::path> const rom{
      PreparedRom(_settings.rom, _scratch.Path()) };
    if (!rom)
      return std::unexpected{ rom.error() };
    _rom = *rom;

    Result<Cartridge> cartridge{ Cartridge::Of(_rom) };
    if (!cartridge)
      return std::unexpected{ cartridge.error() };
    _cartridge = std::move(*cartridge);

    if (Outcome const on{ PowerOn() }; !on)
      return on;

    retro_system_av_info const info{ _core->AvInfo() };
    _fps = info.timing.fps;
    if (_fps <= 0.0)
      return Refused("{} reports {} frames per second",
                     _core->Information().name, _fps);
    _clock.Start(1.0 / _fps);
    return {};
  }

  auto Session::PowerOn() -> Outcome
  {
    _core->Attach(this);
    if (Outcome const loaded{ _core->LoadGame(_rom) }; !loaded)
      return loaded;

    for (std::size_t port{ 0 }; port < PORTS; ++port)
      _core->UseControllerDevice(static_cast<unsigned>(port),
                                 _settings.devices[port]);

    if (_core->PixelFormat() != RETRO_PIXEL_FORMAT_RGB565)
      return Refused("{} did not ask for RGB565, which is the only format "
                     "the bus carries", _core->Information().name);
    return {};
  }

  auto Session::Reopened() -> Outcome
  {
    // Measured on Genesis Plus GX 1.7.4: retro_reset leaves all 65,536 bytes
    // of work ram standing, and an unload/load pair leaves five of them
    // reading as the run before rather than as a fresh launch.
    _core.reset();
    Result<std::unique_ptr<libretro::Core>> core{
      libretro::Core::Load(_settings.core, DirectoriesOf(_settings)) };
    if (!core)
      return Forwarded(core);
    _core = std::move(*core);
    return PowerOn();
  }

  auto Session::Observe(FrameObserver& observer) -> void
  {
    _observers.push_back(&observer);
  }

  auto Session::Observe(AudioObserver& observer) -> void
  {
    _audio_observers.push_back(&observer);
  }

  auto Session::Step(std::uint64_t frames) -> void
  {
    for (std::uint64_t done{ 0 }; done < frames; ++done)
    {
      _clock.Await(_frames);
      _core->Run();
      ++_frames;
    }
  }

  auto Session::RunUntil(std::function<bool()> const& holds,
                         std::uint64_t timeout_frames)
    -> Result<std::uint64_t>
  {
    std::uint64_t const started{ _frames };
    if (holds())
      return 0;
    while (_frames - started < timeout_frames)
    {
      Step(1);
      if (holds())
        return _frames - started;
    }
    return Refused("the condition did not hold within {} frames",
                   timeout_frames);
  }

  auto Session::Pace(double rate) -> void
  {
    _clock.Pace(rate, _frames);
  }

  auto Session::HoldPad(std::size_t port, std::uint32_t buttons) -> void
  {
    if (port >= PORTS || _pads[port] == buttons)
      return;
    _pads[port] = buttons;
    if (_trace != nullptr)
      static_cast<void>(_trace->Write(trace::InputRecord{
        _frames, static_cast<std::uint8_t>(port), buttons }));
  }

  auto Session::Pad(std::size_t port) const -> std::uint32_t
  {
    return port < PORTS ? _pads[port] : 0;
  }

  auto Session::LoadState(std::span<std::byte const> state) -> Outcome
  {
    return _core->LoadState(state);
  }

  auto Session::Restore(Checkpoint const& kept) -> Outcome
  {
    if (Outcome const back{ LoadState(kept.state) }; !back)
      return back;

    // A state another run made stands further along than this one has come;
    // the counter never goes back, so it moves up to meet it.
    if (kept.frames)
      _frames = std::max(_frames, *kept.frames);

    // Written after the move, so the frames a seeded run goes on to make sit
    // beyond the record and the line reader keeps them in a stretch of their
    // own rather than in one fabricated from where the run used to stand.
    if (_trace != nullptr)
      static_cast<void>(_trace->Write(trace::RestoreRecord{
        _frames, LineFramesOf(kept), std::string{ kept.name } }));
    Republish(kept.pixels, kept.at.width, kept.at.height, kept.at.pitch);

    Rewind const back_to{ _frames, kept.place, kept.name, kept.line,
                          SeedOf(kept) };
    for (FrameObserver* observer : _observers)
      observer->OnRewind(back_to);
    return { };
  }

  auto Session::Reset() -> Outcome
  {
    for (std::size_t port{ 0 }; port < PORTS; ++port)
      HoldPad(port, 0);

    if (Outcome const again{ Reopened() }; !again)
      return again;

    if (_trace != nullptr)
      static_cast<void>(_trace->Write(trace::ResetRecord{ _frames }));
    for (FrameObserver* observer : _observers)
      observer->OnReset(frame_observer::Reset{ _frames });

    // A core draws nothing until it runs, and the frame on the bus is the
    // one the run had before it powered on: a predicate judged between the
    // two would answer for a machine that no longer exists.
    Step(RESET_FRAMES);
    _powered_on_at = _frames;
    return { };
  }

  auto Session::Memory() const -> std::vector<MemoryRegion>
  {
    std::vector<MemoryRegion> regions;
    for (auto const& [name, id] :
         { std::pair<std::string_view, unsigned>{ "system",
                                                  RETRO_MEMORY_SYSTEM_RAM },
           std::pair<std::string_view, unsigned>{ "save",
                                                  RETRO_MEMORY_SAVE_RAM },
           std::pair<std::string_view, unsigned>{ "video",
                                                  RETRO_MEMORY_VIDEO_RAM },
           std::pair<std::string_view, unsigned>{ "rtc",
                                                  RETRO_MEMORY_RTC } })
    {
      std::span<std::byte const> const bytes{ _core->Memory(id) };
      if (!bytes.empty())
        regions.push_back(MemoryRegion{ name, bytes });
    }
    if (!_cartridge.Bytes().empty())
      regions.push_back(MemoryRegion{ CARTRIDGE, _cartridge.Bytes() });
    return regions;
  }

  auto Session::HarnessSeconds() const noexcept -> double
  {
    return _fps > 0.0 ? static_cast<double>(_frames) / _fps : 0.0;
  }

  auto Session::HarnessTime() const noexcept -> std::int64_t
  {
    return std::llround(HarnessSeconds()
                        * static_cast<double>(NANOSECONDS_PER_SECOND));
  }

  auto Session::DeterminismLevel() const noexcept -> clock::Determinism
  {
    // docs/design.md 3: one retro_run per frame with the input handed over
    // per frame is D2; D3 is a claim only a replay test makes, per core.
    return clock::Determinism::D2;
  }

  auto Session::Republish(std::span<std::byte const> pixels,
                          std::uint32_t width, std::uint32_t height,
                          std::size_t pitch) -> void
  {
    if (pixels.empty() || width == 0u || height == 0u)
      return;
    _video.Push(bus::FrameDescriptor{ _frames, width, height, pitch,
                                      HarnessSeconds() }, pixels);
  }

  auto Session::PushVideo(std::span<std::byte const> pixels,
                          std::uint32_t width, std::uint32_t height,
                          std::size_t pitch) -> void
  {
    bus::FrameDescriptor const descriptor{ _frames, width, height, pitch,
                                           HarnessSeconds() };
    _video.Push(descriptor, pixels);

    bus::FrameView const frame{ descriptor, pixels };
    std::int64_t const time{ HarnessTime() };
    for (FrameObserver* observer : _observers)
      observer->OnFrame(frame, time);
  }

  auto Session::PushAudio(std::span<std::int16_t const> interleaved) -> void
  {
    _audio.Write(interleaved);
    for (AudioObserver* observer : _audio_observers)
      observer->OnAudio(interleaved);
  }

  auto Session::PollInput() -> void
  {
  }

  auto Session::InputState(unsigned port, unsigned device, unsigned index,
                           unsigned id) -> std::int16_t
  {
    if (device != RETRO_DEVICE_JOYPAD || index != 0 || port >= PORTS ||
        id >= PAD_BUTTONS)
      return 0;
    return ((_pads[port] >> id) & 1u) != 0 ? 1 : 0;
  }
}
