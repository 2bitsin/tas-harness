"""Blake's Hotel as a planner reads it: rooms, edges, hotspots, items."""

import state

# Everything here is either measured on the emulator (`frames`, `word`,
# `centre`, `scroll`) or read off the speedrun guide named in route.py.
# A field that is None was not measured; a planner must treat it as
# unknown, not as zero.
MEASURED = "emulator, v8 step 1b"
STEP_1C = "emulator, v8 step 1c"
GUIDE = "speedrun.com guide y45ls"

UNKNOWN = None

# A hotspot that only exists once a puzzle has been solved: the survey
# that found it says which.
AFTER_DRAWERS = "emulator, v8 step 1b, after the drawers are open"
DERIVED = "the cartridge's box table, through the object array"
SCRIPTED = "the cartridge's object scripts, decoded"

# name -> the word at 0x6ac, from the cartridge's own room table where
# name is unique, and whether the camera scrolls in it.
ROOMS = {
    "lobby": {"word": 16, "scrolls": False, "source": MEASURED},
    "hallway": {"word": 9, "scrolls": True, "source": MEASURED},
    "cafe": {"word": 13, "scrolls": True, "source": MEASURED},
    "office": {"word": 17, "scrolls": False, "source": MEASURED},
    "gardener": {"word": 14, "scrolls": True, "source": MEASURED},
    "kitchen": {"word": 15, "scrolls": UNKNOWN, "source": GUIDE},
    "outside-left": {"word": 11, "scrolls": UNKNOWN, "source": SCRIPTED},
    "outside-right": {"word": 12, "scrolls": UNKNOWN, "source": SCRIPTED},
    "shed": {"word": UNKNOWN, "scrolls": UNKNOWN, "source": GUIDE},
    "basement": {"word": 2, "scrolls": UNKNOWN, "source": GUIDE},
    "bridge": {"word": 21, "scrolls": UNKNOWN, "source": GUIDE},
    "mine": {"word": 6, "scrolls": UNKNOWN, "source": GUIDE},
    "shaft": {"word": 18, "scrolls": UNKNOWN, "source": GUIDE},
    "pond": {"word": 19, "scrolls": UNKNOWN, "source": GUIDE},
    "maze": {"word": 7, "scrolls": UNKNOWN, "source": SCRIPTED},
    "tomb": {"word": 20, "scrolls": UNKNOWN, "source": GUIDE},
    "dungeon": {"word": 4, "scrolls": UNKNOWN, "source": SCRIPTED},
}


# The live object array in work ram, 26 bytes an object, indexed by the
# object's word less three; the hit test at 0x000e94 reads it every frame.
OBJECTS = 0x1200
OBJECT_BYTES = 0x1a
OBJECT_FIRST = 3
OBJECT_COUNT = 0x6f4

# The room an object's own record names: nought is nowhere
# and one is the inventory.
NOWHERE = 0
CARRIED = 1

# Fields of one record, as the hit test uses them.
OBJECT_BOX = 2
OBJECT_ROOM = 6
OBJECT_STATE = 8
OBJECT_VERB = 0x16
OBJECT_FLAGS = 0x18
ACTOR_BIT = 2

# The cartridge's own tables, which cartridge.Cartridge reads through the
# rom region: the packed string block, the eight-byte object table indexed
# by the hovered word, the twenty-byte room table indexed by the room word,
# and the box index whose longs are offsets from the box base.
CART = "cartridge"
STRINGS = 0x14199c
STRINGS_END = 0x145eb8
OBJECT_TABLE = 0x1461f0
OBJECT_ENTRY = 8
OBJECT_LAST = 183
# Object w's script pointer sits one entry back from its name, at
# w * 8 - 4: every script's self-references name w, not w - 1.
OBJECT_SCRIPT = -4
SCRIPT_BASE = 0x1467ac
SCRIPT_END = 0x14f5dc
SCRIPT_HEADER = 6
WALK_ON = 11
ROOM_TABLE = 0xfd488
ROOM_ENTRY = 20
ROOM_FIRST = 2
ROOM_LAST = 22
BOX_INDEX = 0x145eb8
BOX_BASE = 0x13e9b6
BOX_COUNT = 206
BOX_WORDS = 4

