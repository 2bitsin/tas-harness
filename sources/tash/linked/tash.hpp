#pragma once
// libtash: what a target links so the harness can drive it, record it, or
// not be there at all. This header is the whole of the api; README.md is
// the reference.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::linked::detail::recording
{
  class Recording;
}

namespace tash::linked::detail::harness
{
  // The header a target compiled against, refused when the library it links
  // was built from another one.
  inline constexpr std::uint32_t API_VERSION{ 1 };

  inline constexpr std::size_t PADS{ 2 };
  inline constexpr std::size_t PAD_AXES{ 4 };
  inline constexpr std::size_t MICE{ 2 };
  inline constexpr std::size_t KEYS{ 256 };
  inline constexpr std::size_t KEY_WORDS{ KEYS / 64 };

  // The directory file mode records into, and the control channel attached
  // mode will connect to.
  inline constexpr std::string_view RECORD_VARIABLE{ "TASH_RECORD" };
  inline constexpr std::string_view SESSION_VARIABLE{ "TASH_SESSION" };

  enum class Mode
  {
    NONE,      // nothing is listening; every call is a branch and no more
    FILE,      // the library writes down what it is handed
    ATTACHED   // the harness drives the frames
  };

  enum class Sink
  {
    ENVIRONMENT,  // the variables above decide
    DIRECTORY,
    MEMORY,       // a host with no filesystem fetches the bytes
    NONE
  };

  enum class Pixels
  {
    RGB565,
    RGB888,
    XRGB8888,
    RGBA8888
  };

  enum class Number
  {
    U8, I8, U16, I16, U32, I32, U64, I64
  };

  enum class Artifact
  {
    TAPE,
    TRACE,
    MANIFEST
  };

  // The joypad bits, in the order the tape's channel names are in.
  enum class Button
  {
    B, Y, SELECT, START, UP, DOWN, LEFT, RIGHT,
    A, X, L, R, L2, R2, L3, R3
  };

  enum class MouseButton
  {
    LEFT, RIGHT, MIDDLE, WHEEL_UP, WHEEL_DOWN
  };

  template <typename Held>
  [[nodiscard]] constexpr auto Bit(Held button) noexcept -> std::uint32_t
  {
    return std::uint32_t{ 1 } << static_cast<unsigned>(button);
  }

  struct Pad
  {
    std::uint32_t                      buttons{ 0 };
    std::array<std::int16_t, PAD_AXES> axes{ };

    [[nodiscard]] auto operator == (Pad const&) const noexcept
      -> bool = default;
  };

  struct Mouse
  {
    std::int32_t  x{ 0 };
    std::int32_t  y{ 0 };
    std::uint32_t buttons{ 0 };

    [[nodiscard]] auto operator == (Mouse const&) const noexcept
      -> bool = default;
  };

  // The whole of the devices for one frame, not the events that changed
  // them, so a target reading this does not depend on the order they
  // arrived in.
  struct Input
  {
    std::array<Pad, PADS>                pads{ };
    std::array<Mouse, MICE>              mice{ };
    std::array<std::uint64_t, KEY_WORDS> keys{ };

    [[nodiscard]] auto operator == (Input const&) const noexcept
      -> bool = default;
  };

  struct Video
  {
    std::span<std::byte const> pixels{ };
    std::uint32_t              width{ 0 };
    std::uint32_t              height{ 0 };
    std::size_t                pitch{ 0 };  // bytes a row, padding included
    Pixels                     format{ Pixels::RGB565 };
  };

  struct Audio
  {
    std::span<std::int16_t const> samples{ };  // interleaved
    std::uint32_t                 rate{ 0 };
    std::uint32_t                 channels{ 0 };
  };

  // A number the harness reads every frame: an address of that width, or a
  // reader that yields the value when the target keeps it somewhere an
  // address cannot say.
  struct Watch
  {
    std::string_view              name{ };
    void const*                   address{ nullptr };
    Number                        number{ Number::I64 };
    std::function<std::int64_t()> reader{ };
  };

  // What a checkpoint calls: `save` answers the bytes the target is, `load`
  // takes them back and says whether it did.
  struct State
  {
    std::function<std::vector<std::byte>()>         save{ };
    std::function<bool(std::span<std::byte const>)> load{ };
  };

  struct Options
  {
    std::string                            target{ };
    std::string                            profile{ };
    Sink                                   sink{ Sink::ENVIRONMENT };
    std::filesystem::path                  directory{ };
    double                                 fps{ 0.0 };

