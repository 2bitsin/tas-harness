"""Decode Columns' well and the column falling into it out of work ram."""

import dataclasses

# Genesis Plus GX hands work ram byte-swapped, so these index that buffer the
# way the profile's watches do; README.md is the hunt that found them.
RAM_BYTES = 0x10000
ROWS = 13
COLUMNS = 6
WELL_BASE = 0xc8c8
ROW_BYTES = 12
CELL_BYTES = 2
EMPTY = 0
LANE = 0xc87d
FALL_Y = 0x8818
SPAWN_Y = 151
ROW_PIXELS = 16
JEWEL_TILES = 0xc884
TILE_STRIDE = 10
FIRST_TILE = 0x16
TILES_PER_JEWEL = 4


@dataclasses.dataclass(frozen=True)
class Falling:
    """The column in flight: its lane, the row its foot is in, its jewels."""

    x: int
    y: int
    ids: tuple[int, int, int]


@dataclasses.dataclass(frozen=True)
class Well:
    """Thirteen rows of six cells, top row first, and what is falling in."""

    rows: tuple[tuple[int, ...], ...]
    falling: Falling | None

    def __str__(self) -> str:
        return "\n".join(
            "".join("." if cell == EMPTY else str(cell) for cell in row)
            for row in self.rows)


def _falling(data: bytes) -> Falling | None:
    pixel = data[FALL_Y] | (data[FALL_Y + 1] << 8)
    if pixel == 0:
        return None
    ids = tuple((data[JEWEL_TILES + step * TILE_STRIDE] - FIRST_TILE)
                // TILES_PER_JEWEL + 1 for step in range(3))
    return Falling(data[LANE], (pixel - SPAWN_Y) // ROW_PIXELS, ids)


def decode(data: bytes) -> Well:
    """The well and the falling column, from the whole system memory region."""
    if len(data) < RAM_BYTES:
        raise ValueError(f"columns: the system region is {RAM_BYTES} bytes,"
                         f" not {len(data)}")
    rows = tuple(tuple(data[WELL_BASE + row * ROW_BYTES + cell * CELL_BYTES]
                       for cell in range(COLUMNS))
                 for row in range(ROWS))
    return Well(rows, _falling(data))