# The camera, added to the crosshair before the test, and the cell the
# test works in: cell = ((x + 8 + camera_x) >> 3, (y - 8 + camera_y) >> 3).
CAMERA_X = 0x6ca
CAMERA_Y = 0x6ce
CELL = 8
CELL_SHIFT = 3
CROSS_X = 8
CROSS_Y = -8

# Every hotspot box in the cartridge, in eight pixel cells:
# x, y, width, height, at 0x13e9b6 plus the long at 0x145eb8
# indexed by an object's own box number less one.


# The cartridge's own room table, one 20 byte record a room at
# 0x0fd488, indexed by the word at 0x6ac; the name is a pointer
# into the packed string block at 0x14199c.

# The cartridge's object table, 8 bytes an object at 0x1461f0:
# the name's offset then the object's script. The index is the
# word the game puts at 0x6ae while the crosshair is over it.
def edge(room, hold, goes, gate=None, frames=UNKNOWN, source=GUIDE,
         through=None):
    """One held direction out of a room, and what has to be true first."""
    return {"room": room, "hold": hold, "goes": goes, "gate": gate,
            "frames": frames, "source": source, "through": through}


def cross(here, there, word, verb, frames=UNKNOWN, gate=None,
          source=SCRIPTED):
    """One room change a script holds: which object, under which verb."""
    return {"here": here, "there": there, "word": word, "verb": verb,
            "frames": frames, "gate": gate, "source": source}


# Every room change the cartridge's scripts hold, by room word: the object
# whose script holds it and the verb whose block it sits in. Verb 11 is the
# walk-onto the game runs when Shaggy steps into the box.
CROSSINGS = (
    cross( 2,  6, 143, 11),  # Secret Door: The Basement to The Mine
    cross( 2, 11, 142, 11),  # Stairs: The Basement to Outside the Hotel
    cross( 2, 17, 141, 11),  # Stairs: The Basement to The Office
    cross( 4, 20,  20, 11),  # Passage: The Dungeon to The Tomb
    cross( 5,  4,  25,  5),  # Rope: The Dungeon to The Dungeon
    cross( 6,  2,  33, 11),  # Passage: The Mine to The Basement
    cross( 6, 18,  29,  5),  # Mine Car: The Mine to The Shaft
    cross( 7, 19, 158, 11),  # Exit: The Maze to The pond
    cross( 8, 19, 160, 11),  # Exit: The Maze to The pond
    cross( 8, 20, 161, 11),  # Exit: The Maze to The Tomb
    cross( 9, 14,  81, 11),  # Door: The Hallway to The Gardener's Room
    cross( 9, 15,  80,  5, 2020),  # Dumb Waiter: Hallway to Kitchen
    cross( 9, 16,  82, 11),  # The Stairs: The Hallway to The Lobby
    cross(11,  2,  57, 11),  # Double Doors: Outside the Hotel to The Basement
    cross(11, 12,  62, 11),  # Path to Shed: Outside to Outside
    cross(11, 16,  61, 11),  # Main Entrance: Outside the Hotel to The Lobby
    cross(12, 11,  69, 11),  # Path to Main Entrance: Outside to Outside
    cross(13, 15, 130, 11),  # Double Doors: The Cafe to The Kitchen
    cross(13, 16, 136, 11),  # Archway: The Cafe to The Lobby
    cross(14,  9,  86, 11),  # Door: The Gardener's Room to The Hallway
    cross(15,  9, 110,  5),  # Dumb Waiter: The Kitchen to The Hallway
    cross(15, 13, 115, 11),  # Kitchen Door: The Kitchen to The Cafe
    cross(16,  9,  46, 11),  # Stairs: The Lobby to The Hallway
    cross(16, 11,  47, 11),  # Outside Door: The Lobby to Outside the Hotel
    cross(16, 13,  45, 11),  # Archway: The Lobby to The Cafe
    cross(16, 17,  44, 11),  # Door: The Lobby to The Office
    cross(17, 16, 101, 11),  # Door: The Office to The Lobby
    cross(18,  6, 151, 11),  # Exit: The Shaft to The Mine
    cross(18, 19, 152, 11),  # Stone Path: The Shaft to The pond
    cross(19,  7, 156, 11),  # Archway: The pond to The Maze
    cross(19, 18, 155, 11),  # Stone Path: The pond to The Shaft
    cross(19, 20, 156, 11),  # Archway: The pond to The Tomb
    cross(20,  4, 167, 11),  # Archway: The Tomb to The Dungeon
    cross(20, 19, 163, 11),  # Archway: The Tomb to The pond
    cross(21,  5, 169,  5),  # Rope: Across the River to The Dungeon
    cross(21, 11, 174, 11),  # Totem Pole Bridge: Across the River to Outside
)


