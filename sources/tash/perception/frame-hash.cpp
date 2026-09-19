#include "tash/perception/frame-hash.hpp"

#include "_checked-view.hpp"

#include <xxhash.h>

#include <memory>

namespace tash::perception::detail::frame_hash
{
  using utilities::Result;
  using utilities::Refused;
  using utilities::Forwarded;

  namespace
  {
    using detail::checked_view::Checked;
    using detail::checked_view::Cropped;

    struct StateDeleter
    {
      auto operator()(XXH3_state_t* state) const noexcept -> void
      {
        XXH3_freeState(state);
      }
    };
  }

  auto ExactHash(Rgb565View const& frame) -> Result<std::uint64_t>
  {
    auto const checked{ Checked(frame) };
    if (!checked)
      return Forwarded(checked);
    if (frame.pitch == frame.RowBytes())
      return XXH3_64bits(frame.bytes.data(),
                         std::size_t{ frame.pitch } * frame.height);

    std::unique_ptr<XXH3_state_t, StateDeleter> const state{
      XXH3_createState() };
    if (state == nullptr || XXH3_64bits_reset(state.get()) == XXH_ERROR)
      return Refused("perception: xxh3 state could not be started");
    for (std::uint32_t y{ 0 }; y < frame.height; ++y)
    {
      auto const row{ frame.Row(y) };
      XXH3_64bits_update(state.get(), row.data(), row.size());
    }
    return XXH3_64bits_digest(state.get());
  }

  auto ExactHash(Rgb565View const& frame, Region const& region)
    -> Result<std::uint64_t>
  {
    auto const window{ Cropped(frame, region) };
    if (!window)
      return Forwarded(window);
    return ExactHash(*window);
  }

  auto HashesOf(Rgb565View const& frame) -> Result<FrameHashes>
  {
    auto const exact{ ExactHash(frame) };
    if (!exact)
      return Forwarded(exact);
    auto const difference{ DifferenceHash(frame) };
    if (!difference)
      return Forwarded(difference);
    auto const perceptual{ PerceptualHash(frame) };
    if (!perceptual)
      return Forwarded(perceptual);
    return FrameHashes{ *exact, *difference, *perceptual };
  }

  auto HashesOf(Rgb565View const& frame, Region const& region)
    -> Result<FrameHashes>
  {
    auto const window{ Cropped(frame, region) };
    if (!window)
      return Forwarded(window);
    return HashesOf(*window);
  }
}
