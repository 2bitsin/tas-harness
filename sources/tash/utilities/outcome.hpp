#pragma once
// The refusal grammar every module answers in: who refused, what it refused,
// then the value -- `session: no rom member in 'columns.zip'`.

#include <expected>
#include <format>
#include <string>
#include <utility>

namespace tash::utilities::detail::outcome
{
  using Outcome = std::expected<void, std::string>;

  template <typename Value>
  using Result = std::expected<Value, std::string>;

  // Returns the unexpected, so the one helper converts into both shapes.
  template <typename... Args>
  [[nodiscard]] auto Refused(std::format_string<Args...> shape,
                             Args&&... values) -> std::unexpected<std::string>
  {
    return std::unexpected{ std::format(shape, std::forward<Args>(values)...) };
  }

  // Forwards a refusal already worded; a caller must not add its own name.
  template <typename Value>
  [[nodiscard]] auto Forwarded(std::expected<Value, std::string> const& refused)
      -> std::unexpected<std::string>
  {
    return std::unexpected{ refused.error() };
  }
}

namespace tash::utilities
{
  using detail::outcome::Forwarded;
  using detail::outcome::Outcome;
  using detail::outcome::Refused;
  using detail::outcome::Result;
}
