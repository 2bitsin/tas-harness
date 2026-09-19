#include "_byte-sink.hpp"

#include <format>
#include <ios>
#include <utility>

namespace tash::linked::detail::byte_sink
{
  using utilities::Refused;

  auto ByteSink::File(std::filesystem::path path) -> Result<ByteSink>
  {
    std::ofstream file{ path, std::ios::binary | std::ios::trunc };
    if (!file)
      return Refused("libtash: cannot create '{}'", path.string());
    return ByteSink{ std::move(path), std::move(file) };
  }

  auto ByteSink::Memory() -> ByteSink
  {
    return ByteSink{ std::filesystem::path{ }, std::ofstream{ } };
  }

  auto ByteSink::Append(std::span<std::byte const> bytes) -> void
  {
    if (!_file.is_open())
    {
      _kept.insert(_kept.end(), bytes.begin(), bytes.end());
      return;
    }
    _file.write(reinterpret_cast<char const*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!_file && _refusal.empty())
      _refusal = std::format("libtash: cannot write to '{}'", _path.string());
  }

  auto ByteSink::Replace(std::string_view text) -> void
  {
    if (!_file.is_open())
    {
      _kept.assign(reinterpret_cast<std::byte const*>(text.data()),
                   reinterpret_cast<std::byte const*>(text.data()
                                                      + text.size()));
      return;
    }
    _file.close();
    _file.open(_path, std::ios::binary | std::ios::trunc);
    _file.write(text.data(), static_cast<std::streamsize>(text.size()));
    _file.flush();
    if (!_file && _refusal.empty())
      _refusal = std::format("libtash: cannot write to '{}'", _path.string());
  }

  auto ByteSink::Flush() -> void
  {
    if (_file.is_open())
      _file.flush();
  }
}
