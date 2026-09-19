"""The board's money, read out of the cartridge: price, rent, mortgage."""

import struct

REGION = "cartridge"
DEED_TABLE = 0x14ec6
DEED_STRIDE = 20
GROUP_TABLE = 0x14c92
RENT_COLUMNS = 6
COL_MORTGAGE = 12
COL_HOUSE = 14
COL_PRICE = 16

SQUARES = 40
HOTEL = 5
RAILROAD = 8
UTILITY = 9
NOTHING = 10

RAILROAD_RENT = (25, 50, 100, 200)
UTILITY_BARE = 4
UTILITY_PAIR = 10
GROUP_DOUBLE = 2
RAILROAD_SQUARES = (5, 15, 25, 35)
UTILITY_SQUARES = (12, 28)

GROUP_NAMES = ("brown", "light-blue", "pink", "orange", "red", "yellow",
               "green", "dark-blue")


class Board:
    """Every square's deed row, as the 68000 reads it at $14ec6."""

    def __init__(self, run):
        self.deeds = bytes(run.memory(REGION, DEED_TABLE,
                                      SQUARES * DEED_STRIDE))
        self.groups = list(run.memory(REGION, GROUP_TABLE, SQUARES))
        self.rows = [self._row(square) for square in range(SQUARES)]

    def _row(self, square):
        words = struct.unpack_from(">10H", self.deeds, DEED_STRIDE * square)
        return {
            "rents": words[:RENT_COLUMNS],
            "mortgage": words[COL_MORTGAGE // 2],
            "house": words[COL_HOUSE // 2],
            "price": words[COL_PRICE // 2],
            "group": self.groups[square],
        }

    def price(self, square):
        return self.rows[square]["price"]

    def mortgage(self, square):
        return self.rows[square]["mortgage"]

    def house_price(self, square):
        return self.rows[square]["house"]

    def group(self, square):
        code = self.rows[square]["group"]
        if code < len(GROUP_NAMES):
            return GROUP_NAMES[code]
        return {RAILROAD: "railroad", UTILITY: "utility"}.get(code)

    def squares_of(self, group):
        code = GROUP_NAMES.index(group)
        return [s for s in range(SQUARES) if self.groups[s] == code]

    def buyable(self, square):
        return self.rows[square]["group"] != NOTHING

    def rent(self, square, houses=0, whole_group=False, owned=1, dice=0,
             doubled=False):
        """What the owner is owed, by the rules at rom 0x6636 and 0x66ec."""
        row = self.rows[square]
        code = row["group"]
        if code == RAILROAD:
            due = RAILROAD_RENT[max(1, min(owned, len(RAILROAD_RENT))) - 1]
        elif code == UTILITY:
            due = dice * (UTILITY_PAIR if owned > 1 else UTILITY_BARE)
        elif code == NOTHING:
            return 0
        elif houses:
            due = row["rents"][min(houses, HOTEL)]
        else:
            due = row["rents"][0] * (GROUP_DOUBLE if whole_group else 1)
        return due * (GROUP_DOUBLE if doubled else 1)
