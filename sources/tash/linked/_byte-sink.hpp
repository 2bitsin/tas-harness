#pragma once
// Where one artifact's bytes go: a file, where the host has a filesystem,
// or a buffer the target fetches and saves its own way, where it has not.

#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::linked::detail::byte_sink
{
  using utilities::Result;

  class ByteSink
  {
  public:
    [[nodiscard]] static auto File(std::filesystem::path path)
      -> Result<ByteSink>;

    [[nodiscard]] static auto Memory() -> ByteSink;

    auto Append(std::span<std::byte const> bytes) -> void;

    // The whole of a text artifact, as it stands: written again every time
    // it grows, so a target that stops dead leaves the last whole one.
    auto Replace(std::string_view text) -> void;

    auto Flush() -> void;

    [[nodiscard]] auto Bytes() const noexcept -> std::span<std::byte const>
    { return _kept; }

    [[nodiscard]] auto Refusal() const noexcept -> std::string_view
    { return _refusal; }

  private:
    ByteSink(std::filesystem::path path, std::ofstream file)
      : _path{ std::move(path) }, _file{ std::move(file) }
    { }

    std::filesystem::path  _path{ };
    std::ofstream          _file{ };
    std::vector<std::byte> _kept{ };
    std::string            _refusal{ };
  };
}
