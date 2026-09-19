"""Dune's state: the map array, the entity tables, the purses and the words
beside them, all read from work ram in one call."""

WORK_RAM = 0x10000

MAP_BASE = 0x7d9c
MAP_ROW = 0x100
MAP_TILE = 4
MAP_FULL = 64
MAP_WINDOW = 32

TERRAIN = 0
SHROUD = 1
ENTITY = 2
FLAGS = 3

# A tile's flag byte: the low three bits name the side whose ground it is
# (a yard reads 40, 41, 42 for the three houses), bit 3 lit, bit 4 a unit
# stands here, bit 5 a building does. Concrete is terrain, not a flag.
OWNER_MASK = 7
FLAG_REVEALED = 8
FLAG_UNIT = 16
FLAG_BUILDING = 32

ROCK = 143
CONCRETE = 126
SPICE_FIRST = 177
SPICE_LAST = 207

UNIT_BASE = 0x0f44
UNIT_STRIDE = 140
BUILD_BASE = 0x4e26
BUILD_STRIDE = 98

LOAD = 2
SELF = 48
KIND = 51
STATE = 55
OWNER = 57
UNIT_Y = 58
UNIT_X = 60
HEALTH = 66
ORDER = 111
TILE_SHIFT = 8

# A record's state byte reads 160 where the game holds it and 0 where it is
# under orders; a unit's order byte is 1 from the press to the arrival.
IDLE = 160
UNDER_ORDERS = 0

HARVESTER = 16
# The type byte names a harvester everywhere; before it was found, 150
# health did, and it is the harvester's alone in missions 1 and 2.
HARVESTER_HEALTH = 150

PURSE_BASE = 0x4d24
PURSE_STRIDE = 70

MISSION = 0xc04c
HOUSE = 0xc274
CONTROL = 0xbf12
PANEL_ROW = 0xbf8c
PANEL_COLUMN = 0xbf8a
PANEL_PRICE = 0xfe8a
PANEL_GEAR = 0xfe92
PICKED_X = 0xbf5c
PICKED_Y = 0xbf5a

HARKONNEN = 0
ATREIDES = 1
ORDOS = 2
FREMEN = 3
SARDAUKAR = 4
MERCENARY = 5
SIDES = 6
NO_OWNER = 255

HOUSE_NAMES = {HARKONNEN: "harkonnen", ATREIDES: "atreides",
               ORDOS: "ordos", FREMEN: "fremen", SARDAUKAR: "sardaukar",
               MERCENARY: "mercenary"}

UNIT_NAMES = {0: "carryall", 1: "ornithopter", 2: "infantry", 3: "troopers",
              4: "soldier", 5: "trooper", 6: "saboteur", 7: "launcher",
              8: "deviator", 9: "tank", 10: "siege tank", 11: "devastator",
              12: "sonic tank", 13: "trike", 14: "raider trike", 15: "quad",
              16: "harvester", 17: "mcv", 25: "sandworm"}

BUILD_NAMES = {0: "concrete", 1: "concrete4", 2: "palace",
               3: "light factory", 4: "heavy factory", 5: "hi-tech",
               6: "ix", 7: "wor", 8: "const yard", 9: "windtrap",
               10: "barracks", 11: "starport", 12: "refinery",
               13: "repair", 14: "wall", 15: "turret", 16: "rocket turret",
               17: "silo", 18: "outpost"}


def record_word(raw, offset):
    return raw[offset] | raw[offset + 1] << 8


def name_of(names, kind):
    return names.get(kind, f"type {kind}")


def is_free_rock(cell):
    return (cell[TERRAIN] == ROCK and cell[ENTITY] == 0
            and not cell[FLAGS] & (FLAG_UNIT | FLAG_BUILDING))


def is_concrete(cell):
    return cell[TERRAIN] == CONCRETE


def is_free_concrete(cell):
    return (is_concrete(cell) and cell[ENTITY] == 0
            and not cell[FLAGS] & (FLAG_UNIT | FLAG_BUILDING))


def blocks(seen, width, height, fits):
    """Answer every tile whose width by height footprint passes that test."""
    side = len(seen[0])
    out = []
    for y in range(len(seen) - height + 1):
        for x in range(side - width + 1):
            if all(fits(seen[y + dy][x + dx])
                   for dy in range(height) for dx in range(width)):
                out.append((x, y))
    return out


def touches(seen, seat, width, height):
    """The game only builds against what stands: concrete or a building."""
    side = len(seen[0])
    x, y = seat
    for dy in range(-1, height + 1):
        for dx in range(-1, width + 1):
            inside = 0 <= dx < width and 0 <= dy < height
            at_x, at_y = x + dx, y + dy
            if inside or not 0 <= at_x < side or not 0 <= at_y < len(seen):
                continue
            beside = seen[at_y][at_x]
            if is_concrete(beside) or beside[FLAGS] & FLAG_BUILDING:
                return True
    return False


