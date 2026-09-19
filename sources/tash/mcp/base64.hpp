#pragma once
// Bytes as an image content block carries them: RFC 4648 4, padded, with no
// line breaks.

#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

namespace tash::mcp::detail::base64
{
  using utilities::Result;

  [[nodiscard]] auto Base64Of(std::span<std::byte const> bytes) -> std::string;

  [[nodiscard]] auto Base64Of(std::filesystem::path const& file)
    -> Result<std::string>;
}

namespace tash::mcp
{
  using detail::base64::Base64Of;
}
