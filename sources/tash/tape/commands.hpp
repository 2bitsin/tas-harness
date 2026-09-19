#pragma once

#include <oxbox/cli/command.hpp>

#include <span>
#include <string>
#include <string_view>

namespace tash::tape::detail::commands
{
  struct TapeCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(TapeCommand*);

    auto check(std::string file /* the tape file to read */
              ) -> oxbox::cli::CliResult; /* the header, then one line per segment with its anchor, its timeout and how many transitions it holds */
  };

  [[nodiscard]] auto TapeCommands() -> TapeCommand&;

  auto RunTapeCommands(std::span<std::string_view const> arguments)
    -> oxbox::cli::CliResult;
}

namespace tash::tape
{
  using detail::commands::RunTapeCommands;
  using detail::commands::TapeCommand;
  using detail::commands::TapeCommands;
}