def seats(seen, width, height, fits):
    """Answer every footprint that fits and stands against something."""
    return [seat for seat in blocks(seen, width, height, fits)
            if touches(seen, seat, width, height)]


def near(seats, home, top=0):
    """Answer those seats nearest that tile, closest first."""
    return sorted(((x, y + top) for x, y in seats),
                  key=lambda s: abs(s[0] - home[0]) + abs(s[1] - home[1]))


class Snapshot:
    """One frame of Dune, read whole: map, entities, purses and words."""

    def __init__(self, raw, side=MAP_FULL):
        self.raw = raw
        self.side = side
        self.inset = (MAP_FULL - side) // 2
        self.seen = self._read_board()
        self.mission = self.word(MISSION)
        self.house = self.word(HOUSE)
        self.control = self.word(CONTROL)
        self.purses = {owner: self.word(PURSE_BASE + owner * PURSE_STRIDE)
                       for owner in range(SIDES)}
        self.units = self._live(UNIT_BASE, UNIT_STRIDE, FLAG_UNIT)
        self.buildings = self._live(BUILD_BASE, BUILD_STRIDE, FLAG_BUILDING)
        self.build_list = {"row": self.word(PANEL_ROW),
                           "column": self.word(PANEL_COLUMN),
                           "price": self.word(PANEL_PRICE),
                           "gear": self.word(PANEL_GEAR)}

    @classmethod
    def of(cls, run, side=MAP_FULL):
        return cls(run.memory("system", 0, WORK_RAM), side)

    def byte(self, at):
        return self.raw[at]

    def word(self, at):
        return self.raw[at] | self.raw[at + 1] << 8

    def _read_board(self):
        out = []
        for y in range(self.side):
            at = MAP_BASE + (y + self.inset) * MAP_ROW + self.inset * MAP_TILE
            line = self.raw[at:at + MAP_TILE * self.side]
            out.append([list(line[x * MAP_TILE:(x + 1) * MAP_TILE])
                        for x in range(self.side)])
        return out

    def rows(self, top, count):
        return self.seen[top:top + count]

    def tile(self, x, y):
        return self.seen[y][x]

    def on_board(self, at):
        return 0 <= at[0] < self.side and 0 <= at[1] < self.side

    def tile_of(self, raw):
        return (record_word(raw, UNIT_X) >> TILE_SHIFT) - self.inset, \
               (record_word(raw, UNIT_Y) >> TILE_SHIFT) - self.inset

    def record(self, base, stride, slot):
        at = base + slot * stride
        return list(self.raw[at:at + stride])

    def entity(self, base, stride, slot, names):
        raw = self.record(base, stride, slot)
        kind = raw[KIND]
        out = {"slot": slot, "kind": kind, "name": name_of(names, kind),
               "owner": raw[OWNER], "tile": self.tile_of(raw),
               "health": record_word(raw, HEALTH),
               "state": raw[STATE], "raw": raw}
        if len(raw) > ORDER:
            out["order"] = raw[ORDER]
        if kind == HARVESTER:
            out["load"] = record_word(raw, LOAD)
        return out

    def occupants(self, bit):
        """Answer every entity index the map shows under that flag, by tile."""
        out = {}
        for y in range(self.side):
            for x in range(self.side):
                cell = self.seen[y][x]
                if cell[FLAGS] & bit:
                    out.setdefault(cell[ENTITY], []).append((x, y))
        return out

    def _live(self, base, stride, bit):
        """Answer the records the map points at, cross-checked by tile."""
        names = UNIT_NAMES if bit == FLAG_UNIT else BUILD_NAMES
        out = {}
        for slot, tiles in self.occupants(bit).items():
            rec = self.entity(base, stride, slot, names)
            if rec["tile"] in tiles and rec["health"]:
                rec["tiles"] = tiles
                out[slot] = rec
        return out

    def mine(self, table, owner):
        return {slot: rec for slot, rec in table.items()
                if rec["owner"] == owner}

    def owners(self):
        """Answer the map as owner bytes, 255 where nothing stands."""
        out = [[NO_OWNER] * self.side for _ in range(self.side)]
        for table in (self.buildings, self.units):
            for rec in table.values():
                for x, y in rec["tiles"]:
                    out[y][x] = rec["owner"]
        return out

    def picked(self):
        at = (self.word(PICKED_X), self.word(PICKED_Y))
        return at if at != (0, 0) else None

    def tally(self, table):
        out = {}
        for rec in table.values():
            key = (rec["owner"], rec["name"])
            out[key] = out.get(key, 0) + 1
        return out
