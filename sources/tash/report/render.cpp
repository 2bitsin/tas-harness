#include "tash/report/render.hpp"

#include "tash/report/html-writer.hpp"
#include "tash/report/page-template.hpp"
#include "tash/report/report-data.hpp"
#include "tash/report/trace-digest.hpp"
#include "tash/recorder/bundle.hpp"
#include "tash/recorder/run-manifest.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <format>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace tash::report::detail::render
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Refused;

  namespace
  {
    constexpr int    STRIP_WIDTH{ 720 };
    constexpr int    STRIP_HEIGHT{ 72 };
    constexpr int    VERDICT_TICK_TOP{ 0 };
    constexpr int    MARK_TICK_TOP{ 11 };
    constexpr double FLAT_STRIP_HEIGHT{ 0.5 };

    // Decision 6: a run of hundreds of decisions is one row per this many
    // marks of a group, not a row each.
    constexpr std::size_t GROUP_ROW_MARKS{ 50 };

    constexpr std::uint64_t SECONDS_PER_MINUTE{ 60 };
    constexpr std::uint64_t MINUTES_PER_HOUR{ 60 };

    constexpr std::string_view PASS_COLOUR{ "#a3be8c" };
    constexpr std::string_view FAIL_COLOUR{ "#bf616a" };
    constexpr std::string_view MARK_COLOUR{ "#5e81ac" };

    auto Seconds(double at) -> std::string
    {
      return std::format("{:.3f}", at);
    }

    auto Worded(std::uint64_t seconds) -> std::string
    {
      std::uint64_t const minutes{ seconds / SECONDS_PER_MINUTE };
      if (std::uint64_t const hours{ minutes / MINUTES_PER_HOUR }; hours != 0)
        return std::format("{} h {} min {} s", hours,
                           minutes % MINUTES_PER_HOUR,
                           seconds % SECONDS_PER_MINUTE);
      if (minutes != 0)
        return std::format("{} min {} s", minutes,
                           seconds % SECONDS_PER_MINUTE);
      return std::format("{} s", seconds);
    }

    // What the stride left of the run: one picture in `stride`, played at
    // the run's own fps.
    auto VideoRow(std::uint64_t stride, std::uint64_t frames, double fps)
      -> std::string
    {
      std::string row{ std::format("one frame in {}, no audio", stride) };
      if (frames == 0 || fps <= 0.0)
        return row;
      std::uint64_t const pictures{ (frames + stride - 1) / stride };
      return row + std::format(", {} film",
                               Worded(static_cast<std::uint64_t>(std::llround(
                                 static_cast<double>(pictures) / fps))));
    }

    auto Filled(std::string_view shape) -> HtmlWriter
    {
      return HtmlWriter{ shape };
    }

    // The page's seeks divide harness seconds by this; a manifest written by
    // hand can say anything, and nothing may divide by zero.
    auto FilmStride(recorder::RunManifest const& manifest) -> std::uint64_t
    {
      return std::max(manifest.video_stride.value_or(recorder::EVERY_FRAME),
                      recorder::EVERY_FRAME);
    }

    auto FramesRow(recorder::RunManifest const& manifest,
                   std::optional<std::uint64_t> kept) -> std::string
    {
      std::string said{ kept ? std::format("{} recorded, {} kept",
                                           manifest.frames, *kept)
                             : std::format("{}", manifest.frames) };
      if (manifest.probed)
        said += std::format(", probed {} checkpoints, {} parted, {} unprobed",
                            *manifest.probed, manifest.parted.value_or(0),
                            manifest.unprobed.value_or(0));
      return said;
    }

    auto ManifestRows(recorder::RunManifest const& manifest,
                      std::optional<std::uint64_t> kept)
      -> Result<std::string>
    {
      std::vector<std::pair<std::string_view, std::string>> fields{
        { "core", std::format("{} {}", manifest.core_name,
                              manifest.core_version) },
        { "rom", manifest.rom },
        { "profile", manifest.profile },
        { "frames", FramesRow(manifest, kept) },
        { "fps", std::format("{:.4f}", manifest.fps) },
        { "determinism", manifest.determinism },
        { "harness", manifest.harness_version }
      };
      if (manifest.video_stride
          && *manifest.video_stride > recorder::EVERY_FRAME)
        fields.emplace_back("video",
                            VideoRow(*manifest.video_stride,
                                     kept.value_or(manifest.frames),
                                     manifest.fps));
      if (manifest.replay_of)
        fields.emplace_back("replay",
                            std::format("replay of {}", *manifest.replay_of));

      std::string rows;
      for (auto const& [field, value] : fields)
      {
        Result<std::string> const row{ Filled(page_template::MANIFEST_ROW)
                                         .Fill("field", field)
                                         .Fill("value", value)
                                         .Page() };
        if (!row)
          return Forwarded(row);
        rows.append(*row);
      }
      return rows;
    }

    auto VerdictRow(VerdictEntry const& verdict) -> Result<std::string>
    {
      return Filled(page_template::VERDICT_ROW)
        .Fill("outcome", verdict.passed ? "pass" : "fail")
        .Fill("at", Seconds(verdict.when.seconds))
        .Fill("frame", std::format("{}", verdict.when.frame))
        .Fill("seconds", Seconds(verdict.when.seconds))
        .Fill("name", verdict.name)
        .Fill("text", verdict.text)
        .Page();
    }

    auto MarkRow(MarkEntry const& mark) -> Result<std::string>
    {
      return Filled(page_template::MARK_ROW)
        .Fill("at", Seconds(mark.when.seconds))
        .Fill("frame", std::format("{}", mark.when.frame))
        .Fill("seconds", Seconds(mark.when.seconds))
        .Fill("text", mark.text)
        .Page();
    }

    // What a group's row says: where it started, where it ended and how
    // many marks it stands for.
    struct GroupChunk
    {
      std::string   name;
      Moment        first{ };
      std::uint64_t last{ 0 };
      std::size_t   count{ 0 };
    };

    auto GroupRow(GroupChunk const& chunk) -> Result<std::string>
    {
      return Filled(page_template::GROUP_ROW)
        .Fill("at", Seconds(chunk.first.seconds))
        .Fill("frame", std::format("{}", chunk.first.frame))
        .Fill("seconds", Seconds(chunk.first.seconds))
        .Fill("group", chunk.name)
        .Fill("count", std::format("{}", chunk.count))
        .Fill("first", std::format("{}", chunk.first.frame))
        .Fill("last", std::format("{}", chunk.last))
        .Page();
    }

    auto RestoreRow(RestoreEntry const& restore) -> Result<std::string>
    {
      return Filled(page_template::RESTORE_ROW)
        .Fill("at", Seconds(restore.when.seconds))
        .Fill("frame", std::format("{}", restore.when.frame))
        .Fill("seconds", Seconds(restore.when.seconds))
        .Fill("to", std::format("{}", restore.to))
        .Fill("name", restore.name)
        .Page();
    }

    auto RewindGroupRow(GroupChunk const& chunk) -> Result<std::string>
    {
      return Filled(page_template::REWIND_GROUP_ROW)
        .Fill("at", Seconds(chunk.first.seconds))
        .Fill("frame", std::format("{}", chunk.first.frame))
        .Fill("seconds", Seconds(chunk.first.seconds))
        .Fill("count", std::format("{}", chunk.count))
        .Fill("first", std::format("{}", chunk.first.frame))
        .Fill("last", std::format("{}", chunk.last))
        .Page();
    }

    auto ResetRow(ResetEntry const& reset) -> Result<std::string>
    {
      return Filled(page_template::RESET_ROW)
        .Fill("at", Seconds(reset.when.seconds))
        .Fill("frame", std::format("{}", reset.when.frame))
        .Fill("seconds", Seconds(reset.when.seconds))
        .Page();
    }

    auto ResetGroupRow(GroupChunk const& chunk) -> Result<std::string>
    {
      return Filled(page_template::RESET_GROUP_ROW)
        .Fill("at", Seconds(chunk.first.seconds))
        .Fill("frame", std::format("{}", chunk.first.frame))
        .Fill("seconds", Seconds(chunk.first.seconds))
        .Fill("count", std::format("{}", chunk.count))
        .Fill("first", std::format("{}", chunk.first.frame))
        .Fill("last", std::format("{}", chunk.last))
        .Page();
    }

    using TimelineRow = std::pair<std::uint64_t, std::string>;

    auto MarkRows(std::vector<MarkEntry> const& marks)
      -> Result<std::vector<TimelineRow>>
    {
      std::vector<TimelineRow> rows;
      std::map<std::string, GroupChunk> pending;

      auto const flush{ [&rows](GroupChunk const& chunk) -> Outcome
      {
        Result<std::string> row{ GroupRow(chunk) };
        if (!row)
          return Forwarded(row);
        rows.emplace_back(chunk.first.frame, std::move(*row));
        return { };
      } };

      for (MarkEntry const& mark : marks)
      {
        if (mark.group.empty())
        {
          Result<std::string> row{ MarkRow(mark) };
          if (!row)
            return Forwarded(row);
          rows.emplace_back(mark.when.frame, std::move(*row));
          continue;
        }

        GroupChunk& chunk{ pending[mark.group] };
        if (chunk.count == 0)
          chunk = GroupChunk{ mark.group, mark.when, mark.when.frame, 0 };
        chunk.last = mark.when.frame;
        ++chunk.count;
        if (chunk.count == GROUP_ROW_MARKS)
        {
          if (Outcome const written{ flush(chunk) }; !written)
            return Forwarded(written);
          chunk = GroupChunk{ };
        }
      }

      for (auto const& [name, chunk] : pending)
        if (chunk.count != 0)
          if (Outcome const written{ flush(chunk) }; !written)
            return Forwarded(written);
      return rows;
    }

    // Above fifty, a repeated row collapses the way a group of marks does.
    auto Collapsed(auto const& entries, auto const& one, auto const& many)
      -> Result<std::vector<TimelineRow>>
    {
      std::vector<TimelineRow> rows;
      if (entries.size() <= GROUP_ROW_MARKS)
      {
        for (auto const& entry : entries)
        {
          Result<std::string> row{ one(entry) };
          if (!row)
            return Forwarded(row);
          rows.emplace_back(entry.when.frame, std::move(*row));
        }
        return rows;
      }

      GroupChunk chunk{ };
      auto const flush{ [&rows, &chunk, &many] () -> Outcome
      {
        Result<std::string> row{ many(chunk) };
        if (!row)
          return Forwarded(row);
        rows.emplace_back(chunk.first.frame, std::move(*row));
        chunk = GroupChunk{ };
        return { };
      } };

      for (auto const& entry : entries)
      {
        if (chunk.count == 0)
          chunk.first = entry.when;
        chunk.last = entry.when.frame;
        ++chunk.count;
        if (chunk.count == GROUP_ROW_MARKS)
          if (Outcome const written{ flush() }; !written)
            return Forwarded(written);
      }
      if (chunk.count != 0)
        if (Outcome const written{ flush() }; !written)
          return Forwarded(written);
      return rows;
    }

    auto Tick(Moment const& when, std::uint64_t frames, int top,
              std::string_view colour, std::string_view text)
      -> Result<std::string>
    {
      double const across{
        frames < 2 ? 0.0
                   : static_cast<double>(when.frame) * STRIP_WIDTH
                       / static_cast<double>(frames - 1) };
      return Filled(page_template::TICK)
        .Fill("x", std::format("{:.2f}", across))
        .Fill("y", std::format("{}", top))
        .Fill("colour", colour)
        .Fill("text", text)
        .Page();
    }

    auto Strip(StripData const& strip) -> Result<std::string>
    {
      // A strip that never changes has no scale to draw against, so its
      // line runs through the middle rather than along the bottom edge.
      double const span{ strip.highest - strip.lowest };
      bool const   flat{ span <= 0.0 };
      double const step{
        strip.points.size() < 2
          ? 0.0
          : static_cast<double>(STRIP_WIDTH)
              / static_cast<double>(strip.points.size() - 1) };

      std::string points;
      for (std::size_t at{ 0 }; at != strip.points.size(); ++at)
      {
        double const height{
          flat ? FLAT_STRIP_HEIGHT
               : (strip.points[at] - strip.lowest) / span };
        points.append(std::format(
          "{}{:.1f},{:.1f}", at == 0 ? "" : " ",
          static_cast<double>(at) * step,
          STRIP_HEIGHT - height * STRIP_HEIGHT));
      }

      return Filled(page_template::STRIP)
        .Fill("name", strip.name)
        .Fill("range", std::format("{:g} to {:g}, {} points", strip.lowest,
                                   strip.highest, strip.points.size()))
        .Fill("width", std::format("{}", STRIP_WIDTH))
        .Fill("height", std::format("{}", STRIP_HEIGHT))
        .FillHtml("points", std::move(points))
        .Page();
    }

    auto FrameOfName(std::string const& stem) -> std::optional<std::uint64_t>
    {
      if (stem.empty()
          || !std::all_of(stem.begin(), stem.end(),
                          [](unsigned char letter)
                          { return std::isdigit(letter) != 0; }))
        return { };
      std::uint64_t frame{ 0 };
      char const* const end{ stem.data() + stem.size() };
      auto const [stopped, failed]{
        std::from_chars(stem.data(), end, frame) };
      if (failed != std::errc{ } || stopped != end)
        return { };
      return frame;
    }

    auto Artifacts(std::filesystem::path const& directory,
                   std::string_view held, TraceReport const& trace)
      -> Result<std::string>
    {
      std::error_code failed;
      std::vector<std::filesystem::path> files;
      for (auto const& entry :
           std::filesystem::directory_iterator{ directory, failed })
        if (entry.is_regular_file())
          files.push_back(entry.path());
      std::sort(files.begin(), files.end());

      std::string rows;
      for (std::filesystem::path const& file : files)
      {
        auto const frame{ FrameOfName(file.stem().string()) };
        std::string seek;
        if (frame)
        {
          std::uint64_t const at{ std::min(
            *frame, trace.frames == 0 ? 0 : trace.frames - 1) };
          seek = std::format(" data-at=\"{}\"", Seconds(
            trace.frames == 0 ? 0.0
                              : static_cast<double>(at) * trace.seconds
                                  / static_cast<double>(trace.frames)));
        }
        Result<std::string> const row{
          Filled(page_template::ARTIFACT_ROW)
            .FillHtml("seek", std::move(seek))
            .Fill("frame", frame ? std::format("{}", *frame)
                                 : std::string{ })
            .Fill("href",
                  std::format("{}/{}", held, file.filename().string()))
            .Fill("name", file.filename().string())
            .Page() };
        if (!row)
          return Forwarded(row);
        rows.append(*row);
      }
      return rows;
    }

    auto Tails(TraceReport const& trace) -> Result<std::string>
    {
      std::string blocks;
      for (VerdictEntry const& verdict : trace.verdicts)
      {
        if (verdict.passed || verdict.tail.empty())
          continue;
        std::string rows;
        for (TailRow const& row : verdict.tail)
        {
          Result<std::string> const line{ Filled(page_template::TAIL_ROW)
                                            .Fill("frame", row.frame)
                                            .Fill("kind", row.kind)
                                            .Fill("details", row.details)
                                            .Page() };
          if (!line)
            return Forwarded(line);
          rows.append(*line);
        }
        Result<std::string> const block{
          Filled(page_template::TAIL)
            .Fill("title",
                  std::format("the {} records before {} failed at frame {}",
                              verdict.tail.size(), verdict.name,
                              verdict.when.frame))
            .FillHtml("rows", std::move(rows))
            .Page() };
        if (!block)
          return Forwarded(block);
        blocks.append(*block);
      }
      return blocks;
    }

    auto WatchNames(recorder::RunManifest const& manifest)
      -> std::vector<std::string>
    {
      return manifest.watches ? *manifest.watches
                              : std::vector<std::string>{ };
    }
  }

  auto RenderReport(std::filesystem::path const& bundle_root)
    -> Result<std::filesystem::path>
  {
    Result<recorder::Bundle> const bundle{
      recorder::Bundle::At(bundle_root) };
    if (!bundle)
      return Forwarded(bundle);

    bool const traced{ std::filesystem::exists(bundle->Trace()) };
    bool const written{ std::filesystem::exists(bundle->Manifest()) };
    if (!traced && !written)
      return Refused("report: '{}' holds neither {} nor {}",
                     bundle_root.string(), recorder::MANIFEST_NAME,
                     recorder::TRACE_NAME);

    // A run still going has no run.yaml yet; one that cannot be read is a
    // broken bundle, not a young one.
    recorder::RunManifest manifest;
    if (written)
    {
      Result<recorder::RunManifest> read{ bundle->Read() };
      if (!read)
        return Forwarded(read);
      manifest = std::move(*read);
    }

    TraceReport trace;
    if (traced)
    {
      Result<TraceReport> read{
        TraceDigest::Of(bundle->Trace(), WatchNames(manifest)) };
      if (!read)
        return Forwarded(read);
      trace = std::move(*read);
    }

    Result<std::vector<TimelineRow>> marked{ MarkRows(trace.marks) };
    if (!marked)
      return Forwarded(marked);
    std::vector<TimelineRow> rows{ std::move(*marked) };

    Result<std::vector<TimelineRow>> rewound{
      Collapsed(trace.restores, RestoreRow, RewindGroupRow) };
    if (!rewound)
      return Forwarded(rewound);
    rows.insert(rows.end(), std::make_move_iterator(rewound->begin()),
                std::make_move_iterator(rewound->end()));

    Result<std::vector<TimelineRow>> powered{
      Collapsed(trace.resets, ResetRow, ResetGroupRow) };
    if (!powered)
      return Forwarded(powered);
    rows.insert(rows.end(), std::make_move_iterator(powered->begin()),
                std::make_move_iterator(powered->end()));

    std::string ruler;
    for (VerdictEntry const& verdict : trace.verdicts)
    {
      Result<std::string> row{ VerdictRow(verdict) };
      if (!row)
        return Forwarded(row);
      rows.emplace_back(verdict.when.frame, std::move(*row));
      Result<std::string> const tick{
        Tick(verdict.when, trace.frames, VERDICT_TICK_TOP,
             verdict.passed ? PASS_COLOUR : FAIL_COLOUR, verdict.name) };
      if (!tick)
        return Forwarded(tick);
      ruler.append(*tick);
    }
    for (MarkEntry const& mark : trace.marks)
    {
      Result<std::string> const tick{ Tick(mark.when, trace.frames,
                                           MARK_TICK_TOP, MARK_COLOUR,
                                           mark.text) };
      if (!tick)
        return Forwarded(tick);
      ruler.append(*tick);
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](auto const& left, auto const& right)
                     { return left.first < right.first; });
    std::string timeline;
    for (auto const& [frame, row] : rows)
      timeline.append(row);

    std::string strips;
    Result<std::string> const changes{ Strip(trace.change) };
    if (!changes)
      return Forwarded(changes);
    strips.append(*changes);
    for (StripData const& watched : trace.watches)
    {
      Result<std::string> const strip{ Strip(watched) };
      if (!strip)
        return Forwarded(strip);
      strips.append(*strip);
    }

    std::string note{
      traced ? std::format("{} frames, {} s, {} verdicts, {} marks, "
                           "{} restores, {} resets",
                           trace.frames, Seconds(trace.seconds),
                           trace.verdicts.size(), trace.marks.size(),
                           trace.restores.size(), trace.resets.size())
             : std::string{ "this bundle has no trace.bin yet" } };
    bool const filmed{ std::filesystem::exists(bundle->Video()) };
    note.append(filmed ? "; a row seeks the video"
                       : "; there is no video.mkv to seek");

    std::string film;
    if (filmed)
    {
      Result<std::string> const shown{ Filled(page_template::FILM)
                                         .Fill("video", recorder::VIDEO_NAME)
                                         .Page() };
      if (!shown)
        return Forwarded(shown);
      film = *shown;
    }

    Result<std::string> const manifest_rows{ ManifestRows(
      manifest, traced ? std::optional<std::uint64_t>{ trace.kept }
                       : std::nullopt) };
    if (!manifest_rows)
      return Forwarded(manifest_rows);
    Result<std::string> const shots{
      Artifacts(bundle->Shots(), recorder::SHOTS_NAME, trace) };
    if (!shots)
      return Forwarded(shots);
    Result<std::string> const clips{
      Artifacts(bundle->Clips(), recorder::CLIPS_NAME, trace) };
    if (!clips)
      return Forwarded(clips);
    Result<std::string> const tails{ Tails(trace) };
    if (!tails)
      return Forwarded(tails);

    Result<std::string> const page{
      Filled(page_template::PAGE)
        .Fill("name", bundle_root.filename().string())
        .Fill("outcome", manifest.outcome.empty()
                           ? std::string{ "no run.yaml yet" }
                           : manifest.outcome)
        .FillHtml("manifest_rows", *manifest_rows)
        .FillHtml("film", std::move(film))
        .Fill("video_note", note)
        .Fill("film_stride", std::format("{}", FilmStride(manifest)))
        .Fill("width", std::format("{}", STRIP_WIDTH))
        .FillHtml("ruler", std::move(ruler))
        .FillHtml("timeline", std::move(timeline))
        .FillHtml("strips", std::move(strips))
        .FillHtml("shots", *shots)
        .FillHtml("clips", *clips)
        .FillHtml("tails", *tails)
        .Page() };
    if (!page)
      return Forwarded(page);

    std::filesystem::path const where{ bundle_root / REPORT_NAME };
    std::ofstream file{ where, std::ios::binary | std::ios::trunc };
    file << *page;
    if (!file)
      return Refused("report: cannot write {}", where.string());
    return where;
  }
}
