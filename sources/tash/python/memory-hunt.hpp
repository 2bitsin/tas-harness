#pragma once
// The classic hunt from inside a scenario: one libretro region, narrowed a
// step at a time, with the region's bytes never leaving the process.

#include "tash/utilities/outcome.hpp"
#include "tash/watches/memory-map.hpp"
#include "tash/watches/memory-search.hpp"
#include "tash/watches/number-format.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::python::detail::memory_hunt
{
  using utilities::Result;

  inline constexpr std::size_t CANDIDATE_LIMIT{ 20u };

  class MemoryHunt
  {
  public:
    [[nodiscard]] static auto Of(watches::MemoryMap const& memory,
                                 std::string region,
                                 watches::NumberFormat format,
                                 std::uint32_t stride) -> Result<MemoryHunt>;

    // The first step seeds the region; each one after it narrows.
    [[nodiscard]] auto Step(std::string_view how,
                            std::optional<std::int64_t> value)
      -> Result<std::size_t>;

    // The survivors as the step that kept them read them.
    [[nodiscard]] auto Candidates(std::size_t limit) const
      -> std::vector<watches::Candidate>;

    [[nodiscard]] auto Count() const noexcept -> std::size_t;
    [[nodiscard]] auto Seeded() const noexcept -> bool;
    auto Reset() -> void;

    [[nodiscard]] auto Region() const noexcept -> std::string const&;
    [[nodiscard]] auto Format() const noexcept -> watches::NumberFormat;
    [[nodiscard]] auto Stride() const noexcept -> std::uint32_t;

  private:
    MemoryHunt(watches::MemoryMap region, watches::NumberFormat format,
               std::uint32_t stride);

    watches::MemoryMap    _region;
    watches::NumberFormat _format;
    std::uint32_t         _stride;
    watches::MemorySearch _search;
  };
}

namespace tash::python
{
  using detail::memory_hunt::CANDIDATE_LIMIT;
  using detail::memory_hunt::MemoryHunt;
}
