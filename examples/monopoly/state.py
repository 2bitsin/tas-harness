"""The player records, the deed table and the dice Monopoly keeps in ram."""

REC_BASE = 0x84de
REC_STRIDE = 0x1c
REC_SQUARE = 0
REC_SEAT = 3
REC_CASH = 6
REC_DOUBLES = 17
REC_NAME = 18
REC_NAME_LEN = 8

PLAYERS_LEFT = 0x84dd
CURRENT_PLAYER = 0x85dd
DEED_BASE = 0x85de
DEED_STRIDE = 4
DEED_HOUSES = 0
DEED_OWNER = 1
DEED_MORTGAGED = 3
MENU_CURSOR = 0x86c4

DICE = 0x867e
RNG = 0x84b8
GAME_MINUTES = 0x86bc

SQUARES = 40
SEATS = 8
BANK = 255
HOTEL = 5
STATE_BLOCK = (0x84dc, 0x86ff - 0x84dc)

GROUPS = {
    "brown": (1, 3),
    "light-blue": (6, 8, 9),
    "pink": (11, 13, 14),
    "orange": (16, 18, 19),
    "red": (21, 23, 24),
    "yellow": (26, 27, 29),
    "green": (31, 32, 34),
    "dark-blue": (37, 39),
}

RAILROADS = (5, 15, 25, 35)
UTILITIES = (12, 28)
BUYABLE = tuple(sorted(sum(GROUPS.values(), ()) + RAILROADS + UTILITIES))

# The board price the game charges for one house on a group, read off the
# deed cards the browser draws.
HOUSE_PRICE = {
    "brown": 50, "light-blue": 50, "pink": 100, "orange": 100,
    "red": 150, "yellow": 150, "green": 200, "dark-blue": 200,
}


def byte(run, address):
    return run.memory("system", address, 1)[0]


def word(run, address, width=2):
    return int.from_bytes(run.memory("system", address, width), "little")


def record(run, seat):
    return list(run.memory("system", REC_BASE + seat * REC_STRIDE,
                           REC_STRIDE))


def player(run, seat):
    """Answer one player's square, purse, seat number and doubles count."""
    raw = record(run, seat)
    return {
        "square": raw[REC_SQUARE],
        "cash": int.from_bytes(bytes(raw[REC_CASH:REC_CASH + 4]), "little"),
        "seat": raw[REC_SEAT],
        "doubles": raw[REC_DOUBLES],
        "name": name_of(raw),
    }


def name_of(raw):
    """The 68000 writes names into byte-swapped work ram, pair by pair."""
    text = bytes(raw[REC_NAME:REC_NAME + REC_NAME_LEN])
    swapped = bytes(text[i ^ 1] for i in range(len(text)))
    return swapped.split(b"\x00")[0].decode("latin1").strip()


def players(run):
    return [player(run, seat) for seat in range(SEATS)
            if record(run, seat)[REC_SEAT] != 0]


def deeds(run):
    """Answer the deed table as square to mortgaged, houses and owner."""
    raw = run.memory("system", DEED_BASE, SQUARES * DEED_STRIDE)
    out = {}
    for square in range(SQUARES):
        entry = raw[square * DEED_STRIDE:(square + 1) * DEED_STRIDE]
        out[square] = {
            "mortgaged": bool(entry[DEED_MORTGAGED]),
            "houses": entry[DEED_HOUSES],
            "owner": entry[DEED_OWNER],
        }
    return out


def owned_by(run, seat):
    return sorted(square for square, deed in deeds(run).items()
                  if deed["owner"] == seat)


def monopolies(run, seat):
    """Answer the colour groups that seat holds whole and unmortgaged."""
    table = deeds(run)
    held = []
    for group, squares in GROUPS.items():
        if all(table[square]["owner"] == seat
               and not table[square]["mortgaged"] for square in squares):
            held.append(group)
    return held


def houses_on(run, group):
    table = deeds(run)
    return [table[square]["houses"] for square in GROUPS[group]]


def dice(run):
    raw = run.memory("system", DICE, 2)
    return raw[0], raw[1]


def current(run):
    return byte(run, CURRENT_PLAYER)


def left(run):
    return byte(run, PLAYERS_LEFT)
