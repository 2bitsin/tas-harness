#include "tash/python/scenario-run.hpp"

#include "tash/perception/colour.hpp"
#include "tash/perception/region.hpp"
#include "tash/recorder/frame-png.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/session/line.hpp"
#include "tash/session/session.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tash::python
{
  namespace
  {
    using session::Session;
    using session::SessionSettings;

    constexpr std::uint64_t SETTLE_FRAMES{ 30 };
    constexpr std::uint64_t TIMEOUT_FRAMES{ 10 };

    // Far enough in that the boot has written something into work ram.
    constexpr std::uint64_t WORK_RAM_FRAMES{ 300 };
    constexpr std::size_t   WORK_RAM_BYTES{ 65536 };
    constexpr std::size_t   STRING_BYTES{ 40 };
    constexpr std::uint32_t WIDTH_WORD{ 2 };
    constexpr std::uint32_t WIDTH_LONG{ 4 };
    constexpr std::uint32_t WORD_READS{ 256 };

    // The demo is still on its black opening at SETTLE_FRAMES, so this crop
    // is one colour and the count of it is the crop.
    constexpr perception::Region DARK{ 0, 0, 64, 48 };
    constexpr std::uint64_t DARK_PIXELS{ std::uint64_t{ DARK.width }
                                         * DARK.height };
    constexpr perception::Colour BLACK{ 0, 0, 0 };
    constexpr perception::Colour WHITE{ 255, 255, 255 };

    auto Root() -> std::filesystem::path
    {
      std::filesystem::path here{ std::filesystem::current_path() };
      while (!std::filesystem::exists(here / "buildutil.toml")
             && here.has_relative_path())
        here = here.parent_path();
      return here;
    }

    // Genesis Plus GX hands 68000 work ram over byte-swapped, so the string
    // at an address is the region's bytes paired the other way round.
    auto Unswapped(std::vector<std::byte> const& ram, std::size_t at,
                   std::size_t count) -> std::string
    {
      std::string read;
      for (std::size_t step{ 0 }; step < count; ++step)
      {
        char const letter{ static_cast<char>(ram[(at + step) ^ 1u]) };
        if (letter == '\0')
          break;
        read.push_back(letter);
      }
      return read;
    }

    // The number at an address as the 68000 sees it, spelled the way the
    // scenarios spell it before there is a word call.
    auto Guest(std::vector<std::byte> const& ram, std::uint32_t at,
               std::uint32_t width) -> std::int64_t
    {
      std::int64_t read{ 0 };
      for (std::uint32_t step{ 0 }; step < width; ++step)
        read = (read << 8)
               | static_cast<std::int64_t>(ram[(at + step) ^ 1u]);
      return read;
    }

    // A line a place can fall off, as a recording's does once a restore has
    // seeded it: the seed mints tokens of its own and an older place names
    // nothing, so only a line carried with the checkpoint puts the run back.
    class LineHeld final : public session::LineSource,
                           public session::FrameObserver
    {
    public:
      auto OnFrame(bus::FrameView const&, std::int64_t) -> void override
      { ++_frames; }

      auto OnRewind(session::Rewind const& back) -> void override
      {
        if (back.place && back.place->token == _minted
            && back.place->frames <= _frames)
        {
          _frames = back.place->frames;
          return;
        }
        if (back.line != nullptr)
        {
          _frames = back.line->frames;
          ++_minted;
          return;
        }
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
      { return session::LinePlace{ _frames, _minted }; }

    private:
      std::uint64_t _frames{ 0 };
      std::uint64_t _minted{ 1 };
      bool          _adrift{ false };
    };

    auto LineOf(ScenarioRun const& run) -> std::optional<std::uint64_t>
    {
      auto const line{ run.LineFrames() };
      EXPECT_TRUE(line.has_value()) << (line ? "" : line.error());
      return line ? *line : std::nullopt;
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

  TEST(ScenarioRunWithoutABundle, LookWritesUnderADirectoryTheRunOwns)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    EXPECT_FALSE(run.BundleRoot().has_value());

    auto const first{ run.Look(std::nullopt) };
    ASSERT_TRUE(first.has_value()) << (first ? "" : first.error());
    std::filesystem::path const shot{ *first };
    EXPECT_EQ(shot.filename(), "shot-0001.png");
    EXPECT_TRUE(std::filesystem::exists(shot)) << shot.string();
    EXPECT_NE(shot.parent_path().filename().string().find("scenario-shots"),
              std::string::npos) << shot.string();

    auto const second{ run.Look(std::nullopt) };
    ASSERT_TRUE(second.has_value()) << (second ? "" : second.error());
    EXPECT_EQ(std::filesystem::path{ *second }.filename(), "shot-0002.png");
    EXPECT_EQ(std::filesystem::path{ *second }.parent_path(),
              shot.parent_path());
  }

  TEST(ScenarioRunWithoutABundle, LookCropsToTheRegionItIsGiven)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    perception::Region const crop{ 16, 8, 64, 32 };
    auto const written{ run.Look(std::nullopt, crop) };
    ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());

    auto const read{ recorder::ReadPng(*written) };
    ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
    EXPECT_EQ(read->width, crop.width);
    EXPECT_EQ(read->height, crop.height);
  }

  TEST(ScenarioRunWithoutABundle, ColoursCountsTheColourACropHoldsAndNoOther)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const black{ run.Colours(DARK, BLACK, 0u) };
    ASSERT_TRUE(black.has_value()) << (black ? "" : black.error());
    EXPECT_EQ(*black, DARK_PIXELS);

    auto const white{ run.Colours(DARK, WHITE, 0u) };
    ASSERT_TRUE(white.has_value()) << (white ? "" : white.error());
    EXPECT_EQ(*white, 0u);
  }

  TEST(ScenarioRunWithoutABundle, AColourAnchorHoldsAtTheCountItAsksFor)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const held{ run.Judged(
      std::format("colour 0,0,0 within 0 over 0,0,{},{} at least {}",
                  DARK.width, DARK.height, DARK_PIXELS)) };
    ASSERT_TRUE(held.has_value()) << (held ? "" : held.error());
    EXPECT_TRUE(held->passed) << held->wording;

    auto const one_more{ run.Judged(
      std::format("colour 0,0,0 within 0 over 0,0,{},{} at least {}",
                  DARK.width, DARK.height, DARK_PIXELS + 1)) };
    ASSERT_TRUE(one_more.has_value()) << (one_more ? "" : one_more.error());
    EXPECT_FALSE(one_more->passed) << one_more->wording;

    auto const white{ run.Judged(
      std::format("colour 255,255,255 within 0 over 0,0,{},{} at least 1",
                  DARK.width, DARK.height)) };
    ASSERT_TRUE(white.has_value()) << (white ? "" : white.error());
    EXPECT_FALSE(white->passed) << white->wording;
  }

  TEST(ScenarioRunWithoutABundle, ForgetDropsTheStateAndForgivesAStrangeName)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const taken{ run.Checkpoint("here") };
    ASSERT_TRUE(taken.has_value()) << (taken ? "" : taken.error());
    auto const back{ run.Restore("here") };
    EXPECT_TRUE(back.has_value()) << (back ? "" : back.error());

    run.Forget("here");
    auto const gone{ run.Restore("here") };
    ASSERT_FALSE(gone.has_value());
    EXPECT_EQ(gone.error(), "python: no checkpoint is named here");

    run.Forget("never taken");
  }

  TEST(ScenarioRunWithACheckpointStore, AScratchOneSkipsTheProbeAndTheDisk)
  {
    auto const area{ utilities::ScratchAreaOf("scenario-scratch") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    parts.checkpoints = area->Path();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const taken{ run.Checkpoint("try", CheckpointKind::SCRATCH) };
    ASSERT_TRUE(taken.has_value()) << (taken ? "" : taken.error());
    EXPECT_EQ(taken->frames, SETTLE_FRAMES);
    EXPECT_FALSE(taken->file.has_value());
    EXPECT_EQ(run.Probes(), (session::ProbeCounts{ 0u, 0u, 1u }));

    // A probe would have stepped and restored twice over.
    EXPECT_EQ(run.Live().Frames(), SETTLE_FRAMES);

    auto const where{ run.CheckpointFile("try") };
    ASSERT_TRUE(where.has_value()) << (where ? "" : where.error());
    EXPECT_FALSE(std::filesystem::exists(*where)) << where->string();

    run.Live().Step(SETTLE_FRAMES);
    auto const back{ run.Restore("try") };
    ASSERT_TRUE(back.has_value()) << (back ? "" : back.error());
    EXPECT_FALSE(back->file.has_value()) << "the run still had it";

    run.Forget("try");
    auto const gone{ run.Restore("try") };
    ASSERT_FALSE(gone.has_value());
    EXPECT_NE(gone.error().find("no checkpoint at"), std::string::npos)
      << gone.error();
  }

  TEST(ScenarioRunWithACheckpointStore, ARestoreFromMemoryCarriesTheLine)
  {
    auto const area{ utilities::ScratchAreaOf("scenario-line") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    LineHeld line;
    session->Observe(line);
    RunParts parts;
    parts.session = session.get();
    parts.line = &line;
    parts.checkpoints = area->Path();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    ASSERT_TRUE(run.Checkpoint("seed", CheckpointKind::NAMED).has_value());
    run.Live().Step(SETTLE_FRAMES);
    ASSERT_TRUE(run.Checkpoint("plan", CheckpointKind::SCRATCH).has_value());
    EXPECT_EQ(LineOf(run), 2 * SETTLE_FRAMES);

    // Read back from disk, the seed starts the line afresh, and the place
    // the run holds for `plan` names a frame that is no longer on it.
    run.Forget("seed");
    ASSERT_TRUE(run.Restore("seed").has_value());
    EXPECT_EQ(LineOf(run), SETTLE_FRAMES);

    ASSERT_TRUE(run.Restore("plan").has_value());
    EXPECT_EQ(LineOf(run), 2 * SETTLE_FRAMES) << "the line the run kept";

    ASSERT_TRUE(run.Checkpoint("after", CheckpointKind::NAMED).has_value());
    CheckpointStore const store{ area->Path() };
    auto const written{ store.Read("after") };
    ASSERT_TRUE(written.has_value()) << (written ? "" : written.error());
    ASSERT_TRUE(written->line.has_value()) << "a checkpoint with no line";
    EXPECT_EQ(written->line->frames, 2 * SETTLE_FRAMES);
  }

  TEST(ScenarioRunWithACheckpointStore, ARestoreKeepsThisRunsCopyAndSaysSo)
  {
    constexpr std::uint32_t BEAT{ 0x068e };

    auto const area{ utilities::ScratchAreaOf("scenario-newer") };
    ASSERT_TRUE(area.has_value()) << (area ? "" : area.error());
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    parts.checkpoints = area->Path();
    ScenarioRun run{ parts };

    run.Live().Step(WORK_RAM_FRAMES);
    ASSERT_TRUE(run.Checkpoint("plan", CheckpointKind::NAMED).has_value());
    auto const early{ run.Number("system", BEAT, WIDTH_WORD) };
    ASSERT_TRUE(early.has_value()) << (early ? "" : early.error());

    // A second run over the same directory writes the same name again, the
    // way a `tash run` writes over a state a live session is holding.
    ScenarioRun other{ parts };
    other.Live().Step(WORK_RAM_FRAMES);
    auto const again{ other.Checkpoint("plan", CheckpointKind::NAMED) };
    ASSERT_TRUE(again.has_value()) << (again ? "" : again.error());
    auto const late{ other.Number("system", BEAT, WIDTH_WORD) };
    ASSERT_TRUE(late.has_value()) << (late ? "" : late.error());
    ASSERT_NE(*early, *late);

    auto const back{ run.Restore("plan") };
    ASSERT_TRUE(back.has_value()) << (back ? "" : back.error());
    EXPECT_FALSE(back->file.has_value()) << "the file was read";
    ASSERT_TRUE(back->passed_over.has_value()) << "the newer file is unsaid";
    EXPECT_EQ(*back->passed_over, *again->file);

    auto const served{ run.Number("system", BEAT, WIDTH_WORD) };
    ASSERT_TRUE(served.has_value()) << (served ? "" : served.error());
    EXPECT_EQ(*served, *early) << "the newer file was served";

    // Forgetting is what asks for the file, and then nothing was passed by.
    run.Forget("plan");
    auto const taken{ run.Restore("plan") };
    ASSERT_TRUE(taken.has_value()) << (taken ? "" : taken.error());
    EXPECT_EQ(taken->file, again->file);
    EXPECT_FALSE(taken->passed_over.has_value());

    auto const newer{ run.Number("system", BEAT, WIDTH_WORD) };
    ASSERT_TRUE(newer.has_value()) << (newer ? "" : newer.error());
    EXPECT_EQ(*newer, *late);
  }

  TEST(ScenarioRunWithoutABundle, RunUntilTakesTheAnchorPredicateString)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const seen{ run.Observe() };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());

    auto const here{ run.RunUntil(
      std::format("exact_hash {:016x}", seen->exact), TIMEOUT_FRAMES) };
    ASSERT_TRUE(here.has_value()) << (here ? "" : here.error());
    EXPECT_EQ(*here, 0u);

    auto const never{ run.RunUntil("exact_hash 0000000000000000",
                                   TIMEOUT_FRAMES) };
    ASSERT_FALSE(never.has_value());
    EXPECT_EQ(never.error(),
              std::format("tape: exact_hash 0000000000000000 did not hold "
                          "within {} frames", TIMEOUT_FRAMES));

    auto const nonsense{ run.RunUntil("money > 2000", TIMEOUT_FRAMES) };
    ASSERT_FALSE(nonsense.has_value());
    EXPECT_EQ(nonsense.error(), "tape: 'money' is not an anchor kind");
  }

  TEST(ScenarioRunWithoutABundle, JudgedAnswersTheWordingAVerdictCarries)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const held{ run.Judged("none") };
    ASSERT_TRUE(held.has_value()) << (held ? "" : held.error());
    EXPECT_TRUE(held->passed);
    EXPECT_EQ(held->wording,
              std::format("none holds at frame {}", run.Live().Frames()));

    auto const missed{ run.Judged("exact_hash 0000000000000000") };
    ASSERT_TRUE(missed.has_value()) << (missed ? "" : missed.error());
    EXPECT_FALSE(missed->passed);
    EXPECT_NE(missed->wording.find("does not hold"), std::string::npos)
      << missed->wording;

    auto const nobody{ run.Judged("watch money greater 2000") };
    ASSERT_FALSE(nobody.has_value());
    EXPECT_NE(nobody.error().find("has nobody to ask"), std::string::npos)
      << nobody.error();
  }

  TEST(ScenarioRunWithoutABundle, ObserveNamesTheFramesOwnGeometry)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const seen{ run.Observe() };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());

    auto const latest{ run.Latest() };
    ASSERT_TRUE(latest.has_value()) << (latest ? "" : latest.error());
    EXPECT_EQ(seen->width, latest->descriptor.width);
    EXPECT_EQ(seen->height, latest->descriptor.height);
    EXPECT_GT(seen->width, 0u);
    EXPECT_GT(seen->height, 0u);
  }

  TEST(ScenarioRunWithoutABundle, TheLineIsARefusalWithNoRecordingBehindIt)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    auto const line{ run.LineFrames() };
    ASSERT_FALSE(line.has_value());
    EXPECT_NE(line.error().find("keeps no tape"), std::string::npos)
      << line.error();
  }

  TEST(ScenarioRunWithoutABundle, AResetIsJudgedOnTheFrameItProduced)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const seen{ run.Observe() };
    ASSERT_TRUE(seen.has_value()) << (seen ? "" : seen.error());
    std::string const there{ std::format("exact_hash {:016x}", seen->exact) };

    auto const before{ run.Judged(there) };
    ASSERT_TRUE(before.has_value()) << (before ? "" : before.error());
    EXPECT_TRUE(before->passed);

    ASSERT_TRUE(run.Reset().has_value());

    auto const after{ run.Judged(there) };
    ASSERT_TRUE(after.has_value()) << (after ? "" : after.error());
    EXPECT_FALSE(after->passed) << after->wording;

    auto const watched{ run.Observe() };
    ASSERT_TRUE(watched.has_value()) << (watched ? "" : watched.error());
    EXPECT_NE(watched->exact, seen->exact);

    auto const waited{ run.RunUntil(there, TIMEOUT_FRAMES) };
    EXPECT_FALSE(waited.has_value()) << *waited;
  }

  TEST(ScenarioRunWithoutABundle, PlayRefusesAMachineThatHasRunSincePowerOn)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    auto const played{ run.Play(Root() / "examples/columns/tapes/"
                                         "title-to-game.yaml") };

    ASSERT_FALSE(played.has_value());
    EXPECT_NE(played.error().find("from power on"), std::string::npos)
      << played.error();
    EXPECT_NE(played.error().find(std::format("frame {}", SETTLE_FRAMES)),
              std::string::npos)
      << played.error();
  }

  TEST(ScenarioRunWithoutABundle, AResetPutsTheRunBackWhereATapeStarts)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(SETTLE_FRAMES);
    ASSERT_TRUE(run.Reset().has_value());
    auto const played{ run.Play(Root() / "no-such-tape.yaml") };

    ASSERT_FALSE(played.has_value());
    EXPECT_EQ(played.error().find("from power on"), std::string::npos)
      << played.error();
  }

  TEST(ScenarioRunWithoutABundle, StringReadsTheRegionInTheCoresByteOrder)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(WORK_RAM_FRAMES);
    auto const ram{ run.Memory("system", 0, WORK_RAM_BYTES) };
    ASSERT_TRUE(ram.has_value()) << (ram ? "" : ram.error());

    // Whatever this rom has in its work ram, the string at every address is
    // the swapped bytes cut at the first null; the count says it was read.
    std::size_t read{ 0 };
    for (std::size_t at{ 0 }; at < WORK_RAM_BYTES - STRING_BYTES;
         at += STRING_BYTES)
    {
      auto const text{ run.String("system", static_cast<std::uint32_t>(at),
                                  STRING_BYTES) };
      ASSERT_TRUE(text.has_value()) << (text ? "" : text.error());
      EXPECT_EQ(*text, Unswapped(*ram, at, STRING_BYTES)) << at;
      read += text->empty() ? 0u : 1u;
    }
    EXPECT_GT(read, 0u);
  }

  TEST(ScenarioRunWithoutABundle, AWordAndALongReadTheGuestsOwnView)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(WORK_RAM_FRAMES);
    auto const ram{ run.Memory("system", 0, WORK_RAM_BYTES) };
    ASSERT_TRUE(ram.has_value()) << (ram ? "" : ram.error());

    // The scenarios spell a swapped word as the two bytes at R xor 1, which
    // is exactly what the word and the long have to answer without it.
    for (std::uint32_t at{ 0 }; at < WORD_READS * WIDTH_LONG;
         at += WIDTH_LONG)
    {
      auto const word{ run.Number("system", at, WIDTH_WORD) };
      ASSERT_TRUE(word.has_value()) << (word ? "" : word.error());
      EXPECT_EQ(*word, Guest(*ram, at, WIDTH_WORD)) << at;

      auto const wide{ run.Number("system", at, WIDTH_LONG) };
      ASSERT_TRUE(wide.has_value()) << (wide ? "" : wide.error());
      EXPECT_EQ(*wide, Guest(*ram, at, WIDTH_LONG)) << at;
    }

    auto const past{ run.Number("system", WORK_RAM_BYTES - 1u, WIDTH_LONG) };
    ASSERT_FALSE(past.has_value());
    EXPECT_NE(past.error().find("past the"), std::string::npos)
      << past.error();
  }

  TEST(ScenarioRunWithoutABundle, StringRefusesPastTheRegionAndOffIt)
  {
    auto const session{ Opened() };
    ASSERT_NE(session, nullptr);
    RunParts parts;
    parts.session = session.get();
    ScenarioRun run{ parts };

    run.Live().Step(WORK_RAM_FRAMES);
    auto const past{ run.String("system", WORK_RAM_BYTES - 1u,
                                STRING_BYTES) };
    ASSERT_FALSE(past.has_value());
    EXPECT_NE(past.error().find("runs past its end"), std::string::npos)
      << past.error();

    auto const nowhere{ run.String("video", 0, STRING_BYTES) };
    ASSERT_FALSE(nowhere.has_value());
    EXPECT_NE(nowhere.error().find("no video memory"), std::string::npos)
      << nowhere.error();
  }
}
