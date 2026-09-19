#pragma once

#include "tash/bus/audio-ring.hpp"
#include "tash/bus/frame-ring.hpp"
#include "tash/clock/clock.hpp"
#include "tash/clock/determinism.hpp"
#include "tash/libretro/core-sink.hpp"
#include "tash/libretro/core.hpp"
#include "tash/session/audio-observer.hpp"
#include "tash/session/cartridge.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/outcome.hpp"

#include <oxbox/platform/scratch-area.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::session::detail::session
{
  using utilities::Outcome;
  using utilities::Result;

  inline constexpr std::size_t PORTS{ 2 };

  inline constexpr std::int64_t NANOSECONDS_PER_SECOND{ 1'000'000'000 };

  // The frames a reset runs so that what it leaves can be judged.
  inline constexpr std::uint64_t RESET_FRAMES{ 1 };

  // RETRO_DEVICE_ID_JOYPAD ids, as wide as the bitmap a port is held in.
  inline constexpr unsigned PAD_BUTTONS{ 32 };

  struct SessionSettings
  {
    std::filesystem::path core;
    std::filesystem::path rom;
    std::filesystem::path system_directory;
    std::filesystem::path save_directory;
    std::array<unsigned, PORTS> devices{ RETRO_DEVICE_JOYPAD,
                                         RETRO_DEVICE_JOYPAD };
    clock::Pacing pacing{ clock::Pacing::Stepped };
    double rate{ clock::RATE_UNLIMITED };
  };

  // What a caller kept of a moment, as a restore needs it back: the core's
  // state, the frame that was on screen, the frames the run had made, where
  // the moment sits on the line the run keeps, and the line that led there
  // when the moment belongs to another run.
  struct Checkpoint
  {
    std::string_view             name{ };
    std::span<std::byte const>   state{ };
    std::span<std::byte const>   pixels{ };
    bus::FrameDescriptor         at{ };
    std::optional<std::uint64_t> frames{ };
    std::optional<LinePlace>     place{ };
    Line const*                  line{ nullptr };
  };

  // Read-only because one of them is: the cartridge is a file.
  struct MemoryRegion
  {
    std::string_view           name;
    std::span<std::byte const> bytes;
  };

  class Session : public libretro::CoreSink
  {
  public:
    static auto Open(SessionSettings settings)
      -> Result<std::unique_ptr<Session>>;

    Session(oxbox::platform::ScratchArea scratch,
            std::unique_ptr<libretro::Core> core,
            SessionSettings settings);
    ~Session() override;

    Session(Session const&) = delete;
    auto operator=(Session const&) -> Session& = delete;

    // Every frame goes to every observer, in the order they were added,
    // before the step that produced it returns.
    auto Observe(FrameObserver& observer) -> void;

    auto Observe(AudioObserver& observer) -> void;

    // Where pad changes and restores are written; nothing to write is fine.
    auto Record(trace::Writer* into) noexcept -> void { _trace = into; }

    auto Step(std::uint64_t frames) -> void;

    // Runs until `holds` answers true, and refuses when it never does.
    auto RunUntil(std::function<bool()> const& holds,
                  std::uint64_t timeout_frames) -> Result<std::uint64_t>;

    // Changes the pacing of what follows; zero runs as fast as the core does.
    auto Pace(double rate) -> void;

    auto HoldPad(std::size_t port, std::uint32_t buttons) -> void;
    [[nodiscard]] auto Pad(std::size_t port) const -> std::uint32_t;

    [[nodiscard]] auto SaveState() const -> Result<std::vector<std::byte>>
    { return _core->SaveState(); }
    auto LoadState(std::span<std::byte const> state) -> Outcome;

    // The one way back: state loaded, frame republished, observers told.
    auto Restore(Checkpoint const& kept) -> Outcome;

    // Power on again: the library is opened afresh, the counters stand and
    // RESET_FRAMES frames run, so the frame on the bus is the new machine's.
    [[nodiscard]] auto Reset() -> Outcome;

    // The frame the machine now running was powered on at, which is where
    // the line a tape plays begins.
    [[nodiscard]] auto PoweredOnAt() const noexcept -> std::uint64_t
    { return _powered_on_at; }

    [[nodiscard]] auto Memory() const -> std::vector<MemoryRegion>;

    [[nodiscard]] auto Frames() const noexcept -> std::uint64_t
    { return _frames; }
    [[nodiscard]] auto Fps() const noexcept -> double { return _fps; }

    // Emulated seconds produced so far, which is the time a tape counts in.
    [[nodiscard]] auto HarnessSeconds() const noexcept -> double;

    // The same instant a trace record dates itself by.
    [[nodiscard]] auto HarnessTime() const noexcept -> std::int64_t;

    [[nodiscard]] auto WallSeconds() const -> double
    { return _clock.WallSeconds(); }
    [[nodiscard]] auto AchievedRate() const -> double
    { return _clock.AchievedRate(_frames); }

    [[nodiscard]] auto DeterminismLevel() const noexcept -> clock::Determinism;

    [[nodiscard]] auto Video() noexcept -> bus::FrameRing& { return _video; }
    [[nodiscard]] auto Audio() noexcept -> bus::AudioRing& { return _audio; }
    [[nodiscard]] auto CoreOf() const noexcept -> libretro::Core const&
    { return *_core; }
    [[nodiscard]] auto RomPath() const noexcept -> std::filesystem::path const&
    { return _rom; }

    auto PushVideo(std::span<std::byte const> pixels, std::uint32_t width,
                   std::uint32_t height, std::size_t pitch) -> void override;
    auto PushAudio(std::span<std::int16_t const> interleaved) -> void override;
    auto PollInput() -> void override;
    auto InputState(unsigned port, unsigned device, unsigned index,
                    unsigned id) -> std::int16_t override;

  private:
    auto Begin() -> Outcome;

    // The core made ready for the cartridge, as a launch and a reset need.
    auto PowerOn() -> Outcome;

    auto Reopened() -> Outcome;

    // A core does not render on unserialize, so a restored state has no
    // frame until something steps: this puts the saved one back on the bus.
    auto Republish(std::span<std::byte const> pixels, std::uint32_t width,
                   std::uint32_t height, std::size_t pitch) -> void;

    oxbox::platform::ScratchArea _scratch;
    std::unique_ptr<libretro::Core> _core;
    SessionSettings _settings;
    std::filesystem::path _rom;
    Cartridge _cartridge;
    bus::FrameRing _video;
    bus::AudioRing _audio;
    clock::Clock _clock{ clock::Clock::Stepped() };
    double _fps{ 0.0 };
    std::uint64_t _frames{ 0 };
    std::uint64_t _powered_on_at{ 0 };
    std::array<std::uint32_t, PORTS> _pads{};
    trace::Writer* _trace{ nullptr };
    std::vector<FrameObserver*> _observers;
    std::vector<AudioObserver*> _audio_observers;
  };
}

namespace tash::session
{
  using detail::session::Checkpoint;
  using detail::session::MemoryRegion;
  using detail::session::NANOSECONDS_PER_SECOND;
  using detail::session::PAD_BUTTONS;
  using detail::session::PORTS;
  using detail::session::RESET_FRAMES;
  using detail::session::Session;
  using detail::session::SessionSettings;
}
