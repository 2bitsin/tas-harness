#include "tash/recorder/bundle.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>
#include <format>
#include <system_error>

namespace tash::recorder::detail::bundle
{
  using utilities::Refused;

  auto Bundle::DirectoryName(std::string_view name,
                             std::chrono::sys_seconds started) -> std::string
  {
    // Dashes where ISO 8601 puts colons: a run directory has to be a legal
    // path component on every platform tash records on.
    return std::format("{:%Y-%m-%dT%H-%M-%S}Z-{}", started, name);
  }

  auto Bundle::Create(std::filesystem::path const& root, std::string_view name)
    -> Result<Bundle>
  {
    return Create(root, name,
                  std::chrono::floor<std::chrono::seconds>(
                    std::chrono::system_clock::now()));
  }

  auto Bundle::Create(std::filesystem::path const& root, std::string_view name,
                      std::chrono::sys_seconds started) -> Result<Bundle>
  {
    if (name.empty() || name.find('/') != std::string_view::npos)
      return Refused("recorder: '{}' is not one path component", name);

    std::filesystem::path const here{ root / DirectoryName(name, started) };
    std::error_code failed{ };
    std::filesystem::create_directories(here / SHOTS_NAME, failed);
    if (failed)
      return Refused("recorder: cannot create '{}': {}", here.string(),
                     failed.message());
    std::filesystem::create_directories(here / CLIPS_NAME, failed);
    if (failed)
      return Refused("recorder: cannot create '{}': {}",
                     (here / CLIPS_NAME).string(), failed.message());
    return Bundle{ here };
  }

  auto Bundle::At(std::filesystem::path root) -> Result<Bundle>
  {
    if (!std::filesystem::is_directory(root))
      return Refused("recorder: '{}' is not a bundle directory",
                     root.string());
    return Bundle{ std::move(root) };
  }

  auto Bundle::Write(RunManifest const& manifest) const -> Outcome
  {
    try
    {
      oxbox::serialization::SerializeTo(manifest, Manifest());
    }
    catch (std::exception const& failure)
    {
      return Refused("recorder: cannot write '{}': {}", Manifest().string(),
                     failure.what());
    }
    return { };
  }

  auto Bundle::Read() const -> Result<RunManifest>
  {
    if (!std::filesystem::is_regular_file(Manifest()))
      return Refused("recorder: no manifest at '{}'", Manifest().string());
    try
    {
      return oxbox::serialization::DeserializeFrom<RunManifest>(Manifest());
    }
    catch (std::exception const& failure)
    {
      return Refused("recorder: '{}' is not a run manifest: {}",
                     Manifest().string(), failure.what());
    }
  }
}
