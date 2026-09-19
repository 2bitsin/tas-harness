#pragma once
// A bundle written by hand, so the renderer's tests do not need a core.

#include "tash/recorder/bundle.hpp"
#include "tash/recorder/run-manifest.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/writer.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace tash::report::detail::synthetic_bundle
{
  inline constexpr std::uint64_t SYNTHETIC_FRAMES{ 1500 };
  inline constexpr std::uint64_t SPIKE_FRAME{ 900 };
  inline constexpr std::int64_t  FRAME_NANOSECONDS{ 16'688'000 };
  inline constexpr std::uint32_t SCORE_WATCH{ 0 };
  inline constexpr std::uint32_t LEVEL_WATCH{ 1 };
  inline constexpr char const*   SYNTHETIC_GROUP{ "decision" };
  // Restores the way a search makes them: every trial goes back to the one
  // checkpoint, taken when the run had made RESTORE_KEPT frames.
  inline constexpr std::uint64_t RESTORE_KEPT{ 95 };
  inline constexpr std::uint64_t RESTORE_FIRST{ 100 };
  inline constexpr std::uint64_t RESTORE_STRIDE{ 10 };
  // A power cycle part way through, which folds everything before it off.
  inline constexpr std::uint64_t RESET_FRAME{ 500 };

  struct SyntheticParts
  {
    std::string   verdict_text{ "nothing untoward" };
    bool          failing{ true };
    std::uint64_t grouped{ 0 };  // marks of SYNTHETIC_GROUP, every other frame
    std::uint64_t restores{ 0 };  // trials, RESTORE_STRIDE frames apart
    bool          reset{ false }; // one power cycle, at RESET_FRAME
    std::uint64_t video_stride{ recorder::EVERY_FRAME };
    std::uint16_t format_version{ trace::FORMAT_VERSION };
  };

  inline auto Touch(std::filesystem::path const& path) -> void
  {
    std::ofstream file{ path, std::ios::binary };
    file << "not really a video\n";
  }

  // Frames with one change spike, a watch that climbs, one that never
  // moves, two marks and two verdicts, the second of which can fail.
  inline auto SyntheticBundle(recorder::Bundle const& bundle,
                              SyntheticParts const& parts) -> void
  {
    trace::Header heading{ trace::Header::Now("test") };
    heading.format_version = parts.format_version;
    auto writer{ trace::Writer::Open(bundle.Trace(), std::move(heading)) };
    for (std::uint64_t frame{ 0 }; frame != SYNTHETIC_FRAMES; ++frame)
    {
      trace::FrameRecord record;
      record.frame = frame;
      record.harness_time
        = static_cast<std::int64_t>(frame) * FRAME_NANOSECONDS;
      record.hash_exact = frame * 2654435761u;
      record.change_amount = frame == SPIKE_FRAME ? 0.93 : 0.004;
      static_cast<void>(writer->Write(record));
      if (frame % 60 == 0)
      {
        static_cast<void>(writer->Write(
          trace::WatchRecord{ frame, SCORE_WATCH,
                              static_cast<std::int64_t>(frame) }));
        static_cast<void>(writer->Write(
          trace::WatchRecord{ frame, LEVEL_WATCH, 0 }));
      }
    }
    static_cast<void>(writer->Write(
      trace::MarkRecord{ 10, "tape" }));
    static_cast<void>(writer->Write(
      trace::MarkRecord{ SPIKE_FRAME, "burst-0" }));
    for (std::uint64_t at{ 0 }; at != parts.grouped; ++at)
      static_cast<void>(writer->Write(
        trace::MarkRecord{ at * 2, std::format("trial {}", at),
                           SYNTHETIC_GROUP }));
    for (std::uint64_t at{ 0 }; at != parts.restores; ++at)
    {
      std::uint64_t const when{ RESTORE_FIRST + (at * RESTORE_STRIDE) };
      static_cast<void>(writer->Write(
        trace::RestoreRecord{ when, RESTORE_KEPT,
                              std::format("trial {}", at) }));
    }
    if (parts.reset)
      static_cast<void>(writer->Write(trace::ResetRecord{ RESET_FRAME }));
    static_cast<void>(writer->Write(
      trace::VerdictRecord{ 300, "the tape reached the playfield", true,
                            "5 segments" }));
    static_cast<void>(writer->Write(
      trace::VerdictRecord{ SYNTHETIC_FRAMES - 1, "the score rose",
                            !parts.failing, parts.verdict_text }));
    static_cast<void>(writer->Flush());

    recorder::RunManifest manifest;
    manifest.harness_version = "0.0.0";
    manifest.core_name = "Genesis Plus GX";
    manifest.core_version = "v1.7.4";
    manifest.rom = "examples/columns/rom.zip";
    manifest.profile = "examples/columns/profile.yaml";
    manifest.frames = SYNTHETIC_FRAMES;
    manifest.fps = 59.9227;
    manifest.determinism = "D2";
    manifest.outcome = "completed";
    manifest.watches = std::vector<std::string>{ "score", "level" };
    if (parts.video_stride > recorder::EVERY_FRAME)
      manifest.video_stride = parts.video_stride;
    static_cast<void>(bundle.Write(manifest));

    Touch(bundle.Video());
    Touch(bundle.Shots() / "0042.png");
    Touch(bundle.Shots() / "shot0001.png");
    Touch(bundle.Clips() / "burst-0.mkv");
  }
}
