#pragma once
// The crop an image anchor matches against: written beside the tape as a PNG
// by the recorder's own writer, read back into the RGB565 perception takes.

#include "tash/bus/frame-descriptor.hpp"
#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tash::tape::detail::anchor_image
{
  using utilities::Result;

  inline constexpr std::string_view ANCHORS_SUFFIX{ ".anchors" };
  inline constexpr std::string_view IMAGE_SUFFIX{ ".png" };

  class AnchorImage
  {
  public:
    [[nodiscard]] static auto Read(std::filesystem::path const& from)
      -> Result<AnchorImage>;

    [[nodiscard]] auto View() const noexcept -> perception::Rgb565View;

    [[nodiscard]] auto Width() const noexcept -> std::uint32_t
    { return _width; }
    [[nodiscard]] auto Height() const noexcept -> std::uint32_t
    { return _height; }

  private:
    AnchorImage(std::vector<std::uint8_t> pixels, std::uint32_t width,
                std::uint32_t height);

    std::vector<std::uint8_t> _pixels;
    std::uint32_t             _width{ 0 };
    std::uint32_t             _height{ 0 };
  };

  // `demo.yaml` keeps its crops in `demo.anchors/`, one PNG per segment.
  [[nodiscard]] auto AnchorsDirectory(std::filesystem::path const& tape)
    -> std::filesystem::path;

  [[nodiscard]] auto AnchorImagePath(std::filesystem::path const& tape,
                                     std::string_view segment)
    -> std::filesystem::path;

  // An anchor's image is written relative to the tape, so the pair moves
  // together; this is where a player turns it back into a path to open.
  [[nodiscard]] auto ResolvedImage(std::filesystem::path const& tape,
                                   std::string_view image)
    -> std::filesystem::path;

  // Answers the relative path the anchor should carry.
  [[nodiscard]] auto WriteAnchorImage(bus::FrameView const& frame,
                                      perception::Region const& region,
                                      std::filesystem::path const& tape,
                                      std::string_view segment)
    -> Result<std::string>;
}

namespace tash::tape
{
  using detail::anchor_image::AnchorImage;
  using detail::anchor_image::AnchorImagePath;
  using detail::anchor_image::AnchorsDirectory;
  using detail::anchor_image::ResolvedImage;
  using detail::anchor_image::WriteAnchorImage;
}
