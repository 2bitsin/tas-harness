#include "tash/python/sampler-watches.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/verdicts-writer.hpp"
#include "tash/session/rom-file.hpp"
#include "tash/session/session.hpp"
#include "tash/tash/profile.hpp"
#include "tash/tash/scenario-runner.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/scratch-area.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/sampler.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
  // roms/ is the machine's own cartridge library, which the profile names too.
  constexpr auto COLUMNS = "roms/Genesis/Columns (USA, Europe).zip";

  // Three expects, the judge the scenario ends on, and the restore probe
  // the checkpoint after the tape pays for.
  constexpr std::uint64_t SCENARIO_VERDICTS{ 5 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Columns() -> std::filesystem::path
  {
    return Root() / COLUMNS;
  }

  struct Judged
  {
    bool          played{ false };
    std::uint64_t verdicts{ 0 };
    std::uint64_t failed{ 0 };
    std::uint64_t marks{ 0 };
  };

  // The run the cli would build for --scenario --bundle, torn down before
  // the trace is read, because a second libretro core cannot live in this
  // process beside the first.
  auto Play(std::filesystem::path const& trace,
            tash::recorder::Bundle const& bundle) -> bool
  {
    auto const profile{ tash::cli::ProfileFrom(
      Root() / "examples/columns/profile.yaml") };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    if (!profile)
      return false;

    auto settings{ tash::cli::SettingsFrom(*profile) };
    EXPECT_TRUE(settings.has_value()) << (settings ? "" : settings.error());
    if (!settings)
      return false;
    // The profile's rom is relative to where `tash run` is invoked, which
    // is the repository root; a test binary runs from its build tree.
    settings->rom = Columns();

    auto opened{ tash::session::Session::Open(std::move(*settings)) };
    EXPECT_TRUE(opened.has_value()) << (opened ? "" : opened.error());
    if (!opened)
      return false;
    std::unique_ptr<tash::session::Session> const session{
      std::move(*opened) };

    auto writer{ tash::trace::Writer::Open(trace, "tash-test") };
    EXPECT_TRUE(writer.has_value()) << (writer ? "" : writer.error());
    if (!writer)
      return false;

    auto verdicts{ tash::recorder::VerdictsWriter::Open(bundle.Verdicts()) };
    EXPECT_TRUE(verdicts.has_value()) << (verdicts ? "" : verdicts.error());
    if (!verdicts)
      return false;

    auto const watched{ tash::cli::WatchesFrom(*profile) };
    EXPECT_TRUE(watched.has_value()) << (watched ? "" : watched.error());
    if (!watched)
      return false;

    auto opened_sampler{ tash::watches::Sampler::Open(
      *watched, tash::watches::MemoryMap::Of(*session), &*writer) };
    EXPECT_TRUE(opened_sampler.has_value())
      << (opened_sampler ? "" : opened_sampler.error());
    if (!opened_sampler)
      return false;
    std::unique_ptr<tash::watches::Sampler> const sampler{
      std::move(*opened_sampler) };
    session->Observe(*sampler);

    tash::python::SamplerWatches const asked{ *sampler };
    tash::python::RunParts parts;
    parts.session = session.get();
    parts.trace = &*writer;
    parts.bundle = &bundle;
    parts.verdicts = &*verdicts;
    parts.watches = &asked;

    tash::python::ScenarioRun driven{ parts };
    tash::cli::ScenarioRunner runner;
    auto const played{ runner.Play(
      driven, Root() / "examples/columns/scenario.py") };
    EXPECT_TRUE(played.has_value()) << (played ? "" : played.error());
    EXPECT_TRUE(writer->Flush().has_value());
    return played.has_value();
  }

  auto JudgedIn(std::filesystem::path const& trace) -> Judged
  {
    Judged seen{ };
    auto reader{ tash::trace::Reader::Open(trace) };
    EXPECT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    if (!reader)
      return seen;

    reader->ForEach([&seen](auto const& record) {
      using Record = std::decay_t<decltype(record)>;
      if constexpr (std::is_same_v<Record, tash::trace::VerdictRecord>)
      {
        ++seen.verdicts;
        seen.failed += record.passed ? 0u : 1u;
      }
      else if constexpr (std::is_same_v<Record, tash::trace::MarkRecord>)
        ++seen.marks;
    });
    return seen;
  }
}

TEST(ColumnsScenario, EveryVerdictLandsAndNoneFails)
{
  if (!tash::session::CartridgePresent(Columns()))
    GTEST_SKIP() << "no cartridge at " << Columns();

  auto const scratch{ tash::utilities::ScratchAreaOf("columns-scenario") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const trace{ scratch->File("scenario.tash") };

  auto const bundle{ tash::recorder::Bundle::Create(scratch->Path(),
                                                    "columns") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());

  ASSERT_TRUE(Play(trace, *bundle));

  Judged const seen{ JudgedIn(trace) };
  EXPECT_EQ(seen.verdicts, SCENARIO_VERDICTS);
  EXPECT_EQ(seen.failed, 0u);
  EXPECT_GE(seen.marks, 1u);
  EXPECT_TRUE(std::filesystem::exists(bundle->Shots() / "shot-0001.png"));
}
