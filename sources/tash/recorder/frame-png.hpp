#pragma once
// Screenshots, written from the raw slot: the whole frame, or one region of
// it, so a verdict can point at exactly what it looked at.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/perception/region.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tash::recorder::detail::frame_png
{
  using utilities::Outcome;
  using utilities::Result;

  // RGB565 rows packed tight, the way the bus carries a frame.
  struct PngFrame
  {
    std::vector<std::byte> pixels{ };
    std::uint32_t          width{ 0 };
    std::uint32_t          height{ 0 };
  };

  // The same encoder the file form uses, for a caller that wants the bytes.
  [[nodiscard]] auto PngBytes(bus::FrameView const& frame)
    -> Result<std::vector<std::byte>>;

  [[nodiscard]] auto PngBytes(bus::FrameView const& frame,
                              perception::Region const& region)
    -> Result<std::vector<std::byte>>;

  [[nodiscard]] auto WritePng(bus::FrameView const& frame,
                              std::filesystem::path const& to) -> Outcome;

  [[nodiscard]] auto WritePng(bus::FrameView const& frame,
                              perception::Region const& region,
                              std::filesystem::path const& to) -> Outcome;

  // What WritePng wrote, read back: the round trip through 8 bits a channel
  // is exact, so the hashes of the frame come back with it.
  [[nodiscard]] auto ReadPng(std::filesystem::path const& from)
    -> Result<PngFrame>;
}

namespace tash::recorder
{
  using detail::frame_png::PngBytes;
  using detail::frame_png::PngFrame;
  using detail::frame_png::ReadPng;
  using detail::frame_png::WritePng;
}
