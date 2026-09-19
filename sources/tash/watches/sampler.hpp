#pragma once
// Every watch read once a frame, written to the trace only where the value
// moved, plus once at the first frame so a curve starts somewhere.

#include "tash/session/frame-observer.hpp"
#include "tash/trace/writer.hpp"
#include "tash/utilities/outcome.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/watch-set.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace tash::watches::detail::sampler
{
  using utilities::Outcome;
  using utilities::Result;
  using memory_map::MemoryMap;
  using watch_set::WatchSet;

  struct SamplerCounts
  {
    std::uint64_t written{ 0 };
    std::uint64_t refused{ 0 };

    auto operator==(SamplerCounts const&) const -> bool = default;
  };

  class Sampler : public session::FrameObserver
  {
  public:
    // A null writer samples without recording, which is what a predicate
    // reading Value() needs when no trace is open.
    [[nodiscard]] static auto Open(WatchSet watches, MemoryMap const& memory,
                                   trace::Writer* writer = nullptr)
      -> Result<std::unique_ptr<Sampler>>;

    Sampler(WatchSet watches,
            std::vector<std::span<std::byte const>> regions,
            trace::Writer* writer);

    auto OnFrame(bus::FrameView const& frame, std::int64_t harness_time)
      -> void override;

    auto OnRewind(session::Rewind const& back) -> void override;

    auto OnReset(session::Reset const& done) -> void override;

    auto Sample(std::uint64_t frame) -> Outcome;

    [[nodiscard]] auto Value(std::string_view name) const
      -> Result<std::int64_t>;
    [[nodiscard]] auto ValueAt(std::size_t index) const
      -> Result<std::int64_t>;

    [[nodiscard]] auto Watches() const noexcept -> WatchSet const&
    { return _watches; }
    [[nodiscard]] auto Counts() const noexcept -> SamplerCounts
    { return _counts; }
    [[nodiscard]] auto Seeded() const noexcept -> bool { return _seeded; }

  private:
    WatchSet _watches;
    std::vector<std::span<std::byte const>> _regions;
    trace::Writer* _writer;
    std::vector<std::int64_t> _values;
    SamplerCounts _counts{};
    bool _seeded{ false };
  };
}

namespace tash::watches
{
  using detail::sampler::Sampler;
  using detail::sampler::SamplerCounts;
}
