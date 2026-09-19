#include "tash/linked/tash.hpp"

#include "_synthetic-target.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace tash::linked
{
  namespace
  {
    using detail::synthetic_target::Frame;
    using detail::synthetic_target::Picture;

    [[nodiscard]] auto Opened(Sink sink) -> Options
    {
      Options options{ };
      options.target = "synthetic";
      options.sink   = sink;
      options.fps    = 60.0;
      return options;
    }

    // What a target pays for the calls when nothing is listening: the test
    // and the jump the header's inline half holds.
    auto SubmittingToNobody(benchmark::State& state) -> void
    {
      std::vector<std::uint16_t> const picture{ Picture() };
      Video const video{ Frame(picture) };
      Harness harness{ Opened(Sink::NONE) };
      for (auto element : state)
      {
        benchmark::DoNotOptimize(element);
        harness.Submit(video);
      }
    }
    BENCHMARK(SubmittingToNobody);

    // The same call while it is being recorded, which is the hash.
    auto SubmittingToARecording(benchmark::State& state) -> void
    {
      std::vector<std::uint16_t> const picture{ Picture() };
      Video const video{ Frame(picture) };
      Harness harness{ Opened(Sink::MEMORY) };
      for (auto element : state)
      {
        benchmark::DoNotOptimize(element);
        harness.Submit(video);
      }
    }
    BENCHMARK(SubmittingToARecording);

    auto BeginningAFrameForNobody(benchmark::State& state) -> void
    {
      Input input{ };
      Harness harness{ Opened(Sink::NONE) };
      for (auto element : state)
      {
        benchmark::DoNotOptimize(element);
        benchmark::DoNotOptimize(harness.Begin(input));
      }
    }
    BENCHMARK(BeginningAFrameForNobody);
  }
}
