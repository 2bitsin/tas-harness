#pragma once

#include <oxbox/cli/command.hpp>

#include <cstdint>
#include <string>

namespace tash::cli::detail::mem_command
{
  struct MemSearchCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(MemSearchCommand*);

    std::string profile;         /* the run profile naming the core and the ROM the search runs, a yaml file or the directory holding its profile.yaml */
    std::string steps;           /* the hunt, one line: run <frames> | hold <button,...> | release | snapshot | equal | changed | increased | decreased | value <n> | list [n], separated by semicolons */
    std::uint32_t width{ 2 };    /* how many bytes a candidate is: 1, 2 or 4 */
    std::string endian{ "big" }; /* which way round those bytes read: big or little */
    bool is_signed{ false };     /* read candidates as two's complement */
    std::uint32_t stride{ 0 };   /* how far apart candidate addresses sit; 0 steps by the width */
    std::string shot;            /* write the frame the script ends on to this .png, to see what the game was showing */

    auto operator () () const -> oxbox::cli::CliResult;
  };

  struct MemCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(MemCommand*);

    /* narrow a running game's memory down to the addresses a number lives at */
    auto search() const noexcept -> MemSearchCommand&
    { return Command::Get<MemSearchCommand>(); }
  };
}

namespace tash::cli
{
  using detail::mem_command::MemCommand;
  using detail::mem_command::MemSearchCommand;
}