def walk(here, there, flag, want=1, frames=UNKNOWN, source=GUIDE):
    """One room change the engine makes off script, and the flag it wants."""
    return {"here": here, "there": there, "flag": flag, "want": want,
            "frames": frames, "source": source}


# No object in room 7 answers with room 8 and Flashlight(183) carries no
# script, so the maze walk and its light are the engine's own, keyed on the
# flag Use Bulb and Battery with Soda Tab sets.
WALKS = (
    walk(7, 8, (6, 7)),
)


# The room graph. `gate` is a fact from FACTS that must hold before the
# edge exists at all; `frames` is what one crossing cost, measured.
EDGES = (
    edge("lobby", "up", "hallway", None, 252, MEASURED),
    edge("lobby", "down", "cafe", None, 366, MEASURED),
    edge("lobby", "right", "office", "lobby door open", 12, MEASURED),
    edge("lobby", "left", "outside-left", "outside door open"),
    edge("hallway", "left", "lobby", None, UNKNOWN, MEASURED),
    edge("hallway", "up", "gardener", "door #1 open", 240, MEASURED),
    edge("gardener", "right", "hallway", None, 406, MEASURED, 85),
    edge("cafe", "right", "lobby", None, 274, MEASURED),
    edge("cafe", "up", "hallway", None, 574, MEASURED, 126),
    edge("cafe", "up", "kitchen", "dumb waiter open"),
    edge("kitchen", "down", "cafe"),
    edge("office", "left", "lobby", None, UNKNOWN, MEASURED),
    edge("office", "down", "basement", "basement open"),
    edge("outside-left", "left", "outside-right", None, UNKNOWN,
         MEASURED),
    edge("outside-left", "left", "bridge", "the bridge crossed"),
    edge("outside-right", "down", "basement", "basement open"),
    edge("basement", "up", "office"),
    edge("basement", "left", "mine", "the mine open"),
    edge("mine", "left", "shaft", "the mine car ridden"),
    edge("shaft", "left", "pond"),
    edge("pond", "left", "maze"),
    edge("maze", "up", "tomb", "the maze lit"),
    edge("tomb", "left", "dungeon"),
)


def spot(room, word, name, box, verb, actor, source=DERIVED):
    """One hotspot, as the cartridge's own box and the verb a click runs."""
    return {"room": room, "word": word, "name": name, "box": box,
            "verb": verb, "actor": actor, "source": source}


