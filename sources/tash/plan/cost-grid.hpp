#pragma once
// A rectangular grid of per-cell entry costs, 4- or 8-neighbour: the model a
// route is planned over and distances are spread across.

#include "tash/utilities/outcome.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace tash::plan::detail::cost_grid
{
  using utilities::Result;

  // The cell value nothing enters; every other value is what entering costs.
  inline constexpr std::uint8_t IMPASSABLE{ 255u };

  // What a cell no route and no distance reaches answers.
  inline constexpr std::uint64_t UNREACHED{
    std::numeric_limits<std::uint64_t>::max() };

  inline constexpr std::size_t SIDES{ 4u };
  inline constexpr std::size_t CORNERS{ 8u };

  struct Cell
  {
    std::uint32_t x{};
    std::uint32_t y{};

    [[nodiscard]] constexpr auto operator==(Cell const&) const noexcept
      -> bool = default;
  };

  class CostGrid
  {
  public:
    // The costs are read row by row, so index i is cell (i % width, i / width).
    [[nodiscard]] static auto Of(std::span<std::uint8_t const> costs,
                                 std::uint32_t width, std::uint32_t height,
                                 bool diagonal) -> Result<CostGrid>;

    [[nodiscard]] auto Width() const noexcept -> std::uint32_t;
    [[nodiscard]] auto Height() const noexcept -> std::uint32_t;
    [[nodiscard]] auto Diagonal() const noexcept -> bool;
    [[nodiscard]] auto Count() const noexcept -> std::size_t;

    // The least a passable cell costs, which is what a heuristic may assume.
    [[nodiscard]] auto Cheapest() const noexcept -> std::uint8_t;

    [[nodiscard]] auto Holds(Cell cell) const noexcept -> bool;
    [[nodiscard]] auto IndexOf(Cell cell) const noexcept -> std::size_t;
    [[nodiscard]] auto CellAt(std::size_t index) const noexcept -> Cell;
    [[nodiscard]] auto CostAt(std::size_t index) const noexcept
      -> std::uint8_t;
    [[nodiscard]] auto Passable(std::size_t index) const noexcept -> bool;

    // The neighbours of that cell, as indices, impassable ones included.
    [[nodiscard]] auto Around(std::size_t index) const
      -> std::vector<std::size_t>;

    // Steps enough to reach that cell from this one, whatever the costs.
    [[nodiscard]] auto Steps(std::size_t from, std::size_t to) const noexcept
      -> std::uint64_t;

  private:
    CostGrid(std::vector<std::uint8_t> costs, std::uint32_t width,
             std::uint32_t height, bool diagonal);

    std::vector<std::uint8_t> _costs;
    std::uint32_t             _width{};
    std::uint32_t             _height{};
    bool                      _diagonal{};
    std::uint8_t              _cheapest{};
  };
}

namespace tash::plan
{
  using detail::cost_grid::Cell;
  using detail::cost_grid::CostGrid;
  using detail::cost_grid::IMPASSABLE;
  using detail::cost_grid::UNREACHED;
}
