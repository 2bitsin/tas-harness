#include "tash/watches/sampler.hpp"

#include "tash/watches/watch.hpp"

#include <utility>

namespace tash::watches::detail::sampler
{
  using utilities::Forwarded;
  using utilities::Outcome;
  using utilities::Result;

  auto Sampler::Open(WatchSet watches, MemoryMap const& memory,
                     trace::Writer* writer)
    -> Result<std::unique_ptr<Sampler>>
  {
    std::vector<std::span<std::byte const>> regions;
    regions.reserve(watches.Count());
    for (watch::Watch const& definition : watches.All())
    {
      Result<std::span<std::byte const>> const area{
        memory.Area(definition.region) };
      if (!area)
        return Forwarded(area);
      // Reading once here turns an address off the end into an answer at
      // the profile, not a refusal on every frame of the run.
      if (Result<std::int64_t> const read{ watch::ReadWatch(definition,
                                                            *area) };
          !read)
        return Forwarded(read);
      regions.push_back(*area);
    }
    return std::make_unique<Sampler>(std::move(watches), std::move(regions),
                                     writer);
  }

  Sampler::Sampler(WatchSet watches,
                   std::vector<std::span<std::byte const>> regions,
                   trace::Writer* writer)
  : _watches{ std::move(watches) }, _regions{ std::move(regions) },
    _writer{ writer }, _values(_watches.Count())
  {
  }

  auto Sampler::OnFrame(bus::FrameView const& frame, std::int64_t) -> void
  {
    static_cast<void>(Sample(frame.descriptor.number));
  }

  auto Sampler::OnRewind(session::Rewind const& back) -> void
  {
    // A restore rewinds the ram without producing a frame, so what the
    // watches last read is a value the run no longer stands at. A seed puts
    // the run on a line of frames it never ran: every value is written
    // again, at that line's own last frame, because the records before it
    // are off the line and nothing after it need ever move.
    if (!back.seed)
    {
      static_cast<void>(Sample(back.at));
      return;
    }
    _seeded = false;
    static_cast<void>(Sample(back.seed->at));
  }

  auto Sampler::OnReset(session::Reset const& done) -> void
  {
    // A reset takes every record written so far off the line, so the values
    // are all written again rather than only the ones the reset moved.
    _seeded = false;
    static_cast<void>(Sample(done.at));
  }

  auto Sampler::Sample(std::uint64_t frame) -> Outcome
  {
    for (std::size_t index{ 0 }; index < _watches.Count(); ++index)
    {
      Result<std::int64_t> const value{ watch::ReadWatch(
        _watches.All()[index], _regions[index]) };
      if (!value)
      {
        ++_counts.refused;
        return Forwarded(value);
      }
      if (_seeded && *value == _values[index])
        continue;

      _values[index] = *value;
      if (_writer != nullptr)
      {
        static_cast<void>(_writer->Write(trace::WatchRecord{
          frame, static_cast<std::uint32_t>(index), *value }));
        ++_counts.written;
      }
    }
    _seeded = true;
    return {};
  }

  auto Sampler::Value(std::string_view name) const -> Result<std::int64_t>
  {
    Result<std::size_t> const index{ _watches.IndexOf(name) };
    if (!index)
      return Forwarded(index);
    return ValueAt(*index);
  }

  auto Sampler::ValueAt(std::size_t index) const -> Result<std::int64_t>
  {
    if (index >= _values.size())
      return utilities::Refused("watches: no watch {} among {}", index,
                                _values.size());
    if (!_seeded)
      return utilities::Refused("watches: no frame has been sampled yet");
    return _values[index];
  }
}
