#include "tash/python/latest-frame.hpp"

#include "tash/session/session.hpp"
#include "tash/utilities/executable-directory.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace tash::python
{
  namespace
  {
    using session::Session;
    using session::SessionSettings;

    constexpr std::uint64_t STEP_FRAMES{ 300 };

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
  }

  TEST(LatestFrame, NothingIsPublishedBeforeTheFirstFrame)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    LatestFrame latest{ *session, nullptr };
    session->Observe(latest);

    auto const kept{ latest.Kept() };
    ASSERT_FALSE(kept.has_value());
    EXPECT_NE(kept.error().find("no frame yet"), std::string::npos)
      << kept.error();
    EXPECT_FALSE(latest.Observed().has_value());
  }

  TEST(LatestFrame, AReaderTakesWholeFramesWhileAnotherThreadSteps)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    LatestFrame latest{ *session, nullptr };
    session->Observe(latest);
    session->Step(1);

    std::atomic<bool> stepped{ false };
    std::thread stepping{ [&session, &stepped] {
      session->Step(STEP_FRAMES);
      stepped.store(true);
    } };

    std::uint64_t reads{ 0 };
    std::uint64_t last{ 0 };
    while (!stepped.load())
    {
      auto const kept{ latest.Kept() };
      ASSERT_TRUE(kept.has_value()) << (kept ? "" : kept.error());
      ASSERT_EQ(kept->pixels.size(),
                kept->descriptor.pitch * kept->descriptor.height);
      EXPECT_GE(kept->descriptor.number, last);

      auto const seen{ latest.Observed() };
      ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
      EXPECT_EQ(seen->width, kept->descriptor.width);
      EXPECT_GE(seen->frame, kept->descriptor.number + 1u);
      last = kept->descriptor.number;
      ++reads;
    }
    stepping.join();

    EXPECT_GT(reads, 0u);
    auto const kept{ latest.Kept() };
    ASSERT_TRUE(kept.has_value()) << (kept ? "" : kept.error());
    EXPECT_EQ(kept->descriptor.number + 1u, session->Frames());
  }
}
