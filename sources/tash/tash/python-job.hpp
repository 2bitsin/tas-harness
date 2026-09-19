#pragma once
// The one detached python call: a worker thread owns the run while it lasts,
// and the endpoint's thread reads what it has printed without waiting on it.

#include "tash/utilities/outcome.hpp"

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace tash::cli::detail::python_job
{
  using utilities::Outcome;
  using utilities::Result;

  class PythonJob
  {
  public:
    using Work = std::function<Result<std::string>()>;

    PythonJob()                                    = default;
    ~PythonJob();

    PythonJob(PythonJob const&)                    = delete;
    auto operator = (PythonJob const&) -> PythonJob& = delete;

    // Refuses while an earlier worker is still there to be waited for.
    [[nodiscard]] auto Start(std::string name, Work work) -> Outcome;

    // Running until the work answers, Started until the worker is waited for.
    [[nodiscard]] auto Running() const noexcept -> bool
    { return _running.load(std::memory_order_acquire); }

    [[nodiscard]] auto Started() const noexcept -> bool
    { return _thread.joinable(); }

    [[nodiscard]] auto Name() const -> std::string;

    // All of it, or its last `tail` bytes; zero asks for all of it.
    [[nodiscard]] auto Printed(std::size_t tail) const -> std::string;

    // What the job wrote to stderr, cut the same way.
    [[nodiscard]] auto Errored(std::size_t tail) const -> std::string;

    // Empty while the work runs; what it answered, or how it failed, after.
    [[nodiscard]] auto Answer() const -> Result<std::string>;

    auto Print(std::string_view text) -> void;
    auto PrintError(std::string_view text) -> void;

    // Nothing to wait for is not a refusal.
    auto Wait() -> void;

  private:
    std::thread                _thread{ };
    std::atomic<bool>          _running{ false };

    mutable std::mutex         _guard{ };
    std::string                _name{ };
    std::string                _printed{ };
    std::string                _errored{ };
    Result<std::string>        _answer{ };
  };
}

namespace tash::cli
{
  using detail::python_job::PythonJob;
}
