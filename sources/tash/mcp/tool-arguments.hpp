#pragma once
// Every tool's arguments, one struct each: the member's type is the schema's
// type, a member that may be absent is an optional one, and the comment to
// its right is the description tools/list prints.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tash::mcp::detail::tool_arguments
{
  // A tool that takes nothing declares nothing: a scheme is deduced from
  // fields, and a type with none has no scheme to deduce.
  struct NoArguments
  {
  };

  struct LaunchArgs
  {
    friend constexpr auto reflect_scheme(LaunchArgs*);

    std::optional<std::string>  profile; /* the run profile to open;
                                            the server's own by default */
    std::optional<std::string>  bundle;  /* record the run under here */
    std::optional<std::string>  name;    /* what to call the bundle */
    std::optional<double>       rate;    /* pace, as a multiple of real
                                            time; 0 runs flat out */
  };

  struct StepArgs
  {
    friend constexpr auto reflect_scheme(StepArgs*);

    std::optional<std::int64_t> frames;  /* frames to run; one by default */
  };

  struct RunUntilArgs
  {
    friend constexpr auto reflect_scheme(RunUntilArgs*);

    std::string                 predicate; /* an anchor predicate, as the
                                              anchor tool spells it */
    std::optional<std::int64_t> timeout_frames; /* give up after this many */
  };

  struct PaceArgs
  {
    friend constexpr auto reflect_scheme(PaceArgs*);

    double                      rate;    /* multiple of real time; 0 runs as
                                            fast as the core does */
  };

  struct LookArgs
  {
    friend constexpr auto reflect_scheme(LookArgs*);

    std::optional<std::string>  region;  /* crop to `x,y,width,height` */
  };

  struct ActArgs
  {
    friend constexpr auto reflect_scheme(ActArgs*);

    std::optional<std::int64_t>             port;    /* pad 1 or 2; 1 by
                                                        default */
    std::optional<std::vector<std::string>> hold;    /* buttons to press and
                                                        leave held */
    std::optional<std::vector<std::string>> release; /* buttons to let go of;
                                                        all when empty */
    std::optional<std::vector<std::string>> tap;     /* buttons held for the
                                                        frames below */
    std::optional<std::int64_t>             frames;  /* frames to run after
                                                        the pad changed */
  };

  struct AnchorArgs
  {
    friend constexpr auto reflect_scheme(AnchorArgs*);

    std::string                 predicate; /* `watch <name> <how> <value>`,
                                              `exact_hash <hex>`,
                                              `difference_hash <hex> within
                                              <n>`, `perceptual_hash <hex>
                                              within <n>` or `template_image
                                              <png> at <score>` */
  };

  struct PeekArgs
  {
    friend constexpr auto reflect_scheme(PeekArgs*);

    std::string                 address; /* where to read, `0xc800` or a
                                            decimal, as a profile writes it */
    std::int64_t                count;   /* how many bytes to read */
    std::optional<std::string>  region;  /* the memory region to read; the
                                            system ram by default */
  };

  struct NamedArgs
  {
    friend constexpr auto reflect_scheme(NamedArgs*);

    std::string                 name;    /* what it is called */
  };

  struct MarkArgs
  {
    friend constexpr auto reflect_scheme(MarkArgs*);

    std::string                 name;    /* what the mark is called */
    std::optional<std::string>  group;   /* the group the report collapses
                                            it into */
  };

  struct RestoreOrPlayArgs
  {
    friend constexpr auto reflect_scheme(RestoreOrPlayArgs*);

    std::string                 name;    /* the checkpoint to look for */
    std::string                 tape;    /* the tape to play when there is
                                            none, before checkpointing */
  };

  struct ExpectArgs
  {
    friend constexpr auto reflect_scheme(ExpectArgs*);

    std::string                 name;      /* what is being expected */
    std::string                 predicate; /* an anchor predicate, judged
                                              against the latest frame */
    std::optional<std::string>  text;      /* what to record beside it */
  };

  struct JudgeArgs
  {
    friend constexpr auto reflect_scheme(JudgeArgs*);

    std::string                 name;    /* the question answered */
    bool                        passed;  /* the caller's own answer */
    std::optional<std::string>  text;    /* the evidence for it */
  };

  struct ShotArgs
  {
    friend constexpr auto reflect_scheme(ShotArgs*);

    std::optional<std::string>  region;  /* crop to `x,y,width,height` */
  };

  struct ClipArgs
  {
    friend constexpr auto reflect_scheme(ClipArgs*);

    std::string                 label;      /* what the window is called */
    std::optional<std::string>  from_mark;  /* start at this mark */
    std::optional<std::int64_t> from_frame; /* start at this frame instead */
  };

  struct PythonArgs
  {
    friend constexpr auto reflect_scheme(PythonArgs*);

    std::string                 source;  /* evaluated with `tash.run` set */
    std::optional<bool>         detach;  /* answer at once with a job name
                                            and keep running; watch it with
                                            python_status */
    std::optional<std::int64_t> frame_budget; /* frames this may run before
                                                 it raises tash.BudgetExceeded;
                                                 0 lifts the limit */
  };

  struct PythonFileArgs
  {
    friend constexpr auto reflect_scheme(PythonFileArgs*);

    std::string                 path;    /* a file to evaluate the same way */
    std::optional<bool>         detach;  /* answer at once with a job name
                                            and keep running */
    std::optional<std::int64_t> frame_budget; /* frames this may run before
                                                 it raises tash.BudgetExceeded;
                                                 0 lifts the limit */
  };
}
