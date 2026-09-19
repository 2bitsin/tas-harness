#include "tash/tash/bundle-recording.hpp"

#include "tash/recorder/bundle.hpp"
#include "tash/report/render.hpp"
#include "tash/recorder/verdicts-writer.hpp"
#include "tash/session/session.hpp"
#include "tash/tash/profile.hpp"
#include "tash/trace/reader.hpp"
#include "tash/utilities/executable-directory.hpp"
#include "tash/utilities/scratch-area.hpp"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <print>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
  using tash::cli::BundleRecording;
  using tash::recorder::Bundle;
  using tash::session::Session;
  using tash::session::SessionSettings;

  constexpr std::uint64_t RUN_FRAMES{ 600 };
  constexpr std::uint64_t STRIDE{ 2 };

  // Matroska timestamps are milliseconds, so a pts read back is that coarse.
  constexpr std::int64_t MATROSKA_TICK_NANOSECONDS{ 1'000'000 };

  auto Root() -> std::filesystem::path
  {
    std::filesystem::path here{ std::filesystem::current_path() };
    while (!std::filesystem::exists(here / "buildutil.toml")
           && here.has_relative_path())
      here = here.parent_path();
    return here;
  }

  auto Profile() -> tash::cli::RunProfile
  {
    auto profile{ tash::cli::ProfileFrom(Root()
                                         / "examples/homebrew/profile.yaml") };
    EXPECT_TRUE(profile.has_value()) << (profile ? "" : profile.error());
    return profile ? *profile : tash::cli::RunProfile{ };
  }

  auto Opened(tash::cli::RunProfile const& profile) -> std::unique_ptr<Session>
  {
    auto const beside{ tash::utilities::ExecutableDirectory() };
    EXPECT_TRUE(beside.has_value()) << (beside ? "" : beside.error());
    SessionSettings settings;
    settings.core = *beside / "genesis_plus_gx_libretro.so";
    settings.rom = Root() / profile.rom;
    auto session{ Session::Open(std::move(settings)) };
    EXPECT_TRUE(session.has_value()) << (session ? "" : session.error());
    return session ? std::move(*session) : nullptr;
  }

  struct Reopened
  {
    std::vector<std::int64_t> video_nanoseconds;
    std::vector<std::int64_t> audio_nanoseconds;
    bool                      has_audio{ false };
  };

  auto Reopen(std::filesystem::path const& path) -> Reopened
  {
    Reopened seen{ };
    AVFormatContext* opened{ nullptr };
    if (avformat_open_input(&opened, path.string().c_str(), nullptr, nullptr)
        < 0)
      return seen;
    if (avformat_find_stream_info(opened, nullptr) < 0)
    {
      avformat_close_input(&opened);
      return seen;
    }
    for (unsigned which{ 0 }; which != opened->nb_streams; ++which)
      seen.has_audio = seen.has_audio
                       || opened->streams[which]->codecpar->codec_type
                            == AVMEDIA_TYPE_AUDIO;

    AVPacket* packet{ av_packet_alloc() };
    while (av_read_frame(opened, packet) >= 0)
    {
      AVStream const& stream{ *opened->streams[packet->stream_index] };
      std::int64_t const at{ av_rescale_q(
        packet->pts, stream.time_base,
        AVRational{ 1, static_cast<int>(
                         tash::session::NANOSECONDS_PER_SECOND) }) };
      if (stream.codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        seen.video_nanoseconds.push_back(at);
      else if (stream.codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        seen.audio_nanoseconds.push_back(at);
      av_packet_unref(packet);
    }
    av_packet_free(&packet);
    avformat_close_input(&opened);
    return seen;
  }

  auto Read(std::filesystem::path const& path) -> std::string
  {
    std::ifstream file{ path, std::ios::binary };
    std::ostringstream held;
    held << file.rdbuf();
    return held.str();
  }

  auto FrameRecordsIn(std::filesystem::path const& path) -> std::uint64_t
  {
    auto reader{ tash::trace::Reader::Open(path) };
    EXPECT_TRUE(reader.has_value()) << (reader ? "" : reader.error());
    if (!reader)
      return 0;
    std::uint64_t frames{ 0 };
    reader->ForEach([&frames](auto const& record) {
      if constexpr (std::is_same_v<std::decay_t<decltype(record)>,
                                   tash::trace::FrameRecord>)
        ++frames;
    });
    EXPECT_FALSE(reader->Truncated());
    return frames;
  }
}

TEST(BundleRecording, AHomebrewRunFillsABundle)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("bundle-test") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  tash::cli::RunProfile const profile{ Profile() };
  ASSERT_EQ(profile.name, "zsenilia");
  auto const bundle{ Bundle::Create(scratch->Path(), *profile.name) };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  EXPECT_TRUE(bundle->Root().filename().string().ends_with("-zsenilia"));

  auto const session{ Opened(profile) };
  ASSERT_NE(session, nullptr);
  auto const recording{ BundleRecording::Open(
    *bundle, session->Fps(),
    static_cast<std::uint32_t>(session->CoreOf().AvInfo().timing.sample_rate),
    "tash-test") };
  ASSERT_TRUE(recording.has_value()) << (recording ? "" : recording.error());
  session->Observe(static_cast<tash::session::FrameObserver&>(**recording));
  session->Observe(static_cast<tash::session::AudioObserver&>(**recording));

  session->Step(RUN_FRAMES);
  auto const counts{ (*recording)->Close() };
  ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
  EXPECT_EQ((*recording)->Problem(), "");

  // The encoder queue is bounded and the run is unpaced, so a loaded box
  // drops frames by design; what must hold is that none goes missing.
  std::print("[   drops  ] {} of {} frames dropped by the encoder\n",
             counts->dropped, session->Frames());
  EXPECT_EQ(counts->recorded, session->Frames());
  EXPECT_EQ(counts->encoded + counts->dropped, session->Frames());
  EXPECT_GT(counts->audio_chunks, 0u);

  tash::recorder::RunManifest const manifest{
    "0.0.0", session->CoreOf().Information().name,
    session->CoreOf().Information().version, profile.rom,
    "examples/homebrew/profile.yaml", session->Frames(), session->Fps(),
    std::string{ tash::clock::Named(session->DeterminismLevel()) },
    "completed"
  };
  ASSERT_TRUE(bundle->Write(manifest).has_value());
  ASSERT_TRUE(
    tash::recorder::VerdictsWriter::Open(bundle->Verdicts()).has_value());

  for (std::filesystem::path const& path :
       { bundle->Video(), bundle->Trace(), bundle->Shots(), bundle->Clips(),
         bundle->Verdicts(), bundle->Manifest() })
    EXPECT_TRUE(std::filesystem::exists(path)) << path.string();

  auto const read{ bundle->Read() };
  ASSERT_TRUE(read.has_value()) << (read ? "" : read.error());
  EXPECT_EQ(*read, manifest);

  auto const page{ tash::report::RenderReport(bundle->Root()) };
  ASSERT_TRUE(page.has_value()) << (page ? "" : page.error());
  std::string const html{ Read(*page) };
  std::regex const linked{ R"RX((?:href|src)="([^"]+)")RX" };
  std::size_t links{ 0 };
  for (std::sregex_iterator at{ html.begin(), html.end(), linked }, end{ };
       at != end; ++at)
  {
    EXPECT_TRUE(std::filesystem::exists(bundle->Root() / (*at)[1].str()))
      << (*at)[1].str();
    ++links;
  }
  EXPECT_GT(links, 0u);
  EXPECT_EQ(html.find("<li class=\"pass\""), std::string::npos);

  auto const seen{ Reopen(bundle->Video()) };
  EXPECT_EQ(FrameRecordsIn(bundle->Trace()), session->Frames());
  EXPECT_EQ(seen.video_nanoseconds.size(), counts->encoded);
  EXPECT_TRUE(seen.has_audio);
}

TEST(BundleRecording, AStrideEncodesOneFrameInNAndDropsTheAudio)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("bundle-stride") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  tash::cli::RunProfile const profile{ Profile() };
  auto const bundle{ Bundle::Create(scratch->Path(), *profile.name) };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());

  auto const session{ Opened(profile) };
  ASSERT_NE(session, nullptr);
  auto const recording{ BundleRecording::Open(
    *bundle, session->Fps(),
    static_cast<std::uint32_t>(session->CoreOf().AvInfo().timing.sample_rate),
    "tash-test", STRIDE) };
  ASSERT_TRUE(recording.has_value()) << (recording ? "" : recording.error());
  session->Observe(static_cast<tash::session::FrameObserver&>(**recording));
  session->Observe(static_cast<tash::session::AudioObserver&>(**recording));

  session->Step(RUN_FRAMES);
  auto const counts{ (*recording)->Close() };
  ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
  EXPECT_EQ((*recording)->Problem(), "");

  // The trace keeps every frame; only the encoder sees one in two, and no
  // audio goes with a time-lapse.
  EXPECT_EQ(counts->recorded, session->Frames());
  EXPECT_EQ(counts->encoded + counts->dropped, RUN_FRAMES / STRIDE);
  EXPECT_EQ(counts->audio_chunks, 0u);
  EXPECT_EQ(counts->audio_dropped, 0u);
  EXPECT_EQ(FrameRecordsIn(bundle->Trace()), session->Frames());

  auto const seen{ Reopen(bundle->Video()) };
  EXPECT_EQ(seen.video_nanoseconds.size(), counts->encoded);
  EXPECT_FALSE(seen.has_audio);

  // The stride compresses the film's clock, so a kept picture sits one frame
  // period from the last and the film is a time-lapse, not a slideshow.
  ASSERT_GT(seen.video_nanoseconds.size(), 1u);
  std::int64_t const period{ std::llround(
    static_cast<double>(tash::session::NANOSECONDS_PER_SECOND)
    / session->Fps()) };
  std::int64_t closest{ std::numeric_limits<std::int64_t>::max() };
  std::int64_t widest{ 0 };
  std::uint64_t irregular{ 0 };
  for (std::size_t which{ 1 }; which != seen.video_nanoseconds.size();
       ++which)
  {
    std::int64_t const apart{ seen.video_nanoseconds[which]
                              - seen.video_nanoseconds[which - 1] };
    closest = std::min(closest, apart);
    widest = std::max(widest, apart);
    if (std::abs(apart - period) > MATROSKA_TICK_NANOSECONDS)
      ++irregular;
  }
  std::print("[  spacing ] {} pictures, {} to {} ns apart, a frame is {} ns\n",
             seen.video_nanoseconds.size(), closest, widest, period);
  EXPECT_LE(std::abs(closest - period), MATROSKA_TICK_NANOSECONDS);
  // Only a picture the encoder dropped may leave a gap wider than that.
  EXPECT_LE(irregular, counts->dropped);
}

