#include "tash/python/state-search.hpp"

#include "tash/session/frame-observer.hpp"
#include "tash/session/line.hpp"
#include "tash/session/session.hpp"
#include "tash/utilities/executable-directory.hpp"

#include <gtest/gtest.h>

#include <pybind11/embed.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace tash::python
{
  namespace
  {
    namespace py = pybind11;

    using session::Session;
    using session::SessionSettings;

    constexpr std::uint64_t SEED_FRAMES{ 30 };
    constexpr std::uint64_t TRIAL_FRAMES{ 5 };
    constexpr std::uint64_t CANDIDATES{ 3 };

    auto Root() -> std::filesystem::path
    {
      std::filesystem::path here{ std::filesystem::current_path() };
      while (!std::filesystem::exists(here / "buildutil.toml")
             && here.has_relative_path())
        here = here.parent_path();
      return here;
    }

    auto Opened() -> std::unique_ptr<Session>
    {
      auto const beside{ utilities::ExecutableDirectory() };
      EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
      SessionSettings settings;
      settings.core = *beside / "genesis_plus_gx_libretro.so";
      settings.rom = Root() / "examples/homebrew/zsenilia.bin";
      auto session{ Session::Open(std::move(settings)) };
      EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
      return session ? std::move(*session) : nullptr;
    }

    // A line only a place minted at those frames folds, which is what the
    // recording's tokens buy; anything else leaves the run adrift.
    class LineKept final : public session::LineSource,
                           public session::FrameObserver
    {
    public:
      auto OnFrame(bus::FrameView const&, std::int64_t) -> void override
      { ++_frames; }

      auto OnRewind(session::Rewind const& back) -> void override
      {
        if (back.place && back.place->frames <= _frames
            && back.place->token == back.place->frames + 1)
          _frames = back.place->frames;
        else
          _adrift = true;
      }

      auto LineUpTo(std::uint64_t) const
        -> std::optional<session::Line> override
      {
        if (_adrift)
          return std::nullopt;
        return session::Line{ _frames, { }, { } };
      }

      auto Place() const -> session::LinePlace override
      { return session::LinePlace{ _frames, _frames + 1 }; }

    private:
      std::uint64_t _frames{ 0 };
      bool          _adrift{ false };
    };

    struct Searched
    {
      std::unique_ptr<Session> session{ };
      LineKept                 line{ };
      RunParts                 parts{ };
      std::optional<ScenarioRun> run{ };

      auto Seeded() -> bool
      {
        session = Opened();
        if (session == nullptr)
          return false;
        session->Observe(line);
        parts.session = session.get();
        parts.line = &line;
        run.emplace(parts);
        run->Live().Step(SEED_FRAMES);
        return true;
      }
    };

    auto Candidates() -> py::list
    {
      py::list held;
      for (std::uint64_t which{ 0 }; which < CANDIDATES; ++which)
        held.append(py::int_{ which });
      return held;
    }

    // The frames a trial spends, so a search that failed to rewind shows in
    // the line as well as in the counter.
    auto Stepping(ScenarioRun& run) -> py::cpp_function
    {
      return py::cpp_function{ [&run](py::object const&, py::object const&)
        { run.Live().Step(TRIAL_FRAMES); } };
    }

    auto Scoring() -> py::cpp_function
    {
      return py::cpp_function{
        [](py::object const&) { return py::int_{ 1 }; } };
    }

    auto FramesOn(ScenarioRun const& run) -> std::optional<std::uint64_t>
    {
      auto const line{ run.LineFrames() };
      EXPECT_TRUE(line.has_value()) << (line ? "" : line.error());
      return line ? *line : std::nullopt;
    }
  }

  TEST(StateSearch, ASearchLeavesTheRunOnTheLineItSeeded)
  {
    py::scoped_interpreter const interpreter{ };
    Searched held;
    ASSERT_TRUE(held.Seeded());
    ScenarioRun& run{ *held.run };
    ASSERT_EQ(FramesOn(run), SEED_FRAMES);

    StateSearch searching{ run, py::none(), Stepping(run), Scoring(),
                           py::none() };
    SearchResult const answer{ searching.Best(Candidates(), 1) };
    EXPECT_EQ(answer.trials, CANDIDATES);

    EXPECT_EQ(FramesOn(run), SEED_FRAMES) << "the line the search seeded on";

    // A checkpoint taken after a search carries the line, so a restore to it
    // folds instead of casting the run adrift.
    auto const kept{ run.Checkpoint("after", CheckpointKind::SCRATCH) };
    ASSERT_TRUE(kept.has_value()) << (kept ? "" : kept.error());
    run.Live().Step(TRIAL_FRAMES);
    ASSERT_TRUE(run.Restore("after").has_value());
    EXPECT_EQ(FramesOn(run), SEED_FRAMES);
  }

  TEST(StateSearch, ASearchInsideAnothersScoreComesBackToTheOuterSeed)
  {
    py::scoped_interpreter const interpreter{ };
    Searched held;
    ASSERT_TRUE(held.Seeded());
    ScenarioRun& run{ *held.run };

    std::uint64_t inner{ 0 };
    py::cpp_function const nested{ [&run, &inner](py::object const&)
    {
      StateSearch deeper{ run, py::none(), Stepping(run), Scoring(),
                          py::none() };
      inner += deeper.Best(Candidates(), 1).trials;
      return py::int_{ 1 };
    } };

    StateSearch searching{ run, py::none(), Stepping(run), nested,
                           py::none() };
    SearchResult const answer{ searching.Best(Candidates(), 1) };
    EXPECT_EQ(answer.trials, CANDIDATES);
    EXPECT_EQ(inner, CANDIDATES * CANDIDATES);
    EXPECT_EQ(FramesOn(run), SEED_FRAMES) << "the outer seed, not the inner";
  }

  TEST(StateSearch, TheRestoredCallbackFollowsEveryRestoreToTheSeed)
  {
    py::scoped_interpreter const interpreter{ };
    Searched held;
    ASSERT_TRUE(held.Seeded());
    ScenarioRun& run{ *held.run };
    // The search only hands the run's handle on, so any object stands in
    // for `tash.run` here.
    py::object const handle{ py::dict{ } };

    std::uint64_t told{ 0 };
    std::uint64_t standing{ 0 };
    py::cpp_function const dropped{ [&run, &told, &standing, &handle]
      (py::object const& given)
    {
      ++told;
      EXPECT_TRUE(given.is(handle));
      if (FramesOn(run) != SEED_FRAMES)
        ++standing;
    } };

    StateSearch searching{ run, handle, Stepping(run), Scoring(), dropped };
    static_cast<void>(searching.Best(Candidates(), 1));

    // One before each trial and one when the search hands the run back.
    EXPECT_EQ(told, CANDIDATES + 1u);
    EXPECT_EQ(standing, 0u) << "a callback told before the run was rewound";
  }
}
