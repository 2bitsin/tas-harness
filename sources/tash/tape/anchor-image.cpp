#include "tash/tape/anchor-image.hpp"

#include "tash/recorder/frame-png.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstring>
#include <exception>
#include <format>
#include <system_error>
#include <utility>

namespace tash::tape::detail::anchor_image
{
  using utilities::Outcome;
  using utilities::Refused;

  AnchorImage::AnchorImage(std::vector<std::uint8_t> pixels,
                           std::uint32_t width, std::uint32_t height)
    : _pixels{ std::move(pixels) }, _width{ width }, _height{ height }
  { }

  auto AnchorImage::Read(std::filesystem::path const& from)
    -> Result<AnchorImage>
  {
    cv::Mat colour;
    try
    {
      colour = cv::imread(from.string(), cv::IMREAD_COLOR);
    }
    catch (std::exception const& failure)
    {
      return Refused("tape: cannot read '{}': {}", from.string(),
                     failure.what());
    }
    if (colour.empty())
      return Refused("tape: cannot read '{}' as an image", from.string());

    cv::Mat packed;
    cv::cvtColor(colour, packed, cv::COLOR_BGR2BGR565);

    auto const width{ static_cast<std::uint32_t>(packed.cols) };
    auto const height{ static_cast<std::uint32_t>(packed.rows) };
    std::size_t const row{ std::size_t{ width }
                           * perception::RGB565_BYTES_PER_PIXEL };
    std::vector<std::uint8_t> pixels(row * height);
    for (std::uint32_t line{ 0 }; line < height; ++line)
      std::memcpy(pixels.data() + row * line, packed.ptr(
                    static_cast<int>(line)), row);
    return AnchorImage{ std::move(pixels), width, height };
  }

  auto AnchorImage::View() const noexcept -> perception::Rgb565View
  {
    return perception::Rgb565View{
      _pixels, _width, _height,
      _width * perception::RGB565_BYTES_PER_PIXEL };
  }

  auto AnchorsDirectory(std::filesystem::path const& tape)
    -> std::filesystem::path
  {
    std::filesystem::path beside{ tape };
    beside.replace_extension(ANCHORS_SUFFIX);
    return beside;
  }

  auto AnchorImagePath(std::filesystem::path const& tape,
                       std::string_view segment) -> std::filesystem::path
  {
    return AnchorsDirectory(tape)
           / std::format("{}{}", segment, IMAGE_SUFFIX);
  }

  auto ResolvedImage(std::filesystem::path const& tape,
                     std::string_view image) -> std::filesystem::path
  {
    std::filesystem::path const named{ image };
    if (named.is_absolute())
      return named;
    return tape.parent_path() / named;
  }

  auto WriteAnchorImage(bus::FrameView const& frame,
                        perception::Region const& region,
                        std::filesystem::path const& tape,
                        std::string_view segment) -> Result<std::string>
  {
    std::filesystem::path const to{ AnchorImagePath(tape, segment) };
    std::error_code failed{ };
    std::filesystem::create_directories(to.parent_path(), failed);
    if (failed)
      return Refused("tape: cannot create '{}': {}",
                     to.parent_path().string(), failed.message());

    if (Outcome const written{ tash::recorder::WritePng(frame, region, to) };
        !written)
      return Refused("tape: {}", written.error());

    return std::format("{}/{}{}", AnchorsDirectory(tape).filename().string(),
                       segment, IMAGE_SUFFIX);
  }
}
