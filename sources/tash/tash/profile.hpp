#pragma once

#include "tash/session/session.hpp"
#include "tash/utilities/outcome.hpp"
#include "tash/watches/watch-set.hpp"
#include "tash/watches/watch-spec.hpp"

#include <cstdint>
#include <optional>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tash::cli::detail::profile
{
  using utilities::Result;

  // oxbox writes a std::filesystem::path member but does not read one
  // back, so a path in a profile is a string.
  struct RunProfile
  {
    friend constexpr auto reflect_scheme(RunProfile*);

    // What a run of this profile is called; absent, its own directory does.
    std::optional<std::string> name;
    std::string core;
    std::string rom;
    std::string system_dir;
    std::string save_dir;
    std::vector<std::string> ports;

    // Absent rather than empty when a profile follows nothing: a key a
    // reflected scheme does not find is a refusal unless it is optional.
    std::optional<std::vector<watches::WatchSpec>> watches;
  };

  // The yaml a profile is read from: the path itself, or the profile.yaml
  // in it when a profile's own directory is named.
  [[nodiscard]] auto ProfileFileAt(std::filesystem::path const& path)
    -> Result<std::filesystem::path>;

  [[nodiscard]] auto ProfileFrom(std::filesystem::path const& path)
    -> Result<RunProfile>;

  [[nodiscard]] auto DeviceOf(std::string_view named) -> Result<unsigned>;

  [[nodiscard]] auto SettingsFrom(RunProfile const& profile)
    -> Result<session::SessionSettings>;

  [[nodiscard]] auto WatchesFrom(RunProfile const& profile)
    -> Result<watches::WatchSet>;
}

namespace tash::cli
{
  using detail::profile::DeviceOf;
  using detail::profile::ProfileFileAt;
  using detail::profile::ProfileFrom;
  using detail::profile::RunProfile;
  using detail::profile::SettingsFrom;
  using detail::profile::WatchesFrom;
}
