#include "tash/plan/cost-grid.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace tash::plan::detail::cost_grid
{
  using utilities::Refused;

  namespace
  {
    struct Step
    {
      std::int32_t x{};
      std::int32_t y{};
    };

    constexpr std::array<Step, CORNERS> STEPS{
      Step{ 1, 0 },  Step{ -1, 0 }, Step{ 0, 1 },   Step{ 0, -1 },
      Step{ 1, 1 },  Step{ 1, -1 }, Step{ -1, 1 },  Step{ -1, -1 } };

    [[nodiscard]] auto LeastPassable(std::vector<std::uint8_t> const& costs)
      -> std::uint8_t
    {
      std::uint8_t least{ IMPASSABLE };
      for (std::uint8_t cost : costs)
        if (cost != IMPASSABLE)
          least = std::min(least, cost);
      return least == IMPASSABLE ? 0u : least;
    }
  }

  CostGrid::CostGrid(std::vector<std::uint8_t> costs, std::uint32_t width,
                     std::uint32_t height, bool diagonal)
  : _costs{ std::move(costs) }, _width{ width }, _height{ height },
    _diagonal{ diagonal }, _cheapest{ LeastPassable(_costs) }
  {
  }

  auto CostGrid::Of(std::span<std::uint8_t const> costs, std::uint32_t width,
                    std::uint32_t height, bool diagonal) -> Result<CostGrid>
  {
    if (width == 0u || height == 0u)
      return Refused("plan: a grid of {} by {} has no cells", width, height);
    std::size_t const wanted{ std::size_t{ width } * height };
    if (costs.size() != wanted)
      return Refused("plan: a {} by {} grid wants {} cost bytes, not {}",
                     width, height, wanted, costs.size());
    return CostGrid{ std::vector<std::uint8_t>{ costs.begin(), costs.end() },
                     width, height, diagonal };
  }

  auto CostGrid::Width() const noexcept -> std::uint32_t { return _width; }
  auto CostGrid::Height() const noexcept -> std::uint32_t { return _height; }
  auto CostGrid::Diagonal() const noexcept -> bool { return _diagonal; }

  auto CostGrid::Count() const noexcept -> std::size_t
  {
    return _costs.size();
  }

  auto CostGrid::Cheapest() const noexcept -> std::uint8_t
  {
    return _cheapest;
  }

  auto CostGrid::Holds(Cell cell) const noexcept -> bool
  {
    return cell.x < _width && cell.y < _height;
  }

  auto CostGrid::IndexOf(Cell cell) const noexcept -> std::size_t
  {
    return std::size_t{ cell.y } * _width + cell.x;
  }

  auto CostGrid::CellAt(std::size_t index) const noexcept -> Cell
  {
    return Cell{ static_cast<std::uint32_t>(index % _width),
                 static_cast<std::uint32_t>(index / _width) };
  }

  auto CostGrid::CostAt(std::size_t index) const noexcept -> std::uint8_t
  {
    return _costs[index];
  }

  auto CostGrid::Passable(std::size_t index) const noexcept -> bool
  {
    return _costs[index] != IMPASSABLE;
  }

  auto CostGrid::Around(std::size_t index) const -> std::vector<std::size_t>
  {
    Cell const from{ CellAt(index) };
    std::vector<std::size_t> found;
    found.reserve(_diagonal ? CORNERS : SIDES);
    for (Step const& step : std::span{ STEPS }.first(_diagonal ? CORNERS
                                                                : SIDES))
    {
      std::int64_t const x{ std::int64_t{ from.x } + step.x };
      std::int64_t const y{ std::int64_t{ from.y } + step.y };
      Cell const at{ static_cast<std::uint32_t>(x),
                     static_cast<std::uint32_t>(y) };
      if (x >= 0 && y >= 0 && Holds(at))
        found.push_back(IndexOf(at));
    }
    return found;
  }

  auto CostGrid::Steps(std::size_t from, std::size_t to) const noexcept
    -> std::uint64_t
  {
    Cell const here{ CellAt(from) };
    Cell const there{ CellAt(to) };
    std::uint64_t const across{ here.x < there.x ? there.x - here.x
                                                 : here.x - there.x };
    std::uint64_t const down{ here.y < there.y ? there.y - here.y
                                               : here.y - there.y };
    return _diagonal ? std::max(across, down) : across + down;
  }
}
