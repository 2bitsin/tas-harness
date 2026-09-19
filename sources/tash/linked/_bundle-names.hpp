#pragma once
// A bundle's own names, so every tool that opens one opens a recording.

#include <string_view>

namespace tash::linked::detail::bundle_names
{
  inline constexpr std::string_view TAPE_NAME{ "tape.yaml" };
  inline constexpr std::string_view TRACE_NAME{ "trace.bin" };
  inline constexpr std::string_view MANIFEST_NAME{ "run.yaml" };
}
