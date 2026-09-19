#pragma once

#include <oxbox/cli/command.hpp>

#include <cstdint>
#include <string>

namespace tash::cli::detail::serve_command
{
  struct ServeCommand : oxbox::cli::Command
  {
    friend constexpr auto reflect_scheme(ServeCommand*);

    std::string profile;          /* the run profile to open before the first tool call, a yaml file or the directory holding its profile.yaml; without it the agent calls the launch tool itself */
    std::uint16_t port{ 0 };      /* listen here; 0 asks the host for a free port and prints it */
    std::string host{ "127.0.0.1" }; /* the interface to bind; the loopback, because this endpoint runs a core and an interpreter */
    std::string bundle;           /* record the session into a bundle under here */
    std::string name;             /* the bundle's name; the profile's directory */
    double rate{ 0.0 };           /* pace the run at this multiple of real time; 0 runs as fast as the core does */
    std::uint64_t video_stride{ 1 }; /* encode one frame in this many, at the profile's fps, so a long session's video is a time-lapse; above 1 the audio is dropped */
    std::string checkpoints;      /* where named save states are cached between runs; beside the bundle, or here */

    auto operator () () const -> oxbox::cli::CliResult;
  };
}

namespace tash::cli
{
  using detail::serve_command::ServeCommand;
}
