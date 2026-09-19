#pragma once

#include <oxbox/cli/command.hpp>

#include <cstdint>
#include <string>

namespace tash::cli::detail::run_command
{
  struct RunCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(RunCommand*);

    std::string profile;          /* the run profile: a yaml file naming the core, the ROM, the directories and the device on each port, or the directory holding its profile.yaml */
    std::uint64_t frames{ 600 };  /* how many frames to run */
    double rate{ 0.0 };           /* pace the run at this multiple of real time; 0, the default, runs as fast as the core does */
    std::string tape;             /* play this tape from the run's start, then run on for --frames frames */
    std::string scenario;         /* drive the run from this python file instead of counting frames */
    std::string steps;            /* play this script instead of running straight: run <frames> | hold <button,...> | release, separated by semicolons */
    std::string shot;             /* write the last frame to this .png */
    std::string shot_region;      /* crop the shot to `x,y,width,height`, which is how an anchor crop is cut */
    std::string trace;            /* one frame record per frame, here */
    std::string bundle;           /* record the run into a bundle under here */
    std::uint64_t video_stride{ 1 }; /* encode one frame in this many, at the profile's fps, so a long run's video is a time-lapse; above 1 the audio is dropped */
    std::string name;             /* the bundle's name; the profile's stem */
    std::string checkpoints;      /* cache the scenario's named checkpoints under here, rather than in `_checkpoints` beside the bundles */

    auto operator () () const -> oxbox::cli::CliResult;
  };
}

namespace tash::cli
{
  using detail::run_command::RunCommand;
}
