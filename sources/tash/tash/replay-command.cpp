#include "tash/tash/replay-command.hpp"

#include "tash/perception/frame-hash.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/encoder-settings.hpp"
#include "tash/recorder/run-manifest.hpp"
#include "tash/report/render.hpp"
#include "tash/session/session.hpp"
#include "tash/tape/player.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/profile.hpp"
#include "tash/tash/run-line.hpp"
#include "tash/tash/segment-marks.hpp"
#include "tash/trace/line.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/watches/sampler.hpp"
#include "tash/watches/watch-set.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tash::cli::detail::replay_command
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Refused;
  using utilities::Result;

  // What a recorded replay's trace names as the thing that wrote it.
  inline constexpr std::string_view PRODUCER{ "tash tape replay" };

  namespace
  {
    using Reading = std::optional<std::int64_t>;

    // How the run ended, as its own trace recorded it.
    struct Ending
    {
      std::uint64_t        frames{ 0 };
      std::uint64_t        hash{ 0 };
      bool                 any{ false };
      std::vector<Reading> watches{ };
    };

    // What a restore folded off the line is not what the run came out as,
    // so every record it left behind is passed over here too.
    [[nodiscard]] auto EndingIn(std::filesystem::path const& path,
                                std::size_t watches) -> Result<Ending>
    {
      Result<trace::Line> const played{ trace::Line::Of(path) };
      if (!played)
        return Forwarded(played);

      Result<trace::Reader> reader{ trace::Reader::Open(path) };
      if (!reader)
        return Forwarded(reader);

      Ending done{ };
      done.frames = played->Frames();
      done.watches.resize(watches);
      reader->ForEach([&done, &played](auto const& record) {
        using Held = std::decay_t<decltype(record)>;
        if (!played->Holds(record.frame))
          return;
        if constexpr (std::is_same_v<Held, trace::FrameRecord>)
        {
          done.hash = record.hash_exact;
          done.any = true;
        }
        else if constexpr (std::is_same_v<Held, trace::WatchRecord>)
        {
          if (record.watch < done.watches.size())
            done.watches[record.watch] = record.value;
        }
      });
      if (!done.any)
        return Refused("replay: '{}' recorded no frame", path.string());
      return done;
    }

    [[nodiscard]] auto Spelled(Reading value) -> std::string
    {
      return value ? std::format("{}", *value) : std::string{ "nothing" };
    }

    [[nodiscard]] auto Differs(watches::Sampler const* sampler,
                               std::vector<Reading> const& recorded)
      -> std::optional<std::string>
    {
      if (sampler == nullptr)
        return { };
      for (std::size_t index{ 0 }; index < recorded.size(); ++index)
      {
        Result<std::int64_t> const read{ sampler->ValueAt(index) };
        Reading const now{ read ? Reading{ *read } : Reading{ } };
        if (now == recorded[index])
          continue;
        return std::format(
          "replay: watch '{}' differs: the run recorded {} and the replay "
          "read {}", sampler->Watches().All()[index].name,
          Spelled(recorded[index]), Spelled(now));
      }
      return { };
    }

    // The two checks the replay exists for, answering the hash it matched.
    [[nodiscard]] auto Matched(session::Session& run,
                               watches::Sampler const* sampler,
                               Ending const& ending) -> Result<std::uint64_t>
    {
      std::optional<bus::FrameView> const last{ run.Video().Latest() };
      if (!last)
        return Refused("replay: the tape played no frame");
      Result<std::uint64_t> const hash{ perception::ExactHash(*last) };
      if (!hash)
        return Forwarded(hash);
      if (*hash != ending.hash)
        return Refused("replay: the final frame differs: the run recorded "
                       "{:016x} after {} frames and the replay produced "
                       "{:016x} after {}", ending.hash, ending.frames, *hash,
                       run.Frames());
      if (std::optional<std::string> const moved{
            Differs(sampler, ending.watches) })
        return Refused("{}", *moved);
      return *hash;
    }

    // What the bundle a replay records says it is: the line it played, and
    // the first thing that went wrong over it -- a check the replay exists
    // for, then whatever the recording itself could not finish.
    [[nodiscard]] auto Named(recorder::Bundle const& made,
                             std::filesystem::path const& source,
                             std::string const& refusal,
                             std::string const& problem) -> Outcome
    {
      Result<recorder::RunManifest> written{ made.Read() };
      if (!written)
        return Forwarded(written);
      written->replay_of = source.string();
      if (!refusal.empty())
        written->outcome = refusal;
      else
        written->outcome = problem.empty() ? std::string{ "completed" }
                                           : problem;
      return made.Write(*written);
    }
  }

  auto ReplayCommand::operator () () const -> oxbox::cli::CliResult
  {
    if (video_stride == 0)
      return oxbox::cli::CliResult::UsageError(
        "--video-stride counts frames, so it is at least 1");

    Result<recorder::Bundle> const opened{ recorder::Bundle::At(bundle) };
    if (!opened)
      return oxbox::cli::CliResult::UsageError(opened.error());
    Result<recorder::RunManifest> const manifest{ opened->Read() };
    if (!manifest)
      return oxbox::cli::CliResult::Failed(1, manifest.error());

    std::filesystem::path const written{
      opened->Root()
      / manifest->tape.value_or(std::string{ recorder::TAPE_NAME }) };
    if (!std::filesystem::is_regular_file(written))
      return oxbox::cli::CliResult::Failed(
        1, std::format("replay: no tape at '{}'", written.string()));

    Result<RunProfile> const profile{ ProfileFrom(manifest->profile) };
    if (!profile)
      return oxbox::cli::CliResult::Failed(1, profile.error());
    Result<watches::WatchSet> const watched{ WatchesFrom(*profile) };
    if (!watched)
      return oxbox::cli::CliResult::Failed(1, watched.error());
    if (manifest->watches && manifest->watches->size() != watched->Count())
      return oxbox::cli::CliResult::Failed(
        1, std::format("replay: the run followed {} watches and '{}' now "
                       "names {}", manifest->watches->size(),
                       manifest->profile, watched->Count()));

    Result<Ending> const ending{ EndingIn(opened->Trace(),
                                          watched->Count()) };
    if (!ending)
      return oxbox::cli::CliResult::Failed(1, ending.error());

    RunRequest wanted;
    wanted.profile = *profile;
    wanted.profile_path = manifest->profile;
    wanted.bundle = record;
    wanted.name = name;
    wanted.video_stride = video_stride;
    wanted.quality = recorder::Quality::FILM;
    wanted.producer = PRODUCER;

    Result<std::unique_ptr<OpenedRun>> const made{
      OpenedRun::Open(std::move(wanted)) };
    if (!made)
      return oxbox::cli::CliResult::Failed(1, made.error());
    OpenedRun& open{ **made };
    session::Session& run{ open.Live() };

    Result<tape::Player> player{ tape::Player::Of(written) };
    if (!player)
      return oxbox::cli::CliResult::Failed(1, player.error());

    SegmentMarks marks{ open.ScenarioOf() };
    tape::PlayOptions options;
    options.watches = open.WatchedOf();
    options.marks = &marks;

    std::print("core   {} {}\n", run.CoreOf().Information().name,
               run.CoreOf().Information().version);
    std::print("rom    {}\n", run.RomPath().string());

    std::string refusal;
    std::uint64_t hash{ 0 };
    Result<tape::PlayCounts> const played{ player->Play(run, options) };
    if (!played)
      refusal = played.error();
    else
    {
      std::cout << RanLine(run, open.KeptOf(), open.ProbesOf());
      Result<std::uint64_t> const checked{
        Matched(run, open.SamplerOf(), *ending) };
      if (!checked)
        refusal = checked.error();
      else if (!marks.Problem().empty())
        refusal = marks.Problem();
      else
        hash = *checked;
    }

    if (recorder::Bundle const* const into{ open.BundleOf() })
    {
      Result<RunClosed> const closed{ open.Close() };
      if (!closed)
        return oxbox::cli::CliResult::Failed(1, closed.error());
      if (Outcome const told{ Named(*into, opened->Root(), refusal,
                                    closed->problem) }; !told)
        return oxbox::cli::CliResult::Failed(1, told.error());
      Result<std::filesystem::path> const page{
        report::RenderReport(into->Root()) };
      if (!page)
        return oxbox::cli::CliResult::Failed(1, page.error());

      RecordingCounts const film{ closed->recording.value_or(
        RecordingCounts{ }) };
      std::print("bundle {} ({} encoded, {} dropped, {} recorded, {}x{} "
                 "film)\n", into->Root().string(), film.encoded, film.dropped,
                 film.recorded, film.width, film.height);
      if (video_stride > recorder::EVERY_FRAME)
        std::print("video  one frame in {}, no audio\n", video_stride);
      std::print("report {}\n", page->filename().string());
      if (!closed->problem.empty())
        std::print("problem {}\n", closed->problem);
    }

    if (!refusal.empty())
      return oxbox::cli::CliResult::Failed(1, refusal);

    std::print("replayed {} frames of the {} the tape keeps, hash {:016x}, "
               "{}\n", run.Frames(), ending->frames, hash,
               watched->Empty() ? "no watches" : "watches match");
    return { };
  }
}
