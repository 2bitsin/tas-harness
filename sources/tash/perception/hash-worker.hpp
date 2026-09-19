#pragma once
// Hashing off the step thread: one thread, a bounded queue of pixel copies,
// results pulled by the caller so nothing runs on the worker's stack.

#include "tash/perception/change-amount.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/utilities/outcome.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace tash::perception::detail::hash_worker
{
  using utilities::Outcome;
  using utilities::Result;

  inline constexpr std::size_t DEFAULT_QUEUE_CAPACITY{ 8u };

  struct HashedFrame
  {
    std::uint64_t       frame{};
    Result<FrameHashes> hashes{};

    // Against the frame submitted before it; zero for the first, and for a
    // frame whose geometry the one before it does not share.
    ChangeAmount        change{};
  };

  class HashWorker
  {
  public:
    explicit HashWorker(std::size_t capacity = DEFAULT_QUEUE_CAPACITY);
    ~HashWorker();

    HashWorker(HashWorker const&)                    = delete;
    auto operator=(HashWorker const&) -> HashWorker& = delete;

    // Copies the visible pixels and waits for room rather than dropping the
    // frame: a lost hash is a hole in the trace no later pass can fill.
    auto Submit(std::uint64_t frame_number, Rgb565View const& frame) -> Outcome;

    // For a caller that would rather lose a frame than stall the step.
    [[nodiscard]] auto TrySubmit(std::uint64_t frame_number,
                                 Rgb565View const& frame) -> Result<bool>;

    auto Submit(bus::FrameView const& frame) -> Outcome
    {
      return Submit(frame.descriptor.number, ViewOf(frame));
    }

    [[nodiscard]] auto TrySubmit(bus::FrameView const& frame) -> Result<bool>
    {
      return TrySubmit(frame.descriptor.number, ViewOf(frame));
    }

    auto Drain() -> void;

    // Empty only once nothing is queued and nothing is being hashed.
    [[nodiscard]] auto Take() -> std::optional<HashedFrame>;

    [[nodiscard]] auto TryTake() -> std::optional<HashedFrame>;

    [[nodiscard]] auto Pending() const -> std::size_t;
    [[nodiscard]] auto Ready() const -> std::size_t;
    [[nodiscard]] auto Capacity() const noexcept -> std::size_t;

  private:
    struct Job
    {
      std::uint64_t             frame{};
      std::uint32_t             width{};
      std::uint32_t             height{};
      std::vector<std::uint8_t> pixels{};
    };

    [[nodiscard]] static auto Viewed(Job const& job) noexcept -> Rgb565View;

    auto Work() -> void;

    mutable std::mutex      _mutex;
    std::condition_variable _room;
    std::condition_variable _arrival;
    std::condition_variable _progress;
    std::deque<Job>         _jobs;
    std::deque<HashedFrame> _results;
    std::size_t             _capacity;
    std::size_t             _in_flight{ 0u };

    // Work() alone reads or writes this one: what a change amount measures
    // from, which is the frame submitted before the one being hashed.
    Job                     _earlier{};

    bool                    _stopping{ false };
    std::thread             _thread;
  };
}

namespace tash::perception
{
  using detail::hash_worker::DEFAULT_QUEUE_CAPACITY;
  using detail::hash_worker::HashedFrame;
  using detail::hash_worker::HashWorker;
}
