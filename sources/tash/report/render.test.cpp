#include "tash/report/render.hpp"

#include "tash/report/_synthetic-bundle.hpp"
#include "tash/report/trace-digest.hpp"
#include "tash/trace/format.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace
{
  using tash::recorder::Bundle;
  using tash::report::RenderReport;
  using tash::report::detail::synthetic_bundle::SyntheticBundle;
  using tash::report::detail::synthetic_bundle::SyntheticParts;

  auto Rendered(std::filesystem::path const& root, SyntheticParts const& parts)
    -> std::string
  {
    auto const bundle{ Bundle::Create(root, "synthetic") };
    EXPECT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
    if (!bundle)
      return { };
    SyntheticBundle(*bundle, parts);

    auto const page{ RenderReport(bundle->Root()) };
    EXPECT_TRUE(page.has_value()) << (page ? "" : page.error());
    if (!page)
      return { };
    EXPECT_EQ(page->filename(), "report.html");

    std::ifstream file{ *page, std::ios::binary };
    std::ostringstream held;
    held << file.rdbuf();
    return held.str();
  }

  auto Matches(std::string const& page, std::regex const& shape)
    -> std::vector<std::string>
  {
    std::vector<std::string> found;
    for (std::sregex_iterator at{ page.begin(), page.end(), shape },
         end{ }; at != end; ++at)
      found.push_back((*at)[1].str());
    return found;
  }

  constexpr std::uint64_t REPORT_STRIDE{ 2 };

  auto Root(std::filesystem::path const& scratch) -> std::filesystem::path
  {
    for (auto const& entry : std::filesystem::directory_iterator{ scratch })
      if (entry.is_directory())
        return entry.path();
    return scratch;
  }
}

TEST(Report, EveryLinkThePageMakesResolvesInsideTheBundle)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-links") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(scratch->Path(), SyntheticParts{ }) };
  ASSERT_FALSE(page.empty());

  std::filesystem::path const root{ Root(scratch->Path()) };
  std::vector<std::string> const linked{
    Matches(page, std::regex{ R"RX((?:href|src)="([^"]+)")RX" }) };
  ASSERT_FALSE(linked.empty());
  for (std::string const& link : linked)
  {
    EXPECT_EQ(link.find("://"), std::string::npos) << link;
    EXPECT_TRUE(std::filesystem::exists(root / link)) << link;
  }
}

TEST(Report, ThePageCarriesOneRowPerVerdictAndMark)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-rows") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(scratch->Path(), SyntheticParts{ }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const rows{
    Matches(page, std::regex{ R"RX(<li class="(pass|fail|mark)")RX" }) };
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(std::count(rows.begin(), rows.end(), "pass"), 1);
  EXPECT_EQ(std::count(rows.begin(), rows.end(), "fail"), 1);
  EXPECT_EQ(std::count(rows.begin(), rows.end(), "mark"), 2);
}

TEST(Report, AGroupOfMarksIsOneRowPerFifty)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-groups") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(
    scratch->Path(), SyntheticParts{ "nothing untoward", true, 120 }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const rows{
    Matches(page, std::regex{ R"RX(<li class="mark group"[^>]*>(.*?)</li>)RX" })
  };
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_NE(rows[0].find(">decision</span> x50, frames 0 to 98"),
            std::string::npos) << rows[0];
  EXPECT_NE(rows[1].find(">decision</span> x50, frames 100 to 198"),
            std::string::npos) << rows[1];
  EXPECT_NE(rows[2].find(">decision</span> x20, frames 200 to 238"),
            std::string::npos) << rows[2];

  // The ungrouped marks are rows of their own, as they were.
  EXPECT_EQ(Matches(page, std::regex{ R"RX(<li class="(mark)")RX" }).size(),
            2u);
}

