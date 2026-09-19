#include "tash/tash/python-job.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

namespace
{
  using tash::cli::PythonJob;

  constexpr std::size_t TAIL{ 16 };
  constexpr auto BEAT{ std::chrono::milliseconds{ 2 } };

  auto Polled(PythonJob const& job) -> void
  {
    while (job.Running())
      std::this_thread::sleep_for(BEAT);
  }
}

TEST(PythonJob, NothingRunsBeforeTheFirstStart)
{
  PythonJob job;
  EXPECT_FALSE(job.Running());
  EXPECT_FALSE(job.Started());
  EXPECT_EQ(job.Name(), "");
}

TEST(PythonJob, AStartedJobRunsUntilItsWorkAnswers)
{
  PythonJob job;
  std::atomic<bool> let{ false };
  ASSERT_TRUE(job.Start("python-1", [&job, &let]
  {
    job.Print("half way\n");
    while (!let.load())
      std::this_thread::sleep_for(BEAT);
    return std::string{ "done" };
  }).has_value());

  EXPECT_TRUE(job.Started());
  EXPECT_EQ(job.Name(), "python-1");

  // The printing is readable from this thread while the work still holds
  // the other one, which is the whole point of the job.
  while (job.Printed(0).empty())
    std::this_thread::sleep_for(BEAT);
  EXPECT_EQ(job.Printed(0), "half way\n");
  EXPECT_TRUE(job.Running());
  EXPECT_EQ(*job.Answer(), "");

  let.store(true);
  Polled(job);
  EXPECT_FALSE(job.Running());
  EXPECT_EQ(*job.Answer(), "done");
  job.Wait();
  EXPECT_FALSE(job.Started());
}

TEST(PythonJob, ASecondJobWaitsForTheFirstToBeReaped)
{
  PythonJob job;
  ASSERT_TRUE(job.Start("python-1", [] { return std::string{ "one" }; })
                .has_value());
  Polled(job);

  auto const again{ job.Start("python-2",
                              [] { return std::string{ "two" }; }) };
  ASSERT_FALSE(again.has_value());
  EXPECT_NE(again.error().find("python-1"), std::string::npos)
    << again.error();

  job.Wait();
  ASSERT_TRUE(job.Start("python-2", [] { return std::string{ "two" }; })
                .has_value());
  Polled(job);
  EXPECT_EQ(job.Name(), "python-2");
  EXPECT_EQ(*job.Answer(), "two");
}

TEST(PythonJob, AFailedJobAnswersItsRefusal)
{
  PythonJob job;
  ASSERT_TRUE(job.Start("python-1", [&job]
  {
    job.Print("before it went wrong\n");
    return tash::utilities::Refused("python: it went wrong");
  }).has_value());
  Polled(job);

  auto const answered{ job.Answer() };
  ASSERT_FALSE(answered.has_value());
  EXPECT_EQ(answered.error(), "python: it went wrong");
  EXPECT_EQ(job.Printed(0), "before it went wrong\n");
}

TEST(PythonJob, StderrIsKeptBesideStdoutAndNotInIt)
{
  PythonJob job;
  ASSERT_TRUE(job.Start("python-1", [&job]
  {
    job.Print("out\n");
    job.PrintError("err\n");
    return std::string{ "done" };
  }).has_value());
  Polled(job);

  EXPECT_EQ(job.Printed(0), "out\n");
  EXPECT_EQ(job.Errored(0), "err\n");

  job.Wait();
  ASSERT_TRUE(job.Start("python-2", [] { return std::string{ "two" }; })
                .has_value());
  Polled(job);
  EXPECT_EQ(job.Errored(0), "");
}

TEST(PythonJob, ATailStartsAtALineAndNeverInsideACharacter)
{
  PythonJob job;
  job.Print("one\ntwo\nthree\nfour\nfive\n");
  EXPECT_EQ(job.Printed(0), "one\ntwo\nthree\nfour\nfive\n");
  EXPECT_EQ(job.Printed(TAIL), "three\nfour\nfive\n");

  PythonJob wide;
  wide.Print("ääääääääää");
  std::string const tail{ wide.Printed(5) };
  EXPECT_EQ(tail, "ää");

  PythonJob erring;
  erring.PrintError("one\ntwo\nthree\nfour\nfive\n");
  EXPECT_EQ(erring.Errored(TAIL), "three\nfour\nfive\n");
}
