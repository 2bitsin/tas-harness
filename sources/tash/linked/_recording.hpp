#pragma once
// What the library is while it records: the frame the target is in, the
// tape of what it did and the trace of what came of it.

#include "_byte-sink.hpp"
#include "_tape-writer.hpp"
#include "_trace-writer.hpp"
#include "tash/linked/tash.hpp"
#include "tash/utilities/outcome.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tash::linked::detail::recording
{
  using utilities::Result;

  // How often the tape and the manifest are written out again while the run
  // goes on: ten seconds at sixty frames, which is what a target that stops
  // dead loses.
  inline constexpr std::uint64_t REWRITE_FRAMES{ 600 };

  class Recording
  {
  public:
    [[nodiscard]] static auto InDirectory(harness::Options const& options,
                                          std::filesystem::path directory)
      -> Result<std::unique_ptr<Recording>>;

    [[nodiscard]] static auto InMemory(harness::Options const& options)
      -> std::unique_ptr<Recording>;

    ~Recording();

    auto Begin() -> void;
    auto Input(harness::Input const& input) -> void;
    auto Video(harness::Video const& video) -> void;
    auto Audio(harness::Audio const& audio) -> void;

    [[nodiscard]] auto Watch(harness::Watch const& watch)
      -> std::optional<std::size_t>;

    // Kept for the attached mode that calls them; a recording never goes
    // back, so nothing here asks.
    auto Keep(harness::State state) -> void;

    auto Event(std::string_view name, std::string_view text) -> void;

    // The step and the seed the target used, written down once, since a
    // recording drives neither of them (grilling 8).
    [[nodiscard]] auto Dt(double own) -> double;
    [[nodiscard]] auto Seed(std::uint64_t own) -> std::uint64_t;

    auto Flush() -> void;

    [[nodiscard]] auto Bytes(harness::Artifact which) const
      -> std::span<std::byte const>;

  private:
    struct Watched
    {
      void const*                   address{ nullptr };
      harness::Number               number{ harness::Number::I64 };
      std::function<std::int64_t()> reader{ };
    };

    Recording(harness::Options const& options, byte_sink::ByteSink tape,
              byte_sink::ByteSink trace, byte_sink::ByteSink manifest);

    auto Close() -> void;
    auto Decided(std::string_view text) -> void;
    auto Note(std::string_view text) const -> void;
    auto Write() -> void;
    [[nodiscard]] auto Elapsed() const -> std::int64_t;

    std::function<void(std::string_view)> _note;
    std::string                           _target;
    std::string                           _profile;
    double                                _fps;
    tape_writer::TapeWriter               _tape;
    byte_sink::ByteSink                   _tape_sink;
    trace_writer::TraceWriter             _trace;
    byte_sink::ByteSink                   _manifest;
    std::vector<Watched>                  _watches{ };
    std::vector<std::string>              _named{ };
    harness::State                        _state{ };
    std::chrono::steady_clock::time_point _started{
      std::chrono::steady_clock::now() };
    std::uint64_t                         _frame{ 0 };
    std::uint64_t                         _hash{ 0 };
    bool                                  _open{ false };
    bool                                  _said_keys{ false };
    bool                                  _said_dt{ false };
    bool                                  _said_seed{ false };
  };
}
