#include "tash/session/restore-probe.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{
  using tash::session::FirstDifference;
  using tash::session::RestoreDifference;

  std::vector<std::uint64_t> const LINE{ 11u, 22u, 33u, 44u };
}

TEST(RestoreProbe, TwoLinesThatAgreeHaveNoDifference)
{
  EXPECT_EQ(FirstDifference(LINE, LINE),
            (RestoreDifference{ LINE.size(), std::nullopt }));
}

TEST(RestoreProbe, ALineNamesTheFrameItPartsAt)
{
  std::vector<std::uint64_t> parting{ LINE };
  parting[2] = 99u;
  EXPECT_EQ(FirstDifference(LINE, parting),
            (RestoreDifference{ LINE.size(), 3u }));

  parting[0] = 99u;
  EXPECT_EQ(FirstDifference(LINE, parting),
            (RestoreDifference{ LINE.size(), 1u }));
}

TEST(RestoreProbe, ALineThatRanOutPartsWhereItEnded)
{
  std::vector<std::uint64_t> const shorter{ LINE.begin(), LINE.end() - 1 };
  EXPECT_EQ(FirstDifference(LINE, shorter),
            (RestoreDifference{ shorter.size(), shorter.size() + 1u }));
  EXPECT_EQ(FirstDifference(shorter, LINE),
            (RestoreDifference{ shorter.size(), shorter.size() + 1u }));
}
