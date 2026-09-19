#include "tash/libretro/shared-library.hpp"

#include <dlfcn.h>

namespace tash::libretro::detail::shared_library
{
  using utilities::Result;
  using utilities::Refused;

  auto SharedLibrary::Open(std::filesystem::path const& path)
    -> Result<SharedLibrary>
  {
    // RTLD_LOCAL: two cores in one process must not share symbol names.
    void* const handle{ dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL) };
    if (handle == nullptr)
      return Refused("cannot load {}: {}", path.string(), dlerror());
    return SharedLibrary{ handle, path };
  }

  SharedLibrary::~SharedLibrary()
  {
    if (_handle != nullptr)
      dlclose(_handle);
  }

  auto SharedLibrary::operator=(SharedLibrary&& other) noexcept
    -> SharedLibrary&
  {
    if (this != &other)
    {
      if (_handle != nullptr)
        dlclose(_handle);
      _handle = std::exchange(other._handle, nullptr);
      _path = std::move(other._path);
    }
    return *this;
  }

  auto SharedLibrary::Symbol(std::string const& name) const -> Result<void*>
  {
    dlerror();
    void* const found{ dlsym(_handle, name.c_str()) };
    if (char const* const failure{ dlerror() })
      return Refused("{} has no symbol {}: {}", _path.string(), name, failure);
    return found;
  }
}
