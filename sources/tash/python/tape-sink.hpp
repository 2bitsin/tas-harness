#pragma once
// Where a scenario's tape goes: the cli owns the recording and the trace the
// anchors are read from, so it writes the bundle's tape through this.

#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>

namespace tash::python::detail::tape_sink
{
  using utilities::Result;

  struct TapeWritten
  {
    std::filesystem::path file{ };
    std::uint64_t         frames{ 0 };
  };

  class TapeSink
  {
  public:
    TapeSink()          = default;
    virtual ~TapeSink() = default;

    TapeSink(TapeSink const&)                     = delete;
    auto operator = (TapeSink const&) -> TapeSink& = delete;

    // The line as it stands, folded to the frame the run is at.
    [[nodiscard]] virtual auto WriteTape() -> Result<TapeWritten> = 0;
  };
}

namespace tash::python
{
  using detail::tape_sink::TapeSink;
  using detail::tape_sink::TapeWritten;
}
