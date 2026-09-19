#include "tash/tash/run-command.hpp"

#include "tash/perception/region.hpp"
#include "tash/recorder/frame-png.hpp"
#include "tash/recorder/run-manifest.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/profile.hpp"
#include "tash/tash/run-line.hpp"
#include "tash/tash/scenario-runner.hpp"
#include "tash/tape/player.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/memory-search.hpp"
#include "tash/watches/search-driver.hpp"
#include "tash/watches/search-script.hpp"

#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::cli::detail::run_command
{
  using utilities::Outcome;
  using utilities::Refused;
  using utilities::Result;

  // What a trace file names as the thing that wrote it.
  inline constexpr std::string_view PRODUCER{ "tash run" };

  namespace
  {
    auto CropFrom(std::string_view text)
      -> Result<std::optional<perception::Region>>
    {
      if (text.empty())
        return std::optional<perception::Region>{ };
      Result<perception::Region> const read{ perception::RegionFrom(text) };
      if (!read)
        return Refused("--shot-region: {}", read.error());
      return *read;
    }
  }

  auto RunCommand::operator () () const -> oxbox::cli::CliResult
  {
    Result<std::optional<perception::Region>> const crop{
      CropFrom(shot_region) };
    if (!crop)
      return oxbox::cli::CliResult::UsageError(crop.error());

    if (video_stride == 0)
      return oxbox::cli::CliResult::UsageError(
        "--video-stride counts frames, so it is at least 1");

    if (!trace.empty() && !bundle.empty())
      return oxbox::cli::CliResult::UsageError(
        "--trace and --bundle both name where the trace goes; pick one");

    Result<std::filesystem::path> const file{ ProfileFileAt(profile) };
    if (!file)
      return oxbox::cli::CliResult::UsageError(file.error());
    Result<RunProfile> const read{ ProfileFrom(*file) };
    if (!read)
      return oxbox::cli::CliResult::UsageError(read.error());

    RunRequest wanted;
    wanted.profile = *read;
    wanted.profile_path = *file;
    wanted.trace = trace;
    wanted.bundle = bundle;
    wanted.checkpoints = CheckpointsUnder(checkpoints, bundle);
    wanted.name = name;
    wanted.rate = rate;
    wanted.video_stride = video_stride;
    wanted.producer = PRODUCER;

    Result<std::unique_ptr<OpenedRun>> const opened{
      OpenedRun::Open(std::move(wanted)) };
    if (!opened)
      return oxbox::cli::CliResult::Failed(1, opened.error());

    OpenedRun& open{ **opened };
    session::Session& run{ open.Live() };
    std::print("core   {} {}\n", run.CoreOf().Information().name,
               run.CoreOf().Information().version);
    std::print("rom    {}\n", run.RomPath().string());

    if (!tape.empty())
    {
      Result<tash::tape::Player> player{ tash::tape::Player::Of(tape) };
      if (!player)
        return oxbox::cli::CliResult::UsageError(player.error());

      tash::tape::PlayOptions options;
      options.watches = open.WatchedOf();
      options.report = &std::cout;

      Result<tash::tape::PlayCounts> const played{
        player->Play(run, options) };
      if (!played)
        return oxbox::cli::CliResult::Failed(1, played.error());
      std::print("tape   {} ({} segments, {} frames, {} transitions, "
                 "{} retries)\n", tape, played->segments, played->frames,
                 played->transitions, played->retries);
    }

    std::string refusal;
    if (!scenario.empty())
    {
      ScenarioRunner runner;
      if (Outcome const played{ runner.Play(open.ScenarioOf(), scenario) };
          !played)
        refusal = played.error();
    }
    else if (steps.empty())
      run.Step(frames);
    else
    {
      Result<std::vector<watches::SearchStep>> const script{
        watches::StepsFrom(steps) };
      if (!script)
        return oxbox::cli::CliResult::UsageError(script.error());
      watches::MemorySearch search{ watches::MemoryMap::Of(run),
                                    watches::NumberFormat{} };
      watches::SearchDriver driver{ run, search };
      if (Outcome const played{ driver.Play(*script, std::cout) }; !played)
        return oxbox::cli::CliResult::Failed(1, played.error());
    }

    std::cout << RanLine(run, open.KeptOf(), open.ProbesOf());
    if (watches::Sampler* const sampler{ open.SamplerOf() })
    {
      std::size_t index{ 0 };
      for (watches::Watch const& watch : sampler->Watches().All())
      {
        std::print("watch  {:<12} {}\n", watch.name,
                   sampler->ValueAt(index).value_or(0));
        ++index;
      }
      std::print("       {} watch records, {} refused\n",
                 sampler->Counts().written, sampler->Counts().refused);
    }

    Result<RunClosed> const closed{ open.Close() };
    if (!closed)
      return oxbox::cli::CliResult::Failed(1, closed.error());
    if (closed->journal)
      std::print("trace  {} ({} recorded, {} dropped)\n", trace,
                 closed->journal->recorded, closed->journal->dropped);
    if (closed->recording)
    {
      std::print("bundle {} ({} encoded, {} dropped, {} recorded)\n",
                 open.BundleOf()->Root().string(), closed->recording->encoded,
                 closed->recording->dropped, closed->recording->recorded);
      if (video_stride > recorder::EVERY_FRAME)
        std::print("video  one frame in {}, no audio\n", video_stride);
      std::print("report {}\n", closed->page->filename().string());
      if (closed->recording->audio_dropped != 0)
        std::print("audio  {} block{} never reached the recording\n",
                   closed->recording->audio_dropped,
                   closed->recording->audio_dropped == 1 ? "" : "s");
      if (!closed->problem.empty())
        std::print("problem {}\n", closed->problem);
    }

    if (!refusal.empty())
      return oxbox::cli::CliResult::Failed(1, refusal);

    if (!shot.empty())
    {
      std::optional<bus::FrameView> const last{ run.Video().Latest() };
      if (!last)
        return oxbox::cli::CliResult::Failed(1, "the run produced no frame");

      // A bundle keeps its own shot, named by the frame, and never takes the
      // path the caller asked for away from them.
      std::vector<std::filesystem::path> wanted{ std::filesystem::path{
        shot } };
      if (open.BundleOf() != nullptr)
        wanted.push_back(open.BundleOf()->Shots()
                         / std::format("{}.png", last->descriptor.number));

      for (std::filesystem::path const& where : wanted)
      {
        Outcome const written{ *crop
          ? recorder::WritePng(*last, **crop, where)
          : recorder::WritePng(*last, where) };
        if (!written)
          return oxbox::cli::CliResult::Failed(1, written.error());
        std::print("shot   {} ({}x{})\n", where.string(),
                   *crop ? (*crop)->width : last->descriptor.width,
                   *crop ? (*crop)->height : last->descriptor.height);
      }
    }
    return {};
  }
}
