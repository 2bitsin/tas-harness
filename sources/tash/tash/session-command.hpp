#pragma once

#include <_buildutil/reflect.hpp>

#include <oxbox/cli/command.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace tash::cli::detail::session_command
{
  // The port a `tash serve` was started on, when --port says nothing.
  inline constexpr char const* PORT_VARIABLE{ "TASH_MCP_PORT" };

  struct SessionCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(SessionCommand*);

    std::uint16_t port{ 0 };         /* the running server's port; TASH_MCP_PORT when this is 0 */
    std::string host{ "127.0.0.1" }; /* the running server's host */

    /* the tool's own arguments, after a bare --: `tash session act -- --hold A --frames 4` */
    _Label(--) std::vector<std::string> arguments;

    auto operator () (std::string tool) const -> oxbox::cli::CliResult;
  };
}

namespace tash::cli
{
  using detail::session_command::PORT_VARIABLE;
  using detail::session_command::SessionCommand;
}
