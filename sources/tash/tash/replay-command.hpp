#pragma once

#include <oxbox/cli/command.hpp>

#include <cstdint>
#include <string>

namespace tash::cli::detail::replay_command
{
  struct ReplayCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(ReplayCommand*);

    std::string bundle;  /* the run bundle to replay: its run.yaml names the profile, its tape.yaml holds the inputs and its trace.bin holds what the run came out as */
    std::string record;  /* record the replay into a new bundle under here, so the clean playthrough has a video, a trace and a report of its own */
    std::string name;    /* the recorded bundle's name; the profile's stem */
    std::uint64_t video_stride{ 1 }; /* encode one frame in this many, at the profile's fps, so a long replay's video is a time-lapse; above 1 the audio is dropped */

    auto operator () () const -> oxbox::cli::CliResult;
  };
}

namespace tash::cli
{
  using detail::replay_command::ReplayCommand;
}
