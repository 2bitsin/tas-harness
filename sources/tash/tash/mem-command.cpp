#include "tash/tash/mem-command.hpp"

#include "tash/session/session.hpp"
#include "tash/tash/profile.hpp"
#include "tash/recorder/frame-png.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/memory-search.hpp"
#include "tash/watches/search-driver.hpp"
#include "tash/watches/search-script.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <utility>
#include <vector>

namespace tash::cli::detail::mem_command
{
  using utilities::Outcome;
  using utilities::Result;

  auto MemSearchCommand::operator () () const -> oxbox::cli::CliResult
  {
    Result<RunProfile> const read{ ProfileFrom(profile) };
    if (!read)
      return oxbox::cli::CliResult::UsageError(read.error());

    Result<watches::Endianness> const endianness{
      watches::EndiannessOf(endian) };
    if (!endianness)
      return oxbox::cli::CliResult::UsageError(endianness.error());
    Result<std::uint32_t> const checked{ watches::WidthChecked(width) };
    if (!checked)
      return oxbox::cli::CliResult::UsageError(checked.error());
    Result<std::vector<watches::SearchStep>> const script{
      watches::StepsFrom(steps) };
    if (!script)
      return oxbox::cli::CliResult::UsageError(script.error());

    Result<session::SessionSettings> settings{ SettingsFrom(*read) };
    if (!settings)
      return oxbox::cli::CliResult::Failed(1, settings.error());
    Result<std::unique_ptr<session::Session>> const session{
      session::Session::Open(std::move(*settings)) };
    if (!session)
      return oxbox::cli::CliResult::Failed(1, session.error());

    session::Session& run{ **session };
    std::print("core   {} {}\n", run.CoreOf().Information().name,
               run.CoreOf().Information().version);
    std::print("rom    {}\n", run.RomPath().string());

    watches::MemorySearch search{ watches::MemoryMap::Of(run),
                                  watches::NumberFormat{ width, *endianness,
                                                         is_signed },
                                  stride };
    for (watches::MemoryArea const& area : watches::MemoryMap::Of(run).Areas())
      std::print("memory {:<8} {} bytes\n", area.name, area.bytes.size());

    watches::SearchDriver driver{ run, search };
    if (Outcome const played{ driver.Play(*script, std::cout) }; !played)
      return oxbox::cli::CliResult::Failed(1, played.error());

    std::print("left   {} candidates after {} frames\n", search.Count(),
               run.Frames());
    if (!shot.empty())
    {
      std::optional<bus::FrameView> const last{ run.Video().Latest() };
      if (!last)
        return oxbox::cli::CliResult::Failed(1, "the run produced no frame");
      if (Outcome const written{ recorder::WritePng(*last, shot) }; !written)
        return oxbox::cli::CliResult::Failed(1, written.error());
      std::print("shot   {}\n", shot);
    }
    return {};
  }
}
