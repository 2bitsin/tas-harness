#pragma once
// A base's methods are not flattened into a command the way its options are,
// so `check` is declared here and handed straight to the tape module.

#include "tash/tash/replay-command.hpp"
#include "tash/tape/commands.hpp"

#include <oxbox/cli/command.hpp>

#include <string>
#include <utility>

namespace tash::cli::detail::tape_command
{
  struct TapeCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(TapeCommand*);

    auto check(std::string file /* the tape file to read */
              ) -> oxbox::cli::CliResult /* the header, then one line per segment with its anchor, its timeout and how many transitions it holds */
    { return tash::tape::TapeCommands().check(std::move(file)); }

    /* play a bundle's tape from power on and check it comes out the same */
    auto replay() const noexcept -> ReplayCommand&
    { return Command::Get<ReplayCommand>(); }
  };
}

namespace tash::cli
{
  using detail::tape_command::TapeCommand;
}
