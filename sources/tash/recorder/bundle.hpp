#pragma once

#include "tash/recorder/run-manifest.hpp"
#include "tash/utilities/outcome.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

namespace tash::recorder::detail::bundle
{
  using utilities::Outcome;
  using utilities::Result;

  inline constexpr std::string_view VIDEO_NAME{ "video.mkv" };
  inline constexpr std::string_view TRACE_NAME{ "trace.bin" };
  inline constexpr std::string_view SHOTS_NAME{ "shots" };
  inline constexpr std::string_view CLIPS_NAME{ "clips" };
  inline constexpr std::string_view VERDICTS_NAME{ "verdicts.jsonl" };
  inline constexpr std::string_view TAPE_NAME{ "tape.yaml" };
  inline constexpr std::string_view MANIFEST_NAME{ "run.yaml" };

  // One directory per run, named for the moment it started, so a root sorts
  // chronologically and two runs of the same profile never collide.
  class Bundle
  {
  public:
    [[nodiscard]] static auto Create(std::filesystem::path const& root,
                                     std::string_view name) -> Result<Bundle>;

    [[nodiscard]] static auto Create(std::filesystem::path const& root,
                                     std::string_view name,
                                     std::chrono::sys_seconds started)
      -> Result<Bundle>;

    // A bundle a run already wrote, for the tools that read one back.
    [[nodiscard]] static auto At(std::filesystem::path root) -> Result<Bundle>;

    [[nodiscard]] static auto DirectoryName(std::string_view name,
                                            std::chrono::sys_seconds started)
      -> std::string;

    [[nodiscard]] auto Root() const -> std::filesystem::path const&
    { return _root; }

    [[nodiscard]] auto Video() const -> std::filesystem::path
    { return _root / VIDEO_NAME; }
    [[nodiscard]] auto Trace() const -> std::filesystem::path
    { return _root / TRACE_NAME; }
    [[nodiscard]] auto Shots() const -> std::filesystem::path
    { return _root / SHOTS_NAME; }
    [[nodiscard]] auto Clips() const -> std::filesystem::path
    { return _root / CLIPS_NAME; }
    [[nodiscard]] auto Verdicts() const -> std::filesystem::path
    { return _root / VERDICTS_NAME; }
    [[nodiscard]] auto Manifest() const -> std::filesystem::path
    { return _root / MANIFEST_NAME; }
    [[nodiscard]] auto Tape() const -> std::filesystem::path
    { return _root / TAPE_NAME; }

    [[nodiscard]] auto Write(RunManifest const& manifest) const -> Outcome;

    [[nodiscard]] auto Read() const -> Result<RunManifest>;

  private:
    explicit Bundle(std::filesystem::path root) : _root{ std::move(root) } { }

    std::filesystem::path _root;
  };
}

namespace tash::recorder
{
  using detail::bundle::Bundle;
  using detail::bundle::CLIPS_NAME;
  using detail::bundle::MANIFEST_NAME;
  using detail::bundle::SHOTS_NAME;
  using detail::bundle::TAPE_NAME;
  using detail::bundle::TRACE_NAME;
  using detail::bundle::VERDICTS_NAME;
  using detail::bundle::VIDEO_NAME;
}