# Every hotspot of every room the run reached, read out of the live object
# array and BOXES rather than rastered. A box of None is an actor, placed
# by its sprite, and only those still need the crosshair to find them.
HOTSPOTS = (
    # hallway
    spot("hallway", 73, "", (17, 6, 4, 5), 10, False),
    spot("hallway", 74, "Door", (25, 4, 3, 9), 3, False),
    spot("hallway", 75, "Door", (30, 5, 2, 4), 3, False),
    spot("hallway", 76, "Door", (39, 3, 2, 8), 3, False),
    spot("hallway", 77, "Door", (42, 4, 2, 8), 3, False),
    spot("hallway", 78, "Door", (37, 5, 2, 4), 3, False),
    spot("hallway", 79, "Door", (28, 4, 2, 7), 3, False),
    spot("hallway", 80, "Dumb Waiter", (17, 5, 5, 6), 5, False),
    spot("hallway", 81, "Door", (33, 4, 3, 5), 4, False),
    spot("hallway", 82, "The Stairs", (4, 14, 3, 2), 10, False),
    # cafe
    spot("cafe", 125, "The Cook", None, 10, True),
    spot("cafe", 126, "Cabinet", (26, 11, 4, 3), 3, False),
    spot("cafe", 128, "Cash Register", (6, 11, 5, 4), 10, False),
    spot("cafe", 129, "Key", (7, 7, 1, 1), 10, False),
    spot("cafe", 130, "Double Doors", (13, 5, 4, 6), 3, False),
    spot("cafe", 131, "", (9, 10, 4, 4), 10, False),
    spot("cafe", 132, "Lamp", (18, 1, 4, 5), 10, False),
    spot("cafe", 133, "Radio", (17, 6, 4, 7), 5, False),
    spot("cafe", 136, "Archway", (23, 6, 4, 7), 10, False),
    spot("cafe", 137, "Bottle of Oil", (22, 12, 3, 5), 10, False),
    # gardener
    spot("gardener", 83, "Television", (22, 14, 10, 5), 10, False),
    spot("gardener", 84, "Antenna", (23, 8, 7, 5), 10, False),
    spot("gardener", 85, "Air Freshener", (23, 12, 3, 5), 10, False),
    spot("gardener", 86, "Door", (27, 6, 5, 9), 4, False),
    spot("gardener", 87, "Drawer", (7, 8, 2, 3), 4, False),
    spot("gardener", 88, "Drawer", (20, 11, 4, 3), 4, False),
    spot("gardener", 89, "Poison Oak", (2, 10, 5, 5), 10, False),
    spot("gardener", 90, "Bed Spring", (17, 10, 3, 2), 10, False),
    spot("gardener", 91, "Book", (21, 11, 3, 2), 10, False),
    # lobby
    spot("lobby", 6, "Bellhop", None, 10, True),
    spot("lobby", 37, "Picture", (42, 2, 3, 5), 10, False),
    spot("lobby", 38, "Zebra", (17, 10, 8, 4), 10, False),
    spot("lobby", 39, "Mailbox", (47, 4, 6, 5), 10, False),
    spot("lobby", 40, "Fire", None, 10, True),
    spot("lobby", 43, "Bell", (40, 8, 2, 2), 5, False),
    spot("lobby", 44, "Door", (37, 3, 2, 5), 3, False),
    spot("lobby", 45, "Archway", (36, 18, 10, 1), 10, False),
    spot("lobby", 46, "Stairs", (32, 4, 3, 2), 10, False),
    spot("lobby", 47, "Outside Door", (11, 8, 4, 5), 3, False),
)


def act(room, spot_word, verb, target, gives, needs=(), source=GUIDE,
        frames=UNKNOWN):
    """One action on one hotspot: what it needs and what it makes true."""
    return {"room": room, "spot": spot_word, "verb": verb, "target": target,
            "gives": gives, "needs": tuple(needs), "source": source,
            "frames": frames}


