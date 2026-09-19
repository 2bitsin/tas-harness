#pragma once

#include "tash/tash/core-command.hpp"
#include "tash/tash/mem-command.hpp"
#include "tash/tash/report-command.hpp"
#include "tash/tash/run-command.hpp"
#include "tash/tash/serve-command.hpp"
#include "tash/tash/session-command.hpp"
#include "tash/tash/tape-command.hpp"
#include "tash/trace/commands.hpp"

#include <oxbox/cli/command.hpp>

namespace tash::cli::detail::tash_cli
{
  struct TashCli : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(TashCli*);

    /* what a libretro core announces, without running it */
    auto core() const noexcept -> CoreCommand&
    { return Command::Get<CoreCommand>(); }

    /* narrow a running game's memory to the addresses a number lives at */
    auto mem() const noexcept -> MemCommand&
    { return Command::Get<MemCommand>(); }

    /* draw a run bundle's report.html from its trace */
    auto report() const noexcept -> ReportCommand&
    { return Command::Get<ReportCommand>(); }

    /* run a profile headless for a number of frames */
    auto run() const noexcept -> RunCommand&
    { return Command::Get<RunCommand>(); }

    /* serve the run over MCP, so an agent drives it a tool at a time */
    auto serve() const noexcept -> ServeCommand&
    { return Command::Get<ServeCommand>(); }

    /* call one tool on a running `tash serve` and print what it said */
    auto session() const noexcept -> SessionCommand&
    { return Command::Get<SessionCommand>(); }

    /* read a trace file a run wrote */
    auto trace() const noexcept -> tash::trace::TraceCommand&
    { return Command::Get<tash::trace::TraceCommand>(); }

    /* read a tape a run would play, or replay the one a bundle recorded */
    auto tape() const noexcept -> TapeCommand&
    { return Command::Get<TapeCommand>(); }
  };
}

namespace tash::cli
{
  using detail::tash_cli::TashCli;
}
