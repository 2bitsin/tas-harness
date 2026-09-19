#include "tash/recorder/frame-png.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstring>
#include <exception>
#include <fstream>
#include <string>
#include <string_view>

namespace tash::recorder::detail::frame_png
{
  using utilities::Outcome;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    inline constexpr char const* PNG_SUFFIX{ ".png" };

    [[nodiscard]] auto Packed(bus::FrameView const& frame,
                              perception::Region const& region, cv::Mat& into)
      -> Outcome
    {
      int const height{ static_cast<int>(region.height) };
      int const width{ static_cast<int>(region.width) };
      into.create(height, width, CV_8UC2);
      std::size_t const row{ static_cast<std::size_t>(width)
                             * bus::BYTES_PER_PIXEL };
      for (int line{ 0 }; line < height; ++line)
      {
        std::size_t const start{
          (static_cast<std::size_t>(line) + region.y) * frame.descriptor.pitch
          + std::size_t{ region.x } * bus::BYTES_PER_PIXEL };
        if (start + row > frame.pixels.size())
          return Refused("the frame is {} bytes, short of {} rows of {}",
                         frame.pixels.size(), frame.descriptor.height,
                         frame.descriptor.pitch);
        std::memcpy(into.ptr(line), frame.pixels.data() + start, row);
      }
      return { };
    }

    [[nodiscard]] auto Checked(bus::FrameView const& frame,
                               perception::Region const& region,
                               std::string_view what) -> Outcome
    {
      if (frame.descriptor.width == 0u || frame.descriptor.height == 0u)
        return Refused("there is no frame to write to {}", what);
      if (!region.FitsIn(frame.descriptor.width, frame.descriptor.height))
        return Refused(
          "the region {}x{} at {},{} is not inside a {}x{} frame",
          region.width, region.height, region.x, region.y,
          frame.descriptor.width, frame.descriptor.height);
      return { };
    }

    [[nodiscard]] auto Encoded(bus::FrameView const& frame,
                               perception::Region const& region,
                               std::string_view what)
      -> Result<std::vector<std::byte>>
    {
      if (Outcome const sound{ Checked(frame, region, what) }; !sound)
        return utilities::Forwarded(sound);

      cv::Mat packed;
      if (Outcome const copied{ Packed(frame, region, packed) }; !copied)
        return utilities::Forwarded(copied);

      cv::Mat colour;
      cv::cvtColor(packed, colour, cv::COLOR_BGR5652BGR);
      std::vector<unsigned char> written;
      try
      {
        if (!cv::imencode(PNG_SUFFIX, colour, written))
          return Refused("cannot encode {}", what);
      }
      catch (std::exception const& failure)
      {
        return Refused("cannot encode {}: {}", what, failure.what());
      }

      std::vector<std::byte> bytes(written.size());
      std::memcpy(bytes.data(), written.data(), written.size());
      return bytes;
    }

    [[nodiscard]] auto WholeOf(bus::FrameView const& frame)
      -> perception::Region
    {
      return perception::Region{ 0u, 0u, frame.descriptor.width,
                                 frame.descriptor.height };
    }
  }

  auto PngBytes(bus::FrameView const& frame) -> Result<std::vector<std::byte>>
  {
    return PngBytes(frame, WholeOf(frame));
  }

  auto PngBytes(bus::FrameView const& frame, perception::Region const& region)
    -> Result<std::vector<std::byte>>
  {
    return Encoded(frame, region, "a png");
  }

  auto WritePng(bus::FrameView const& frame, std::filesystem::path const& to)
    -> Outcome
  {
    return WritePng(frame, WholeOf(frame), to);
  }

  auto ReadPng(std::filesystem::path const& from) -> Result<PngFrame>
  {
    cv::Mat colour;
    try
    {
      colour = cv::imread(from.string(), cv::IMREAD_COLOR);
    }
    catch (std::exception const& failure)
    {
      return Refused("cannot read {}: {}", from.string(), failure.what());
    }
    if (colour.empty())
      return Refused("cannot read {}", from.string());

    cv::Mat packed;
    cv::cvtColor(colour, packed, cv::COLOR_BGR2BGR565);

    PngFrame frame;
    frame.width = static_cast<std::uint32_t>(packed.cols);
    frame.height = static_cast<std::uint32_t>(packed.rows);
    frame.pixels.resize(static_cast<std::size_t>(packed.total())
                        * bus::BYTES_PER_PIXEL);
    std::memcpy(frame.pixels.data(), packed.data, frame.pixels.size());
    return frame;
  }

  auto WritePng(bus::FrameView const& frame, perception::Region const& region,
                std::filesystem::path const& to) -> Outcome
  {
    Result<std::vector<std::byte>> const bytes{
      Encoded(frame, region, to.string()) };
    if (!bytes)
      return utilities::Forwarded(bytes);

    std::ofstream writing{ to, std::ios::binary };
    writing.write(reinterpret_cast<char const*>(bytes->data()),
                  static_cast<std::streamsize>(bytes->size()));
    if (!writing)
      return Refused("cannot write {}", to.string());
    return { };
  }
}
