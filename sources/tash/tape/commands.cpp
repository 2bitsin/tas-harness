#include "tash/tape/commands.hpp"

#include "tash/tape/anchor-check.hpp"
#include "tash/tape/pointer.hpp"
#include "tash/tape/tape.hpp"
#include "tash/tape/transitions.hpp"

#include <oxbox/cli/main.hpp>

#include <filesystem>
#include <format>
#include <iostream>

namespace tash::tape::detail::commands
{
  using utilities::Result;

  namespace
  {
    [[nodiscard]] auto Wording(anchor::Anchor const& waited,
                               std::filesystem::path const& beside)
      -> std::string
    {
      Result<anchor_check::AnchorCheck> const ready{
        anchor_check::AnchorCheck::For(waited, beside) };
      if (!ready)
        return std::string{ anchor::NameOf(waited.kind) };
      return ready->Wording();
    }
  }

  auto TapeCommand::check(std::string file) -> oxbox::cli::CliResult
  {
    std::filesystem::path const path{ file };
    auto const read{ tape::TapeFrom(path) };
    if (!read)
      return oxbox::cli::CliResult::Failed(1, read.error());

    std::cout << std::format("tape    {} ({} segments)\n", read->header.name,
                             read->segments.size());
    if (read->header.core)
      std::cout << std::format("core    {}\n", *read->header.core);
    if (read->header.profile)
      std::cout << std::format("profile {}\n", *read->header.profile);

    for (tape::Segment const& segment : read->segments)
    {
      auto const moves{ transitions::TransitionsFrom(segment.Lines()) };
      if (!moves)
        return oxbox::cli::CliResult::Failed(1, moves.error());
      auto const points{ pointer::PointerMovesFrom(segment.Points()) };
      if (!points)
        return oxbox::cli::CliResult::Failed(1, points.error());
      std::cout << std::format(
        "  {:<16} {:<40} timeout {:<6} {} transitions", segment.name,
        Wording(segment.Waits(), path), tape::TimeoutOf(*read, segment),
        moves->size());
      if (!points->empty())
        std::cout << std::format(", {} pointer moves", points->size());
      std::cout << "\n";
    }
    return { };
  }

  auto TapeCommands() -> TapeCommand&
  {
    return oxbox::cli::Command::Get<TapeCommand>();
  }

  auto RunTapeCommands(std::span<std::string_view const> arguments)
    -> oxbox::cli::CliResult
  {
    return oxbox::cli::Main(TapeCommands(), arguments, "tash tape");
  }
}
