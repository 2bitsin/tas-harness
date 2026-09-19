#pragma once

#include <string_view>

namespace tash::clock::detail::determinism
{
  // docs/design.md 3: what an adapter promises about repeating a run.
  enum class Determinism
  {
    D0,  // observation only, no control over time
    D1,  // input is delivered, time still runs on its own
    D2,  // one logic frame per step, input supplied per frame
    D3,  // D2 and bit exact: the target takes dt and seed from the harness
  };

  [[nodiscard]] constexpr auto Named(Determinism level) -> std::string_view
  {
    switch (level)
    {
      case Determinism::D0: return "D0";
      case Determinism::D1: return "D1";
      case Determinism::D2: return "D2";
      case Determinism::D3: return "D3";
    }
    return "D?";
  }
}

namespace tash::clock
{
  using detail::determinism::Determinism;
  using detail::determinism::Named;
}
