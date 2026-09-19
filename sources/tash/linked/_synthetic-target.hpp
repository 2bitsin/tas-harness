#pragma once
// A target with no window and no SDL in it: ten frames of pads, a pointer
// walking across them, three watches, one event, and a picture that never
// changes -- everything a recording has to come back with.

#include "tash/linked/tash.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace tash::linked::detail::synthetic_target
{
  inline constexpr std::uint32_t WIDTH{ 32 };
  inline constexpr std::uint32_t HEIGHT{ 16 };
  inline constexpr std::uint64_t FRAMES{ 10 };
  inline constexpr std::uint64_t EVENT_FRAME{ 5 };
  inline constexpr std::uint64_t SEED{ 20260916 };
  inline constexpr double        DT{ 1.0 / 60.0 };

  inline constexpr std::string_view EVENT_NAME{ "door" };
  inline constexpr std::string_view EVENT_TEXT{ "opened" };

  struct Guest
  {
    std::uint16_t room{ 3 };
    std::int32_t  score{ -7 };
    std::uint64_t step{ 0 };
  };

  [[nodiscard]] inline auto Picture() -> std::vector<std::uint16_t>
  {
    std::vector<std::uint16_t> pixels(std::size_t{ WIDTH } * HEIGHT, 0u);
    for (std::size_t at{ 0 }; at < pixels.size(); ++at)
      pixels[at] = static_cast<std::uint16_t>(at * 37u + 11u);
    return pixels;
  }

  [[nodiscard]] inline auto Frame(std::vector<std::uint16_t> const& picture)
    -> Video
  {
    return Video{ std::as_bytes(std::span{ picture }), WIDTH, HEIGHT,
                  std::size_t{ WIDTH } * 2u, Pixels::RGB565 };
  }

  // What the target's own devices said on a frame, so what a tape reads
  // back has something to be equal to.
  [[nodiscard]] inline auto Sampled(std::uint64_t frame) -> Input
  {
    Input input{ };
    if (frame >= 2u && frame < 6u)
      input.pads[0].buttons |= Bit(Button::START);
    if (frame >= 3u)
      input.pads[1].buttons |= Bit(Button::RIGHT);
    if (frame >= 4u && frame < 7u)
      input.mice[0].buttons |= Bit(MouseButton::LEFT);
    input.mice[0].x = static_cast<std::int32_t>(100u + frame);
    input.mice[0].y = 50;
    return input;
  }

  inline auto Play(Harness& harness, Guest& guest,
                   std::vector<std::uint16_t> const& picture) -> void
  {
    harness.Register("room", &guest.room, Number::U16);
    harness.Register("score", &guest.score, Number::I32);
    auto const step{ [&guest]
                     { return static_cast<std::int64_t>(guest.step); } };
    harness.Register(Watch{ "step", nullptr, Number::I64, step });
    harness.Register(State{
      [&guest] { return std::vector<std::byte>(sizeof(guest), std::byte{ }); },
      [](std::span<std::byte const>) { return true; } });

    guest.step = harness.Seed(SEED);
    double const dt{ harness.Dt(DT) };

    for (std::uint64_t frame{ 0 }; frame < FRAMES; ++frame)
    {
      Input input{ };
      if (!harness.Begin(input))
        input = Sampled(frame);
      harness.Submit(input);

      guest.room  = static_cast<std::uint16_t>(3u + frame);
      guest.score = static_cast<std::int32_t>(-7 + static_cast<int>(frame));
      guest.step  = static_cast<std::uint64_t>(dt * 1000.0) + frame;

      harness.Submit(Frame(picture));
      harness.Submit(Audio{ { }, 48000u, 2u });

      if (frame == EVENT_FRAME)
        harness.Event(EVENT_NAME, EVENT_TEXT);
    }
    harness.Flush();
  }
}