TEST(Report, AFailedVerdictBringsTheTraceTailWithIt)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-tail") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const failed{ Rendered(scratch->Path(), SyntheticParts{ }) };
  ASSERT_FALSE(failed.empty());

  std::vector<std::string> const rows{
    Matches(failed, std::regex{ R"RX(<td class="kind">([a-z]+)</td>)RX" }) };
  EXPECT_EQ(rows.size(), tash::report::TAIL_RECORDS);
  EXPECT_NE(failed.find("<details open>"), std::string::npos);

  auto const clean{ tash::utilities::ScratchAreaOf("report-clean") };
  ASSERT_TRUE(clean.has_value()) << (clean ? "" : clean.error());
  std::string const passed{
    Rendered(clean->Path(), SyntheticParts{ "all good", false }) };
  ASSERT_FALSE(passed.empty());
  EXPECT_EQ(passed.find("<details"), std::string::npos);
}

TEST(Report, AStripNeverPutsMorePointsInThePageThanItIsWide)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-strip") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(scratch->Path(), SyntheticParts{ }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const strips{
    Matches(page, std::regex{ R"RX(<polyline points="([^"]*)")RX" }) };
  ASSERT_EQ(strips.size(), 3u);
  for (std::string const& points : strips)
  {
    std::size_t const drawn{
      static_cast<std::size_t>(
        std::count(points.begin(), points.end(), ',')) };
    EXPECT_GT(drawn, 0u);
    EXPECT_LE(drawn, tash::report::STRIP_POINTS);
  }
}

TEST(Report, TextFromARunIsEscapedWhereverItLands)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-escape") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(
    scratch->Path(),
    SyntheticParts{ R"RX(score <b>"rose" & fell</b>)RX", true }) };
  ASSERT_FALSE(page.empty());

  EXPECT_NE(page.find("score &lt;b&gt;&quot;rose&quot; &amp; fell&lt;/b&gt;"),
            std::string::npos);
  EXPECT_EQ(page.find("<b>"), std::string::npos);
}

TEST(Report, AWatchThatNeverMovesIsDrawnThroughTheMiddle)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-flat") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(scratch->Path(), SyntheticParts{ }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const strips{
    Matches(page, std::regex{ R"RX(<polyline points="([^"]*)")RX" }) };
  ASSERT_EQ(strips.size(), 3u);
  std::vector<std::string> const heights{
    Matches(strips.back(), std::regex{ R"RX([0-9.]+,([0-9.]+))RX" }) };
  ASSERT_FALSE(heights.empty());
  for (std::string const& height : heights)
    EXPECT_EQ(height, "36.0");
}

TEST(Report, ARunYamlThatCannotBeReadIsARefusal)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-broken") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const bundle{ Bundle::Create(scratch->Path(), "synthetic") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  SyntheticBundle(*bundle, SyntheticParts{ });

  std::ofstream{ bundle->Manifest(), std::ios::binary | std::ios::trunc }
    << "core_name: [ not a manifest\n";
  auto const page{ RenderReport(bundle->Root()) };
  EXPECT_FALSE(page.has_value());
}

TEST(Report, TheTimeLineShowsEveryRestoreAndTheManifestWhatTheLineKept)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-restores") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(
    scratch->Path(), SyntheticParts{ "nothing untoward", true, 0, 3 }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const rows{
    Matches(page, std::regex{ R"RX(<li class="rewind"[^>]*>(.*?)</li>)RX" }) };
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_NE(rows[0].find("rewound to frame 95, "), std::string::npos)
    << rows[0];
  EXPECT_NE(rows[0].find(">trial 0</span>"), std::string::npos) << rows[0];
  EXPECT_NE(rows[2].find(">trial 2</span>"), std::string::npos) << rows[2];

  // Three trials of ten frames each, all folded back to frame 95.
  EXPECT_NE(page.find("<td>1500 recorded, 1475 kept</td>"),
            std::string::npos);
}

