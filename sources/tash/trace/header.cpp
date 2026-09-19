#include "tash/trace/header.hpp"

#include <chrono>
#include <utility>

namespace tash::trace::detail::header
{
  auto Header::Now(std::string producer) -> Header
  {
    auto const since_epoch{
      std::chrono::system_clock::now().time_since_epoch() };
    return Header{
      format::FORMAT_VERSION,
      std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch)
        .count(),
      std::move(producer) };
  }
}
