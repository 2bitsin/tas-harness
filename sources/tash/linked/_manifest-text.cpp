#include "_manifest-text.hpp"

#include "_bundle-names.hpp"

#include "tash/clock/determinism.hpp"
#include "tash/utilities/version.hpp"

#include <format>

namespace tash::linked::detail::manifest_text
{
  namespace
  {
    // The library watches the target; it does not step it, and the dt the
    // target used is the target's own (design 3).
    constexpr clock::Determinism RECORDED_LEVEL{ clock::Determinism::D1 };
  }

  auto ManifestText(Recorded const& recorded) -> std::string
  {
    std::string text{ std::format(
      "harness_version: {}\ncore_name: {}\ncore_version: ''\nrom: ''\n"
      "profile: {}\nframes: {}\nfps: {}\ndeterminism: {}\noutcome: {}\n"
      "tape: {}\n",
      utilities::HARNESS_VERSION, recorded.target, recorded.profile,
      recorded.frames, recorded.fps, clock::Named(RECORDED_LEVEL),
      recorded.outcome, bundle_names::TAPE_NAME) };
    if (!recorded.watches.empty())
    {
      text += "watches:\n";
      for (std::string const& name : recorded.watches)
        text += std::format("  - {}\n", name);
    }
    return text;
  }
}