TEST(BundleRecording, BothStreamsAreStampedFromTheRecordsOwnClock)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("bundle-restored") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  tash::cli::RunProfile const profile{ Profile() };
  auto const bundle{ Bundle::Create(scratch->Path(), *profile.name) };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());

  auto const session{ Opened(profile) };
  ASSERT_NE(session, nullptr);

  // A session that restores a checkpoint from disk before it records is a
  // run whose first recorded frame carries the checkpoint's frame number.
  session->Step(RUN_FRAMES);
  auto const state{ session->SaveState() };
  ASSERT_TRUE(state.has_value()) << (state ? "" : state.error());
  ASSERT_GT(session->Frames(), 0u);

  auto const recording{ BundleRecording::Open(
    *bundle, session->Fps(),
    static_cast<std::uint32_t>(session->CoreOf().AvInfo().timing.sample_rate),
    "tash-test") };
  ASSERT_TRUE(recording.has_value()) << (recording ? "" : recording.error());
  session->Observe(static_cast<tash::session::FrameObserver&>(**recording));
  session->Observe(static_cast<tash::session::AudioObserver&>(**recording));

  session->Step(RUN_FRAMES);

  // A restore mid-record moves the run's frame counter on to the frame the
  // state was taken at; the record's own clock does not move with it.
  tash::session::Checkpoint kept{ };
  kept.name = "ahead";
  kept.state = *state;
  kept.frames = session->Frames() + RUN_FRAMES;
  ASSERT_TRUE(session->Restore(kept).has_value());
  session->Step(RUN_FRAMES);

  auto const counts{ (*recording)->Close() };
  ASSERT_TRUE(counts.has_value()) << (counts ? "" : counts.error());
  EXPECT_EQ((*recording)->Problem(), "");
  EXPECT_GT(counts->audio_chunks, 0u);

  auto const seen{ Reopen(bundle->Video()) };
  ASSERT_FALSE(seen.video_nanoseconds.empty());
  ASSERT_FALSE(seen.audio_nanoseconds.empty());

  // One clock: the first picture and the first block share a timestamp, and
  // the film is as long as the frames it holds, not as long as the run.
  EXPECT_LE(std::abs(seen.video_nanoseconds.front()
                     - seen.audio_nanoseconds.front()),
            MATROSKA_TICK_NANOSECONDS);
  EXPECT_LE(std::abs(seen.video_nanoseconds.front()),
            MATROSKA_TICK_NANOSECONDS);

  std::int64_t const period{ std::llround(
    static_cast<double>(tash::session::NANOSECONDS_PER_SECOND)
    / session->Fps()) };
  std::int64_t widest{ 0 };
  for (std::size_t which{ 1 }; which != seen.video_nanoseconds.size();
       ++which)
    widest = std::max(widest, seen.video_nanoseconds[which]
                                - seen.video_nanoseconds[which - 1]);
  std::print("[  streams ] first video {} ns, first audio {} ns, widest gap "
             "{} ns, a frame is {} ns\n",
             seen.video_nanoseconds.front(), seen.audio_nanoseconds.front(),
             widest, period);
  // Only a dropped picture leaves a gap, and never the restore's jump.
  EXPECT_LT(widest, period * static_cast<std::int64_t>(counts->dropped + 2));
  EXPECT_LE(std::abs(seen.audio_nanoseconds.back()
                     - seen.video_nanoseconds.back()),
            period * 2);
}

TEST(BundleRecording, AStrideOfZeroIsRefused)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("bundle-no-stride") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  tash::cli::RunProfile const profile{ Profile() };
  auto const bundle{ Bundle::Create(scratch->Path(), *profile.name) };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());

  auto const refused{ BundleRecording::Open(*bundle, 60.0, 0u, "tash-test",
                                            0u) };
  ASSERT_FALSE(refused.has_value());
  EXPECT_NE(refused.error().find("at least 1"), std::string::npos)
    << refused.error();
}
