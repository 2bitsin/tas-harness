#pragma once

#include "tash/utilities/outcome.hpp"

#include <filesystem>
#include <string>
#include <utility>

namespace tash::libretro::detail::shared_library
{
  using utilities::Result;

  class SharedLibrary
  {
  public:
    static auto Open(std::filesystem::path const& path)
      -> Result<SharedLibrary>;

    ~SharedLibrary();

    SharedLibrary(SharedLibrary&& other) noexcept
      : _handle{ std::exchange(other._handle, nullptr) },
        _path{ std::move(other._path) }
    {
    }

    auto operator=(SharedLibrary&& other) noexcept -> SharedLibrary&;

    SharedLibrary(SharedLibrary const&) = delete;
    auto operator=(SharedLibrary const&) -> SharedLibrary& = delete;

    [[nodiscard]] auto Symbol(std::string const& name) const -> Result<void*>;

    [[nodiscard]] auto Path() const noexcept -> std::filesystem::path const&
    { return _path; }

  private:
    SharedLibrary(void* handle, std::filesystem::path path)
      : _handle{ handle }, _path{ std::move(path) }
    {
    }

    void* _handle{ nullptr };
    std::filesystem::path _path;
  };
}

namespace tash::libretro
{
  using detail::shared_library::SharedLibrary;
}
