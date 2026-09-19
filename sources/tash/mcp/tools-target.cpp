#include "tash/mcp/tool-arguments.hpp"
#include "tash/mcp/tool-table.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tash::mcp::detail::tool_table
{
  using content::Failed;
  using content::Said;
  using content::ToolOutcome;
  using tool::Entry;
  using tool_context::ToolContext;
  using utilities::Result;

  namespace
  {
    using namespace tool_arguments;

    auto Launch(ToolContext& tools, LaunchArgs const& asked)
      -> Result<ToolOutcome>
    {
      session_host::LaunchAsked opening;
      if (asked.profile)
        opening.profile = std::filesystem::path{ *asked.profile };
      if (asked.bundle)
        opening.bundle = std::filesystem::path{ *asked.bundle };
      opening.name = asked.name;
      opening.rate = asked.rate;

      Result<session_host::Launched> const opened{
        tools.Host().Launch(opening) };
      if (!opened)
        return Failed(opened.error());

      return Said(std::format(
        "core   {} {}\nrom    {}\nhash   {}\nfps    {:.4f}\nbundle {}",
        opened->core, opened->core_version, opened->rom, opened->rom_hash,
        opened->fps,
        opened->bundle ? opened->bundle->string() : std::string{ "none" }));
    }

    // A status answers with the end of a long job's printing, not with all
    // of it: python_output is where the whole of it is.
    inline constexpr std::size_t STATUS_TAIL{ 1024 };
    inline constexpr std::size_t WHOLE_OUTPUT{ 0 };

    auto Shutdown(ToolContext& tools, NoArguments const&)
      -> Result<ToolOutcome>
    {
      Result<std::string> const closed{ tools.Host().Shutdown() };
      return closed ? Said(*closed) : Failed(closed.error());
    }

    auto Report(ToolContext& tools, NoArguments const&) -> Result<ToolOutcome>
    {
      Result<std::string> const rendered{ tools.Host().Report() };
      return rendered ? Said(*rendered) : Failed(rendered.error());
    }

    auto Tape(ToolContext& tools, NoArguments const&) -> Result<ToolOutcome>
    {
      Result<std::string> const written{ tools.Host().Tape() };
      return written ? Said(*written) : Failed(written.error());
    }

    // The budget exists because a synchronous call holds the thread the
    // server answers on; a detached job holds nothing, so it defaults to
    // none and python_status and python_cancel are what reach it.
    [[nodiscard]] auto BudgetOf(std::optional<std::int64_t> asked,
                                bool detach) -> std::uint64_t
    {
      if (asked && *asked >= 0)
        return static_cast<std::uint64_t>(*asked);
      return detach ? python::UNLIMITED : session_host::DEFAULT_FRAME_BUDGET;
    }

    // The budget in force, named in every answer a python job has: a job
    // that dies on one nobody chose otherwise reads as the source's fault.
    [[nodiscard]] auto BudgetWording(std::uint64_t budget) -> std::string
    {
      if (budget == python::UNLIMITED)
        return "no frame budget";
      return std::format("a {}-frame budget", budget);
    }

    // The frames a job has made, against the budget it began under.
    [[nodiscard]] auto RanWording(session_host::JobReport const& job)
      -> std::string
    {
      if (job.budget == python::UNLIMITED)
        return std::format("{} frames under no budget", job.frames);
      return std::format("{} frames of its {}-frame budget", job.frames,
                         job.budget);
    }

    // stderr comes back beside stdout rather than in it, marked so a
    // reader knows which stream wrote which line.
    inline constexpr std::string_view STDERR_MARK{ "stderr:\n" };

    [[nodiscard]] auto Printing(session_host::JobReport const& job)
      -> std::string
    {
      if (job.errored.empty())
        return job.printed;
      return std::format("{}{}{}", job.printed, STDERR_MARK, job.errored);
    }

    [[nodiscard]] auto EndedWith(std::string said, std::string_view line)
      -> std::string
    {
      if (!said.empty() && said.back() != '\n')
        said += '\n';
      said += line;
      return said;
    }

    auto Python(ToolContext& tools, PythonArgs const& asked)
      -> Result<ToolOutcome>
    {
      if (Result<python::ScenarioRun*> const live{ tools.Live() }; !live)
        return Failed(live.error());
      bool const detach{ asked.detach.value_or(false) };
      std::uint64_t const budget{ BudgetOf(asked.frame_budget, detach) };
      if (detach)
      {
        Result<std::string> const job{
          tools.Host().Detach(asked.source, budget) };
        return job ? Said(std::format(
                       "{} is running under {}; python_status, and "
                       "python_cancel stops it", *job, BudgetWording(budget)))
                   : Failed(job.error());
      }
      Result<session_host::JobReport> const done{
        tools.Host().Evaluate(asked.source, budget) };
      if (!done)
        return Failed(done.error());
      std::string const ran{
        std::format("{} ran {}", done->name, RanWording(*done)) };
      if (!done->refusal.empty())
        return Failed(EndedWith(Printing(*done) + done->refusal, ran));
      return tools.Moved(EndedWith(Printing(*done) + done->answered, ran));
    }

    auto Cancel(ToolContext& tools, NoArguments const&) -> Result<ToolOutcome>
    {
      Result<std::string> const stopping{ tools.Host().Cancel() };
      return stopping ? Said(*stopping) : Failed(stopping.error());
    }

    auto PythonFile(ToolContext& tools, PythonFileArgs const& asked)
      -> Result<ToolOutcome>
    {
      std::ifstream reading{ asked.path, std::ios::binary };
      if (!reading)
        return Failed(std::format("mcp: no python file at {}", asked.path));
      std::string const source{ std::istreambuf_iterator<char>{ reading },
                                std::istreambuf_iterator<char>{ } };
      return Python(tools,
                    PythonArgs{ source, asked.detach, asked.frame_budget });
    }

    [[nodiscard]] auto Standing(session_host::JobReport const& job)
      -> std::string_view
    {
      if (job.running)
        return "is running";
      return job.refusal.empty() ? "finished" : "failed";
    }

    auto PythonStatus(ToolContext& tools, NoArguments const&)
      -> Result<ToolOutcome>
    {
      Result<session_host::JobReport> const job{
        tools.Host().JobOf(STATUS_TAIL) };
      if (!job)
        return Failed(job.error());
      std::string said{ std::format(
        "{} {} after {}; {}", job->name, Standing(*job), RanWording(*job),
        job->running ? "python_cancel stops it" : "python_output") };
      if (std::string const printing{ Printing(*job) }; !printing.empty())
        said += std::format("\n{}", printing);
      return Said(std::move(said));
    }

    auto PythonOutput(ToolContext& tools, NoArguments const&)
      -> Result<ToolOutcome>
    {
      Result<session_host::JobReport> const job{
        tools.Host().JobOf(WHOLE_OUTPUT) };
      if (!job)
        return Failed(job.error());
      if (!job->refusal.empty())
        return Failed(EndedWith(
          Printing(*job) + job->refusal,
          std::format("{} ran {}", job->name, RanWording(*job))));
      if (job->running)
        return Said(Printing(*job));
      // Finished, so the run is the caller's again: the observation a
      // foreground call would have ended on belongs here.
      return tools.Moved(EndedWith(
        Printing(*job) + job->answered,
        std::format("{} ran {}", job->name, RanWording(*job))));
    }
  }

  auto TargetTools() -> std::vector<tool::Tool>
  {
    return {
      Entry<LaunchArgs, &Launch>(
        "launch", "Open a run profile: the core, the ROM and the bundle. Without one, the profile the server was started with."),
      Entry<NoArguments, &Shutdown>(
        "shutdown", "Close the run and finish what it was recording."),
      Entry<PythonArgs, &Python>(
        "python", "Evaluate python against the live run; what it defines "
                  "stays defined. Waited for, it runs under a frame budget "
                  "of 360000 frames; detached, under none. `frame_budget` "
                  "names another and 0 lifts it; past it the next step, "
                  "run_until or play raises tash.BudgetExceeded."),
      Entry<PythonFileArgs, &PythonFile>(
        "python_file", "Evaluate a python file the same way: `path` is the "
                       "file and `frame_budget` the frames it may run, "
                       "360000 waited for and none detached. From a shell: "
                       "`tash session python_file --port <n> -- --path "
                       "player.py --detach true`."),
      Entry<NoArguments, &Cancel>(
        "python_cancel", "Stop the detached python job at its next step, "
                         "run_until or play."),
      Entry<NoArguments, &PythonStatus>(
        "python_status", "Say whether the detached python job is still "
                         "running, the frames it has made against its "
                         "budget, and the tail of what it printed on stdout "
                         "and stderr."),
      Entry<NoArguments, &PythonOutput>(
        "python_output", "Everything the python job printed on stdout and "
                         "stderr, and once it is over what it answered."),
      Entry<NoArguments, &Report>(
        "report", "Render the run's report.html and answer where it is."),
      Entry<NoArguments, &Tape>(
        "tape", "Write the run's tape.yaml as the line stands, folded to "
                "this frame the way a restore folds it, and the manifest a "
                "replay reads the profile from; answer its path and the "
                "frames it holds. `tash tape replay --bundle <the run's "
                "bundle> --record <dir>` then films the line so far, the run "
                "goes on, and closing it writes the tape again.")
    };
  }
}