# Every action the guide's line performs, as preconditions and effects.
# `gives` names facts in FACTS; an item is a fact whose name is the item.
ACTIONS = (
    act("lobby", 44, state.OPEN, "Door", ("lobby door open",),
        source=MEASURED, frames=234),
    act("hallway", 74, state.OPEN, "Door", ("door #1 open",),
        source=MEASURED, frames=1324),
    act("hallway", 75, state.OPEN, "Door", ("door #2 open",),
        source=MEASURED, frames=1391),
    act("hallway", 76, state.OPEN, "Door", ("door #3 open",),
        source=MEASURED, frames=1750),
    act("hallway", 77, state.OPEN, "Door", ("door #5 open",),
        source=MEASURED, frames=1378),
    act("hallway", 78, state.OPEN, "Door", ("door #6 open",),
        source=MEASURED, frames=1304),
    act("hallway", 79, state.OPEN, "Door", ("door #7 open",),
        source=MEASURED, frames=4221),
    act("hallway", 80, state.OPEN, "Dumb Waiter", ("dumb waiter open",),
        source=MEASURED, frames=610),
    act("gardener", 88, state.OPEN, "Drawer", ("drawer open",)),
    act("gardener", UNKNOWN, state.TAKE, "Antacid", ("antacid",),
        ("drawer open",)),
    act("gardener", UNKNOWN, state.TAKE, "Book", ("book",)),
    act("gardener", 89, state.USE, "Poison Oak", ("poison oak",),
        ("work gloves",)),
    act("gardener", 90, state.TAKE, "Bed Spring", ("bed spring",)),
    act("gardener", 85, state.TAKE, "Air Freshener", ("air freshener",)),
    act("kitchen", 119, state.PUSH, "Refrigerator",
        ("refrigerator pushed",), source=MEASURED, frames=3459),
    act("kitchen", 121, state.TAKE, "Soda Tab", ("soda tab",),
        ("refrigerator pushed",), source=MEASURED, frames=1719),
    act("kitchen", 106, state.USE, "Kitchen Sink", ("pot of water",),
        ("pot",), source=MEASURED, frames=659),
    act("kitchen", 116, state.OPEN, "Microwave", ("microwave open",),
        source=MEASURED, frames=1773),
    act("kitchen", UNKNOWN, state.TAKE, "Chili", ("chili",),
        ("microwave open",)),
    act("kitchen", UNKNOWN, state.PUSH, "Flour", ("flour pushed",)),
    act("kitchen", UNKNOWN, state.LOOK, "Peephole", ("peephole looked at",),
        ("flour pushed",)),
    act("kitchen", 116, state.USE, "Microwave", ("cow bell",),
        ("frozen bell", "microwave open")),
    act("kitchen", UNKNOWN, state.USE, "Vent Cover", ("vent open",),
        ("screwdriver",)),
    act("kitchen", UNKNOWN, state.USE, "Termites", ("termites",),
        ("vent open", "empty chili can")),
    act("cafe", UNKNOWN, state.PUSH, "Radio", ("radio pushed",)),
    act("cafe", UNKNOWN, state.OPEN, "Radio", ("radio open",),
        ("radio pushed",)),
    act("cafe", UNKNOWN, state.TAKE, "Battery", ("battery",),
        ("radio open",)),
    act("cafe", UNKNOWN, state.EAT, "Antacid", ("the cook gone",),
        ("antacid",)),
    act("cafe", UNKNOWN, state.TAKE, "Key", ("key",), ("the cook gone",)),
    act("cafe", UNKNOWN, state.OPEN, "Cabinet", ("can opener",)),
    act("cafe", UNKNOWN, state.USE, "Chili", ("empty chili can",),
        ("can opener", "chili")),
    act("lobby", 40, state.USE, "Fire", ("the fire out",),
        ("pot of water",)),
    act("lobby", UNKNOWN, state.TAKE, "Crumpled Note", ("crumpled note",),
        ("the fire out",)),
    act("lobby", UNKNOWN, state.LOOK, "Crumpled Note", ("note read",),
        ("crumpled note",)),
    act("lobby", UNKNOWN, state.OPEN, "Double Doors",
        ("outside door open",)),
    act("lobby", UNKNOWN, state.USE, "Outlet", ("cord plugged in",),
        ("extension cord",)),
    act("lobby", UNKNOWN, state.USE, "Extension Cord", ("heater on",),
        ("heater", "cord plugged in")),
    act("lobby", UNKNOWN, state.USE, "Bear", ("the bridge crossed",),
        ("poison oak",)),
    act("lobby", UNKNOWN, state.USE, "Cow Bell", ("the bellhop called",),
        ("cow bell",)),
    act("lobby", UNKNOWN, state.GIVE, "Bellhop", ("the bellhop paid",),
        ("doll", "the bellhop called")),
    act("lobby", UNKNOWN, state.TAKE, "Goblet", ("goblet",),
        ("the bellhop paid",)),
    act("outside-left", UNKNOWN, state.USE, "Snowman", ("bell in the snow",),
        ("shovel",)),
    act("outside-left", UNKNOWN, state.TAKE, "Frozen Bell", ("frozen bell",),
        ("bell in the snow",)),
    act("outside-left", UNKNOWN, state.USE, "Lock", ("shed unlocked",),
        ("key",)),
    act("outside-left", UNKNOWN, state.OPEN, "Door", ("shed open",),
        ("shed unlocked",)),
    act("outside-left", UNKNOWN, state.TAKE, "Crowbar", ("crowbar",),
        ("shed open",)),
    act("outside-left", UNKNOWN, state.TAKE, "Work Gloves",
        ("work gloves",), ("shed open",)),
    act("outside-left", UNKNOWN, state.TAKE, "Weed Killer",
        ("weed killer",), ("shed open",)),
    act("outside-left", UNKNOWN, state.USE, "Bed Spring",
        ("the lights reached",), ("bed spring",)),
    act("outside-left", UNKNOWN, state.TAKE, "Christmas Lights",
        ("christmas lights",), ("the lights reached",)),
    act("outside-right", UNKNOWN, state.USE, "Snow Covered Doors",
        ("doors cleared",), ("shovel",)),
    act("outside-right", UNKNOWN, state.USE, "Locked Doors",
        ("doors prised",), ("crowbar", "doors cleared")),
    act("outside-right", UNKNOWN, state.OPEN, "Double Doors",
        ("basement open",), ("doors prised",)),
    act("basement", UNKNOWN, state.USE, "Rack", ("the mine open",),
        ("note read",)),
    act("basement", UNKNOWN, state.OPEN, "Locker", ("locker open",)),
    act("basement", UNKNOWN, state.TAKE, "Extension Cord",
        ("extension cord",), ("locker open",)),
    act("basement", UNKNOWN, state.TAKE, "Screwdriver", ("screwdriver",),
        ("locker open",)),
    act("basement", UNKNOWN, state.USE, "Hook", ("the scenario done",),
        ("medallion",)),
    act("office", UNKNOWN, state.TAKE, "Heater", ("heater",)),
    act("office", UNKNOWN, state.OPEN, "Drawer", ("office drawer open",)),
    act("office", UNKNOWN, state.TAKE, "Scissors", ("scissors",),
        ("office drawer open",)),
    act("bridge", UNKNOWN, state.USE, "Rope", ("rope cut",), ("scissors",)),
    act("bridge", UNKNOWN, state.TAKE, "Rope", ("rope",), ("rope cut",)),
    act("bridge", UNKNOWN, state.TAKE, "Doll", ("doll",)),
    act("mine", UNKNOWN, state.TAKE, "Wheel", ("wheels",)),
    act("mine", UNKNOWN, state.USE, "Mine Car", ("the car wheeled",),
        ("wheels",)),
    act("mine", UNKNOWN, state.USE, "Mine Car", ("the mine car ridden",),
        ("the car wheeled",)),
    act("pond", UNKNOWN, state.USE, "Air Freshener", ("the pond crossed",),
        ("air freshener",)),
    act("pond", UNKNOWN, state.PUSH, "Switch", ("the switch thrown",),
        ("medallion",)),
    act("maze", UNKNOWN, state.USE, "Christmas Lights", ("flashlight",),
        ("battery", "soda tab", "christmas lights")),
    act("maze", UNKNOWN, state.USE, "Flashlight", ("the maze lit",),
        ("flashlight",)),
    act("tomb", UNKNOWN, state.USE, "Statue", ("the goblet placed",),
        ("goblet",)),
    act("tomb", UNKNOWN, state.USE, "Killer Lettuce", ("the lettuce killed",),
        ("weed killer",)),
    act("tomb", UNKNOWN, state.GIVE, "Uncle Blake", ("the book given",),
        ("book", "Blake freed")),
    act("tomb", UNKNOWN, state.TALK, "Statue", ("the statue answered",),
        ("the book given", "the goblet placed")),
    act("tomb", UNKNOWN, state.TAKE, "Medallion", ("medallion",),
        ("the statue answered",)),
    act("dungeon", UNKNOWN, state.USE, "Cuffs", ("the cuffs roped",),
        ("rope",)),
    act("dungeon", UNKNOWN, state.USE, "Uncle Blake", ("Blake freed",),
        ("termites", "the cuffs roped")),
)

