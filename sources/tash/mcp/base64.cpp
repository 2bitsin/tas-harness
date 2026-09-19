#include "tash/mcp/base64.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string_view>
#include <vector>

namespace tash::mcp::detail::base64
{
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view ALPHABET{
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
    inline constexpr char PADDING{ '=' };
    inline constexpr std::size_t GROUP{ 3 };
    inline constexpr unsigned SIX_BITS{ 0x3fu };
  }

  auto Base64Of(std::span<std::byte const> bytes) -> std::string
  {
    std::string encoded;
    encoded.reserve((bytes.size() + GROUP - 1) / GROUP * 4);
    for (std::size_t at{ 0 }; at < bytes.size(); at += GROUP)
    {
      std::size_t const left{ bytes.size() - at };
      std::uint32_t group{ 0 };
      for (std::size_t step{ 0 }; step < GROUP; ++step)
        group = (group << 8u)
          | (step < left
               ? static_cast<std::uint32_t>(bytes[at + step]) & 0xffu
               : 0u);

      for (std::size_t step{ 0 }; step < 4; ++step)
        encoded.push_back(step <= left
          ? ALPHABET[(group >> (18u - 6u * step)) & SIX_BITS]
          : PADDING);
    }
    return encoded;
  }

  auto Base64Of(std::filesystem::path const& file) -> Result<std::string>
  {
    std::ifstream reading{ file, std::ios::binary };
    if (!reading)
      return Refused("mcp: nothing to read at {}", file.string());
    std::vector<char> const held{ std::istreambuf_iterator<char>{ reading },
                                  std::istreambuf_iterator<char>{ } };
    return Base64Of(std::as_bytes(std::span{ held }));
  }
}
