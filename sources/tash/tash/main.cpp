#include "tash/tash/tash-cli.hpp"

#include <oxbox/cli/console.hpp>
#include <oxbox/cli/main.hpp>

auto main(int argc, char* argv[]) -> int
{
  oxbox::cli::PrepareConsole();
  return oxbox::cli::Main<tash::cli::TashCli>(argc, argv);
}
