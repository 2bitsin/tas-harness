#include "tash/tash/serve-command.hpp"

#include "tash/mcp/dispatcher.hpp"
#include "tash/mcp/mcp-server.hpp"
#include "tash/mcp/protocol.hpp"
#include "tash/mcp/tool-context.hpp"
#include "tash/tash/opened-run.hpp"
#include "tash/tash/run-host.hpp"

#include <filesystem>
#include <memory>
#include <print>
#include <string>
#include <utility>

namespace tash::cli::detail::serve_command
{
  using utilities::Result;

  auto ServeCommand::operator () () const -> oxbox::cli::CliResult
  {
    if (video_stride == 0)
      return oxbox::cli::CliResult::UsageError(
        "--video-stride counts frames, so it is at least 1");

    mcp::LaunchAsked opening;
    if (!profile.empty())
      opening.profile = std::filesystem::path{ profile };
    if (!bundle.empty())
      opening.bundle = std::filesystem::path{ bundle };
    if (!name.empty())
      opening.name = name;
    opening.rate = rate;
    opening.video_stride = video_stride;

    RunHost target;
    target.Remember(opening);
    target.KeepCheckpointsUnder(CheckpointsUnder(checkpoints, bundle));
    mcp::ToolContext tools{ target };
    mcp::Dispatcher answering{ tools };

    mcp::McpOptions listening;
    listening.host = host;
    listening.port = port;

    Result<std::unique_ptr<mcp::McpServer>> serving{
      mcp::McpServer::Open(answering, std::move(listening)) };
    if (!serving)
      return oxbox::cli::CliResult::Failed(1, serving.error());

    if (!profile.empty())
    {
      Result<mcp::Launched> const opened{
        target.Launch(mcp::LaunchAsked{ }) };
      if (!opened)
        return oxbox::cli::CliResult::Failed(1, opened.error());
      std::print("core   {} {}\nrom    {}\n", opened->core,
                 opened->core_version, opened->rom);
    }

    (*serving)->Start();
    std::print("mcp    http://{}:{}{}\n", host, (*serving)->Port(),
               mcp::MCP_PATH);
    (*serving)->Run();
    return {};
  }
}
