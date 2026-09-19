#pragma once
// The knobs a caller may turn: the codec's own, the depth and fullness
// policy of the queue before it, and the whole-pixel upscale a film wants.

#include <cstddef>
#include <cstdint>
#include <string>

namespace tash::recorder::detail::encoder_settings
{
  inline constexpr char const* DEFAULT_PRESET{ "veryfast" };

  inline constexpr int DEFAULT_CRF{ 23 };  // x264's own default

  // Half a second of frames: long enough to ride out a slow group of
  // pictures, short enough that a stalled encoder is seen as drops.
  inline constexpr std::size_t DEFAULT_QUEUE_DEPTH{ 32 };

  inline constexpr std::int64_t NANOSECONDS_PER_SECOND{ 1'000'000'000 };

  inline constexpr char const* FILM_PRESET{ "slow" };

  inline constexpr int FILM_CRF{ 16 };  // near-transparent at this size

  inline constexpr int FILM_KEYFRAME_SECONDS{ 2 };

  // A whole-pixel upscale to here leaves 4:2:0 chroma no one-pixel detail
  // to smear, which is what makes a 320-wide core watchable.
  inline constexpr std::uint32_t FILM_WIDTH_AT_LEAST{ 1280 };

  inline constexpr double FILM_SCANLINE_DARKENING{ 0.35 };  // towards black

  // A live producer drops rather than stall; a replay waits rather than lose.
  enum class WhenFull { DROP, WAIT };

  // A record is all keyframes so clips can be cut from it; a film is not.
  enum class Quality { RECORD, FILM };

  struct EncoderSettings
  {
    std::string   preset{ DEFAULT_PRESET };
    int           crf{ DEFAULT_CRF };
    bool          lossless{ false };
    std::size_t   queue_depth{ DEFAULT_QUEUE_DEPTH };
    WhenFull      when_full{ WhenFull::DROP };
    std::uint32_t scale{ 1 };
    int           keyframe_seconds{ 0 };  // 0: every frame its own group
    double        scanline_darkening{ 0.0 };  // 0: no scanlines
  };

  [[nodiscard]] inline constexpr auto FilmScaleFor(std::uint32_t width)
    -> std::uint32_t
  {
    return width == 0u ? 1u : (FILM_WIDTH_AT_LEAST + width - 1u) / width;
  }
}

namespace tash::recorder
{
  using detail::encoder_settings::DEFAULT_CRF;
  using detail::encoder_settings::DEFAULT_PRESET;
  using detail::encoder_settings::DEFAULT_QUEUE_DEPTH;
  using detail::encoder_settings::EncoderSettings;
  using detail::encoder_settings::FILM_CRF;
  using detail::encoder_settings::FILM_KEYFRAME_SECONDS;
  using detail::encoder_settings::FILM_PRESET;
  using detail::encoder_settings::FILM_SCANLINE_DARKENING;
  using detail::encoder_settings::FILM_WIDTH_AT_LEAST;
  using detail::encoder_settings::FilmScaleFor;
  using detail::encoder_settings::NANOSECONDS_PER_SECOND;
  using detail::encoder_settings::Quality;
  using detail::encoder_settings::WhenFull;
}