# The goal a planner drives at.
GOAL = "the scenario done"

# Items the player carries. Every one is a fact in ACTIONS' `gives`, and
# this is the index a planner reads to answer "where do I get X".
ITEMS = ("antacid", "book", "poison oak", "bed spring", "air freshener",
         "soda tab", "pot", "pot of water", "chili", "empty chili can",
         "battery", "key", "can opener", "crumpled note", "shovel",
         "frozen bell", "cow bell", "crowbar", "work gloves", "weed killer",
         "christmas lights", "flashlight", "extension cord", "screwdriver",
         "heater", "scissors", "rope", "doll", "goblet", "wheels",
         "termites", "medallion")

# Items the guide uses without ever taking them: they are in hand at the
# start of the scenario, or the guide's line takes them off screen.
CARRIED_AT_START = ("pot", "shovel")

# The two orders that lose the scenario, so a search refuses them.
SOFT_LOCKS = (
    {"fact": "crumpled note", "without": "note read",
     "why": "the wine rack refuses a note that was never looked at, and the"
            " mine is then unreachable", "word": UNKNOWN},
    {"fact": "key", "without": "the cook gone",
     "why": "the key cannot be taken while the cook is in the cafe, and the"
            " cook only leaves for the antacid", "word": UNKNOWN},
)