TEST(Report, ABundleTooOldToFoldIsRefusedWhenItRestored)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-old-fold") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  auto const bundle{ Bundle::Create(scratch->Path(), "synthetic") };
  ASSERT_TRUE(bundle.has_value()) << (bundle ? "" : bundle.error());
  SyntheticParts parts{ };
  parts.restores = 3;
  parts.format_version = tash::trace::EARLIEST_FOLDABLE_VERSION - 1;
  SyntheticBundle(*bundle, parts);

  auto const page{ RenderReport(bundle->Root()) };
  ASSERT_FALSE(page.has_value());
  EXPECT_NE(page.error().find("has to be run again"), std::string::npos)
    << page.error();
}

TEST(Report, ABundleTooOldToFoldIsReportedWhenNothingRestored)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-old-plain") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  SyntheticParts parts{ };
  parts.format_version = tash::trace::EARLIEST_FOLDABLE_VERSION - 1;
  std::string const page{ Rendered(scratch->Path(), parts) };
  ASSERT_FALSE(page.empty());
  EXPECT_NE(page.find("<td>1500 recorded, 1500 kept</td>"),
            std::string::npos);
}

TEST(Report, TheTimeLineShowsAResetAndTheLineKeepsOnlyWhatFollowedIt)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-reset") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(
    scratch->Path(), SyntheticParts{ "nothing untoward", true, 0, 0, true }) };
  ASSERT_FALSE(page.empty());

  std::vector<std::string> const rows{
    Matches(page, std::regex{ R"RX(<li class="reset"[^>]*>(.*?)</li>)RX" }) };
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_NE(rows[0].find(">reset</span> to power on"), std::string::npos)
    << rows[0];

  // Everything before frame 500 folded off, so a thousand frames are left.
  EXPECT_NE(page.find("<td>1500 recorded, 1000 kept</td>"),
            std::string::npos);
  EXPECT_NE(page.find("1 resets"), std::string::npos);
}

TEST(Report, AStrideTellsThePageWhatASeekDividesBy)
{
  auto const plain{ tash::utilities::ScratchAreaOf("report-no-stride") };
  ASSERT_TRUE(plain.has_value()) << (plain ? "" : plain.error());
  EXPECT_NE(Rendered(plain->Path(), SyntheticParts{ }).find("var stride = 1;"),
            std::string::npos);

  auto const scratch{ tash::utilities::ScratchAreaOf("report-stride") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  SyntheticParts parts{ };
  parts.video_stride = REPORT_STRIDE;
  std::string const page{ Rendered(scratch->Path(), parts) };
  ASSERT_FALSE(page.empty());

  EXPECT_NE(page.find("var stride = 2;"), std::string::npos);
  // 1500 frames one in two is 750 pictures, which at 59.9227 fps is 13 s.
  EXPECT_NE(page.find("<td>one frame in 2, no audio, 13 s film</td>"),
            std::string::npos);
}

TEST(Report, MoreThanFiftyRestoresCollapseOneRowPerFifty)
{
  auto const scratch{ tash::utilities::ScratchAreaOf("report-rewinds") };
  ASSERT_TRUE(scratch.has_value()) << (scratch ? "" : scratch.error());
  std::string const page{ Rendered(
    scratch->Path(), SyntheticParts{ "nothing untoward", true, 0, 60 }) };
  ASSERT_FALSE(page.empty());

  EXPECT_TRUE(Matches(page,
                      std::regex{ R"RX(<li class="rewind"[^>]*>)RX" }).empty());
  std::vector<std::string> const rows{ Matches(
    page,
    std::regex{ R"RX(<li class="rewind group"[^>]*>(.*?)</li>)RX" }) };
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_NE(rows[0].find(">rewound</span> x50, frames 100 to 590"),
            std::string::npos) << rows[0];
  EXPECT_NE(rows[1].find(">rewound</span> x10, frames 600 to 690"),
            std::string::npos) << rows[1];
}
