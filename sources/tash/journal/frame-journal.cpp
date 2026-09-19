#include "tash/journal/frame-journal.hpp"

#include "tash/perception/change-amount.hpp"

#include <optional>
#include <utility>

namespace tash::journal::detail::frame_journal
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Result;

  auto FrameJournal::Open(std::filesystem::path const& path,
                          std::string producer, std::size_t queue_capacity)
    -> Result<std::unique_ptr<FrameJournal>>
  {
    Result<trace::Writer> writer{
      trace::Writer::Open(path, std::move(producer)) };
    if (!writer)
      return Forwarded(writer);
    return std::make_unique<FrameJournal>(std::move(*writer), queue_capacity);
  }

  FrameJournal::FrameJournal(trace::Writer writer, std::size_t queue_capacity)
  : _writer{ std::move(writer) }, _worker{ queue_capacity }
  {
  }

  FrameJournal::~FrameJournal()
  {
    if (!_closed)
      static_cast<void>(Close());
  }

  auto FrameJournal::OnFrame(bus::FrameView const& frame,
                             std::int64_t harness_time) -> void
  {
    // Blocking: a trace with a hole in it is worth less than a run that
    // waits, so back pressure paces the core when hashing is the slower side.
    if (Outcome const submitted{ _worker.Submit(frame) }; submitted)
      _submitted.emplace_back(frame.descriptor.number, harness_time);
    else
      ++_counts.dropped;
    Collect();
  }

  // The line a seeding restore brought ends at a frame this run never ran,
  // and a replay checks itself against what the trace says the line was.
  auto FrameJournal::OnRewind(session::Rewind const& back) -> void
  {
    if (!back.seed)
      return;
    if (Outcome const submitted{ _worker.Submit(back.seed->at,
                                                perception::ViewOf(
                                                  back.seed->frame)) };
        submitted)
      _submitted.emplace_back(back.seed->at, back.seed->harness_time);
    else
      ++_counts.dropped;
    Collect();
  }

  auto FrameJournal::Close() -> Result<JournalCounts>
  {
    if (_closed)
      return _counts;
    _closed = true;

    _worker.Drain();
    Collect();
    if (Outcome const flushed{ _writer.Flush() }; !flushed)
      return Forwarded(flushed);
    if (Outcome const status{ _writer.Status() }; !status)
      return Forwarded(status);
    return _counts;
  }

  auto FrameJournal::Flush() -> Outcome
  {
    if (_closed)
      return { };
    _worker.Drain();
    Collect();
    return _writer.Flush();
  }

  auto FrameJournal::Collect() -> void
  {
    while (std::optional<perception::HashedFrame> const hashed{
             _worker.TryTake() })
      WriteHashed(*hashed);
  }

  auto FrameJournal::WriteHashed(perception::HashedFrame const& hashed) -> void
  {
    // One worker thread taking jobs in order answers in order, so the
    // oldest frame still submitted is this one.
    std::int64_t harness_time{ 0 };
    if (!_submitted.empty())
    {
      harness_time = _submitted.front().second;
      _submitted.pop_front();
    }

    if (!hashed.hashes)
    {
      ++_counts.dropped;
      return;
    }

    static_cast<void>(_writer.Write(trace::FrameRecord{
      hashed.frame, harness_time, hashed.hashes->exact,
      hashed.hashes->difference, hashed.hashes->perceptual,
      hashed.change.changed_ratio }));
    ++_counts.recorded;
  }
}
