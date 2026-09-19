#pragma once
// The three numbers every frame carries into the trace: one exact digest and
// two perceptual ones.

#include "tash/perception/perceptual-hash.hpp"
#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>

namespace tash::perception::detail::frame_hash
{
  using utilities::Result;

  struct FrameHashes
  {
    std::uint64_t exact{};
    std::uint64_t difference{};
    std::uint64_t perceptual{};

    [[nodiscard]] constexpr auto operator==(FrameHashes const&) const noexcept
      -> bool = default;
  };

  // XXH3 over each row's visible bytes: no padding, no geometry, in the digest.
  [[nodiscard]] auto ExactHash(Rgb565View const& frame)
    -> Result<std::uint64_t>;

  [[nodiscard]] auto ExactHash(Rgb565View const& frame,
                               Region const& region) -> Result<std::uint64_t>;

  [[nodiscard]] auto HashesOf(Rgb565View const& frame) -> Result<FrameHashes>;

  [[nodiscard]] auto HashesOf(Rgb565View const& frame,
                              Region const& region) -> Result<FrameHashes>;

  [[nodiscard]] inline auto ExactHash(bus::FrameView const& frame)
    -> Result<std::uint64_t>
  {
    return ExactHash(ViewOf(frame));
  }

  [[nodiscard]] inline auto ExactHash(bus::FrameView const& frame,
                                      Region const& region)
    -> Result<std::uint64_t>
  {
    return ExactHash(ViewOf(frame), region);
  }

  [[nodiscard]] inline auto HashesOf(bus::FrameView const& frame)
    -> Result<FrameHashes>
  {
    return HashesOf(ViewOf(frame));
  }

  [[nodiscard]] inline auto HashesOf(bus::FrameView const& frame,
                                     Region const& region)
    -> Result<FrameHashes>
  {
    return HashesOf(ViewOf(frame), region);
  }
}

namespace tash::perception
{
  using detail::frame_hash::ExactHash;
  using detail::frame_hash::FrameHashes;
  using detail::frame_hash::HashesOf;
}
