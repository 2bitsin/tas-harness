#pragma once
// The watches seam, filled: what the run's sampler read this frame, by the
// names the profile gave them.

#include "tash/python/watch-values.hpp"
#include "tash/watches/sampler.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::python::detail::sampler_watches
{
  class SamplerWatches : public WatchValues
  {
  public:
    explicit SamplerWatches(watches::Sampler const& sampler);

    [[nodiscard]] auto Names() const -> std::vector<std::string> override;

    [[nodiscard]] auto Value(std::string_view name) const
      -> std::optional<std::int64_t> override;

  private:
    watches::Sampler const* _sampler;
  };
}

namespace tash::python
{
  using detail::sampler_watches::SamplerWatches;
}
