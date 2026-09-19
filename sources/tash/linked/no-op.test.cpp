#include "tash/linked/tash.hpp"

#include "_synthetic-target.hpp"

#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

// The floor the header is written to; this build compiles at 26 and a
// target may compile at either.
static_assert(__cplusplus >= 202302L);

namespace tash::linked
{
  namespace
  {
    using detail::synthetic_target::Guest;
    using detail::synthetic_target::Picture;
    using detail::synthetic_target::Play;

    [[nodiscard]] auto Asked(Sink sink, std::vector<std::string>& notes,
                             std::uint32_t api = API_VERSION) -> Options
    {
      Options options{ };
      options.target = "synthetic";
      options.sink   = sink;
      options.fps    = 60.0;
      options.note   = [&notes](std::string_view text)
                       { notes.emplace_back(text); };
      options.api    = api;
      return options;
    }

    auto Cleared(std::string_view name) -> void
    {
      ASSERT_EQ(unsetenv(std::string{ name }.c_str()), 0);
    }
  }

  TEST(NoHarness, NothingAskedForIsNothingOpened)
  {
    std::vector<std::string> notes;
    Harness harness{ Asked(Sink::NONE, notes) };
    EXPECT_EQ(harness.Mode(), Mode::NONE);
    EXPECT_FALSE(harness.Attached());
    EXPECT_TRUE(notes.empty());
  }

  TEST(NoHarness, EveryCallAnswersWhatTheTargetBroughtToIt)
  {
    std::vector<std::string> notes;
    Harness harness{ Asked(Sink::NONE, notes) };

    Input input{ };
    EXPECT_FALSE(harness.Begin(input));
    EXPECT_EQ(harness.Dt(0.25), 0.25);
    EXPECT_EQ(harness.Seed(77u), 77u);
    EXPECT_FALSE(harness.Register("room", &input, Number::U16).has_value());
    EXPECT_TRUE(harness.Bytes(Artifact::TAPE).empty());

    // The whole of a run against a library that is not there.
    std::vector<std::uint16_t> const picture{ Picture() };
    Guest guest{ };
    Play(harness, guest, picture);
    EXPECT_TRUE(notes.empty());
  }

  TEST(NoHarness, AnEnvironmentThatNamesNothingRecordsNothing)
  {
    std::vector<std::string> notes;
    Cleared(RECORD_VARIABLE);
    Cleared(SESSION_VARIABLE);
    Harness harness{ Asked(Sink::ENVIRONMENT, notes) };
    EXPECT_EQ(harness.Mode(), Mode::NONE);
    EXPECT_TRUE(notes.empty());
  }

  TEST(NoHarness, TheVariableIsWhatTurnsARecordingOn)
  {
    auto const scratch{ utilities::ScratchAreaOf("linked-environment") };
    ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
    std::filesystem::path const root{ scratch->File("run") };
    ASSERT_EQ(setenv(std::string{ RECORD_VARIABLE }.c_str(), root.c_str(), 1),
              0);

    std::vector<std::string> notes;
    {
      Harness harness{ Asked(Sink::ENVIRONMENT, notes) };
      EXPECT_EQ(harness.Mode(), Mode::FILE);
    }
    Cleared(RECORD_VARIABLE);
    EXPECT_TRUE(std::filesystem::exists(root / "tape.yaml"));
    EXPECT_TRUE(notes.empty());
  }

  TEST(NoHarness, ATargetBuiltAgainstAnotherApiIsToldSo)
  {
    std::vector<std::string> notes;
    Harness harness{ Asked(Sink::MEMORY, notes, API_VERSION + 1u) };
    EXPECT_EQ(harness.Mode(), Mode::NONE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_NE(notes.front().find("api"), std::string::npos);
  }

  TEST(NoHarness, AHarnessNamedButNotBuiltInIsSaidOutLoud)
  {
    ASSERT_EQ(setenv(std::string{ SESSION_VARIABLE }.c_str(),
                     "/run/tash.sock", 1), 0);

    std::vector<std::string> notes;
    {
      Harness harness{ Asked(Sink::ENVIRONMENT, notes) };
      EXPECT_EQ(harness.Mode(), Mode::NONE);
    }
    Cleared(SESSION_VARIABLE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_NE(notes.front().find("attached"), std::string::npos);
  }
}
