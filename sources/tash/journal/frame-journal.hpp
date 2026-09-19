#pragma once

#include "tash/perception/hash-worker.hpp"
#include "tash/session/frame-observer.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace tash::journal::detail::frame_journal
{
  using utilities::Outcome;
  using utilities::Result;

  struct JournalCounts
  {
    std::uint64_t recorded{ 0 };

    // Frames whose pixels the hashes refused, which is the only way a record
    // goes missing: a full queue holds the step thread rather than losing one.
    std::uint64_t dropped{ 0 };

    auto operator==(JournalCounts const&) const -> bool = default;
  };

  // One frame record per frame, in frame order: the step thread only copies
  // pixels into the worker, the hashes and the change amount are its own.
  class FrameJournal : public session::FrameObserver
  {
  public:
    [[nodiscard]] static auto Open(
      std::filesystem::path const& path, std::string producer,
      std::size_t queue_capacity = perception::DEFAULT_QUEUE_CAPACITY)
      -> Result<std::unique_ptr<FrameJournal>>;

    FrameJournal(trace::Writer writer, std::size_t queue_capacity);
    ~FrameJournal() override;

    auto OnFrame(bus::FrameView const& frame, std::int64_t harness_time)
      -> void override;

    auto OnRewind(session::Rewind const& back) -> void override;

    // Drains the worker, writes what it was still holding and flushes.
    auto Close() -> Result<JournalCounts>;

    // The same drain and flush, without the close.
    [[nodiscard]] auto Flush() -> Outcome;

    [[nodiscard]] auto Counts() const noexcept -> JournalCounts
    { return _counts; }

    // A run writes its watch records into the same file, in the same thread.
    [[nodiscard]] auto WriterOf() noexcept -> trace::Writer*
    { return &_writer; }

  private:
    auto Collect() -> void;
    auto WriteHashed(perception::HashedFrame const& hashed) -> void;

    trace::Writer _writer;
    perception::HashWorker _worker;

    // The harness time of each frame submitted, oldest first: a result
    // carries its frame number back but not its instant.
    std::deque<std::pair<std::uint64_t, std::int64_t>> _submitted;

    JournalCounts _counts{};
    bool _closed{ false };
  };
}

namespace tash::journal
{
  using detail::frame_journal::FrameJournal;
  using detail::frame_journal::JournalCounts;
}