    // Where a refusal goes: the library owns no stream of the target's.
    std::function<void(std::string_view)>  note{ };

    std::uint32_t                          api{ API_VERSION };
  };

  // The target's handle on the harness, and on nothing when there is none.
  // Every call is the test of one pointer before the work, so a build with
  // nothing attached pays a branch and never a call.
  class Harness
  {
  public:
    explicit Harness(Options options);

    ~Harness();

    [[nodiscard]] auto Mode() const noexcept -> harness::Mode
    { return _recording ? ModeOf() : harness::Mode::NONE; }

    // Whether anything is at the other end at all: a harness driving, or
    // a recording being written.
    [[nodiscard]] auto Attached() const noexcept -> bool
    { return _recording != nullptr; }

    // True when the harness filled the input; false when the frame is the
    // target's own to sample, which is what Frame() then submits.
    [[nodiscard]] auto Begin(Input& input) -> bool
    { return _recording ? Began(input) : false; }

    auto Frame(Input& input) -> void
    {
      if (!Begin(input))
        Submit(input);
    }

    auto Submit(Input const& input) -> void
    { if (_recording) Submitted(input); }

    auto Submit(Video const& video) -> void
    { if (_recording) Submitted(video); }

    auto Submit(Audio const& audio) -> void
    { if (_recording) Submitted(audio); }

    // Answers the index the trace records this watch under, or nothing
    // when it is refused; registering is worth doing for its own sake.
    auto Register(Watch const& watch) -> std::optional<std::size_t>
    { return _recording ? Registered(watch) : std::nullopt; }

    auto Register(std::string_view name, void const* address, Number number)
      -> std::optional<std::size_t>
    { return Register(Watch{ name, address, number, { } }); }

    auto Register(State state) -> void
    { if (_recording) Registered(std::move(state)); }

    auto Event(std::string_view name, std::string_view text) -> void
    { if (_recording) Emitted(name, text); }

    // The step to advance by and the seed to start from: what the target
    // would have used when nobody is driving, written down when it is
    // recorded.
    [[nodiscard]] auto Dt(double own) -> double
    { return _recording ? Stepped(own) : own; }

    [[nodiscard]] auto Seed(std::uint64_t own) -> std::uint64_t
    { return _recording ? Seeded(own) : own; }

    // Everything recorded so far is complete: the files on disk are whole
    // and the bytes below are the whole of them.
    auto Flush() -> void
    { if (_recording) Flushed(); }

    // What a memory sink holds, for a host that saves the bytes its own
    // way; empty in every other state. They live until the next call.
    [[nodiscard]] auto Bytes(Artifact which) const -> std::span<std::byte const>
    { return _recording ? Fetched(which) : std::span<std::byte const>{ }; }

  private:
    [[nodiscard]] auto ModeOf() const -> harness::Mode;
    [[nodiscard]] auto Began(Input& input) -> bool;
    auto Submitted(Input const& input) -> void;
    auto Submitted(Video const& video) -> void;
    auto Submitted(Audio const& audio) -> void;
    auto Registered(Watch const& watch) -> std::optional<std::size_t>;
    auto Registered(State state) -> void;
    auto Emitted(std::string_view name, std::string_view text) -> void;
    [[nodiscard]] auto Stepped(double own) -> double;
    [[nodiscard]] auto Seeded(std::uint64_t own) -> std::uint64_t;
    auto Flushed() -> void;
    [[nodiscard]] auto Fetched(Artifact which) const
      -> std::span<std::byte const>;

    std::unique_ptr<recording::Recording> _recording;
  };
}

namespace tash::linked
{
  using detail::harness::API_VERSION;
  using detail::harness::Artifact;
  using detail::harness::Audio;
  using detail::harness::Bit;
  using detail::harness::Button;
  using detail::harness::Harness;
  using detail::harness::Input;
  using detail::harness::KEY_WORDS;
  using detail::harness::KEYS;
  using detail::harness::MICE;
  using detail::harness::Mode;
  using detail::harness::Mouse;
  using detail::harness::MouseButton;
  using detail::harness::Number;
  using detail::harness::Options;
  using detail::harness::Pad;
  using detail::harness::PAD_AXES;
  using detail::harness::PADS;
  using detail::harness::Pixels;
  using detail::harness::RECORD_VARIABLE;
  using detail::harness::SESSION_VARIABLE;
  using detail::harness::Sink;
  using detail::harness::State;
  using detail::harness::Video;
  using detail::harness::Watch;
}
