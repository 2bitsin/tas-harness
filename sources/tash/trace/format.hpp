#pragma once

// Little endian throughout: a header, then records, each a kind byte, a u32
// payload size and that many bytes, so a reader walks past a kind it lacks.

#include <array>
#include <cstddef>
#include <cstdint>

namespace tash::trace::detail::format
{
  inline constexpr std::array<char, 8> MAGIC{
    'T', 'A', 'S', 'H', 'T', 'R', 'C', 'E' };

  // 4 added resets; 5 made a restore's `to` line frames, not harness frames
  inline constexpr std::uint16_t FORMAT_VERSION{ 5 };
  inline constexpr std::uint16_t EARLIEST_FOLDABLE_VERSION{ 5 };

  // magic, format version, header size, creation time, producer size
  inline constexpr std::size_t FIXED_HEADER_SIZE{ 8 + 2 + 4 + 8 + 4 };

  inline constexpr std::size_t RECORD_FRAME_SIZE{ 1 + 4 };

  // Sixty records is a second of frames, so a lost process costs a second.
  inline constexpr std::size_t DEFAULT_FLUSH_INTERVAL{ 60 };

  // What stops a size read out of a corrupt tail becoming an allocation.
  inline constexpr std::uint32_t MAXIMUM_PAYLOAD_SIZE{ 1u << 20 };

  enum class Kind : std::uint8_t
  {
    FRAME    = 1,
    INPUT    = 2,
    WATCH    = 3,
    DECISION = 4,
    VERDICT  = 5,
    TRIGGER  = 6,
    MARK     = 7,
    RESTORE  = 8,
    RESET    = 9,
  };
}

namespace tash::trace
{
  using detail::format::DEFAULT_FLUSH_INTERVAL;
  using detail::format::EARLIEST_FOLDABLE_VERSION;
  using detail::format::FIXED_HEADER_SIZE;
  using detail::format::FORMAT_VERSION;
  using detail::format::Kind;
  using detail::format::MAGIC;
  using detail::format::MAXIMUM_PAYLOAD_SIZE;
  using detail::format::RECORD_FRAME_SIZE;
}
