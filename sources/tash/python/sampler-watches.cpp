#include "tash/python/sampler-watches.hpp"

namespace tash::python::detail::sampler_watches
{
  SamplerWatches::SamplerWatches(watches::Sampler const& sampler)
  : _sampler{ &sampler }
  {
  }

  auto SamplerWatches::Names() const -> std::vector<std::string>
  {
    std::vector<std::string> named;
    for (watches::Watch const& watch : _sampler->Watches().All())
      named.push_back(watch.name);
    return named;
  }

  auto SamplerWatches::Value(std::string_view name) const
    -> std::optional<std::int64_t>
  {
    utilities::Result<std::int64_t> const read{
      _sampler->Value(name) };
    if (!read)
      return std::nullopt;
    return *read;
  }
}
