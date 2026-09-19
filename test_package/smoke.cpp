// A consumer of libtash alone: the shipped tash/linked header, the shipped
// library, and no other component of the package on the link line.

#include <tash/linked/tash.hpp>

#include <cstdio>

auto main() -> int
{
  tash::linked::Harness harness{ tash::linked::Options{
    .target = "smoke", .sink = tash::linked::Sink::NONE, .fps = 60.0 } };
  if (harness.Mode() != tash::linked::Mode::NONE || harness.Attached())
    return 1;

  tash::linked::Input input{ };
  if (harness.Begin(input) || harness.Dt(0.5) != 0.5
      || harness.Seed(9u) != 9u)
    return 1;

  std::printf("tash::linked answers with no harness attached\n");
  return 0;
}
