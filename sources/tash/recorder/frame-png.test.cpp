#include "tash/recorder/frame-png.hpp"

#include "tash/recorder/_test-frame.hpp"
#include "tash/utilities/scratch-area.hpp"

#include <gtest/gtest.h>
#include <opencv2/imgcodecs.hpp>

#include <cstdint>

namespace tash::recorder
{
  namespace
  {
    using testing::TestFrame;

    constexpr std::uint32_t WIDTH{ 96 };
    constexpr std::uint32_t HEIGHT{ 64 };
    constexpr std::uint32_t PADDING_BYTES{ 12 };
  }

  TEST(RecorderFramePng, TheWholeFrameKeepsItsSize)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-shot") };
    ASSERT_TRUE(area.has_value()) << area.error();
    auto const path{ area->File("whole.png") };

    TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
    frame.Paint(0u);
    ASSERT_TRUE(WritePng(frame.View(), path).has_value());

    cv::Mat const read{ cv::imread(path.string(), cv::IMREAD_COLOR) };
    EXPECT_EQ(read.cols, static_cast<int>(WIDTH));
    EXPECT_EQ(read.rows, static_cast<int>(HEIGHT));
  }

  TEST(RecorderFramePng, ACropIsTheSizeOfItsRegion)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-crop") };
    ASSERT_TRUE(area.has_value()) << area.error();
    auto const path{ area->File("crop.png") };

    TestFrame frame{ WIDTH, HEIGHT, PADDING_BYTES };
    frame.Paint(3u);
    perception::Region const region{ 8u, 4u, 32u, 16u };
    ASSERT_TRUE(WritePng(frame.View(), region, path).has_value());

    cv::Mat const read{ cv::imread(path.string(), cv::IMREAD_COLOR) };
    EXPECT_EQ(read.cols, static_cast<int>(region.width));
    EXPECT_EQ(read.rows, static_cast<int>(region.height));
  }

  TEST(RecorderFramePng, ARegionOffTheFrameIsRefused)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-off") };
    ASSERT_TRUE(area.has_value()) << area.error();

    TestFrame frame{ WIDTH, HEIGHT };
    frame.Paint(0u);
    auto const written{ WritePng(frame.View(),
                                 perception::Region{ WIDTH - 4u, 0u, 16u, 4u },
                                 area->File("off.png")) };
    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("not inside"), std::string::npos)
      << written.error();
  }

  TEST(RecorderFramePng, AFrameWithNoPixelsIsRefused)
  {
    auto const area{ utilities::ScratchAreaOf("recorder-none") };
    ASSERT_TRUE(area.has_value()) << area.error();

    TestFrame const frame{ 0u, 0u };
    EXPECT_FALSE(WritePng(frame.View(), area->File("none.png")).has_value());
  }
}