def fact(name, word, field, how, value, source=STEP_1C):
    """One flag, as the address in the object array that carries it."""
    at = OBJECTS + (word - OBJECT_FIRST) * OBJECT_BYTES + field
    return {"name": name, "word": word, "at": at, "how": how,
            "value": value, "source": source}


# Every fact a plan rests on is one byte or word of an object's own record.
# An item is carried when its room word reads 1, and a drawer is open when
# the verb a bare click would run has turned from Open into Shut. The
# hallway's seven doors are not in here: they read Open again on the next
# visit, and the bitmask that opens them, at word 81 plus 0xd, clears when
# the room is re-entered.
FACT_WORDS = {
    "antacid": fact("antacid", 92, OBJECT_ROOM, "equal", CARRIED),
    "book": fact("book", 91, OBJECT_ROOM, "equal", CARRIED),
    "bed spring": fact("bed spring", 90, OBJECT_ROOM, "equal", CARRIED),
    "air freshener": fact("air freshener", 85, OBJECT_ROOM, "equal",
                          CARRIED),
    "poison oak": fact("poison oak", 89, OBJECT_ROOM, "equal", CARRIED),
    "battery": fact("battery", 135, OBJECT_ROOM, "equal", CARRIED),
    "key": fact("key", 129, OBJECT_ROOM, "equal", CARRIED),
    "can opener": fact("can opener", 127, OBJECT_ROOM, "equal", CARRIED),
    "crumpled note": fact("crumpled note", 42, OBJECT_ROOM, "equal",
                          CARRIED),
    "goblet": fact("goblet", 41, OBJECT_ROOM, "equal", CARRIED),
    "shovel": fact("shovel", 72, OBJECT_ROOM, "equal", CARRIED),
    "frozen bell": fact("frozen bell", 117, OBJECT_ROOM, "equal", CARRIED),
    "cow bell": fact("cow bell", 118, OBJECT_ROOM, "equal", CARRIED),
    "crowbar": fact("crowbar", 67, OBJECT_ROOM, "equal", CARRIED),
    "work gloves": fact("work gloves", 65, OBJECT_ROOM, "equal", CARRIED),
    "weed killer": fact("weed killer", 66, OBJECT_ROOM, "equal", CARRIED),
    "christmas lights": fact("christmas lights", 52, OBJECT_ROOM, "equal",
                             CARRIED),
    "extension cord": fact("extension cord", 176, OBJECT_ROOM, "equal",
                           CARRIED),
    "screwdriver": fact("screwdriver", 140, OBJECT_ROOM, "equal", CARRIED),
    "scissors": fact("scissors", 95, OBJECT_ROOM, "equal", CARRIED),
    "heater": fact("heater", 99, OBJECT_ROOM, "equal", CARRIED),
    "doll": fact("doll", 172, OBJECT_ROOM, "equal", CARRIED),
    "rope": fact("rope", 169, OBJECT_ROOM, "equal", CARRIED),
    "soda tab": fact("soda tab", 121, OBJECT_ROOM, "equal", CARRIED),
    "chili": fact("chili", 103, OBJECT_ROOM, "equal", CARRIED),
    "empty chili can": fact("empty chili can", 104, OBJECT_ROOM, "equal",
                            CARRIED),
    "pot of water": fact("pot of water", 123, OBJECT_ROOM, "equal", CARRIED),
    "termites": fact("termites", 105, OBJECT_ROOM, "equal", CARRIED),
    "medallion": fact("medallion", 165, OBJECT_ROOM, "equal", CARRIED),
    "flashlight": fact("flashlight", 181, OBJECT_ROOM, "equal", CARRIED),
    "bedside tables": fact("bedside tables", 87, OBJECT_VERB, "equal",
                           state.SHUT),
    "the other bedside table": fact("the other bedside table", 88,
                                    OBJECT_VERB, "equal", state.SHUT),
    "the book is out": fact("the book is out", 91, OBJECT_ROOM, "equal",
                            ROOMS["gardener"]["word"]),
    "the antacid is out": fact("the antacid is out", 92, OBJECT_ROOM,
                               "equal", ROOMS["gardener"]["word"]),
}
