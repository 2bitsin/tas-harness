#include "tash/mcp/observation-wording.hpp"

#include <cstddef>
#include <format>
#include <string>

namespace tash::mcp::detail::observation_wording
{
  auto Wording(python::Observation const& seen) -> std::string
  {
    std::string said{ std::format(
      "frame {} time {:.3f} change {:.4f} size {}x{}\n"
      "exact {:016x} difference {:016x} perceptual {:016x}",
      seen.frame, seen.time, seen.change, seen.width, seen.height,
      seen.exact, seen.difference, seen.perceptual) };
    for (std::size_t port{ 0 }; port < seen.pads.size(); ++port)
    {
      said += std::format("\npad p{}", port + 1);
      if (seen.pads[port].empty())
        said += " nothing";
      for (std::string const& button : seen.pads[port])
        said += std::format(" {}", button);
    }
    for (auto const& [name, value] : seen.watches)
      said += value ? std::format("\nwatch {} {}", name, *value)
                    : std::format("\nwatch {} unread", name);
    return said;
  }
}
