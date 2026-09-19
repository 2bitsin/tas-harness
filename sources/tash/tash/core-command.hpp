#pragma once

#include <oxbox/cli/command.hpp>

#include <string>

namespace tash::cli::detail::core_command
{
  struct CoreCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(CoreCommand*);

    auto info(std::string core /* the core: a path, or a name such as genesis_plus_gx */
             ) -> oxbox::cli::CliResult; /* what the core announces about itself, and the environment calls it asked for and was refused */
  };
}

namespace tash::cli
{
  using detail::core_command::CoreCommand;
}
