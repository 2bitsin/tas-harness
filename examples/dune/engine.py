"""Dune's hand: the map cursor, the yard's list and the placement ghost."""

import state

BLIP_X = 0xbf4c
BLIP_Y = 0xbf4e
RADAR_LEFT = 240
RADAR_TOP = 144
CURSOR_X = 0xbf12
CURSOR_Y = 0xbf14
MODE = 0xf702
PANEL_ROW = 0xbf8c
PANEL_COLUMN = 0xbf8a
PANEL_PRICE = 0xfe8a
PANEL_GEAR = 0xfe92
PANEL_ITEM = 0xbfa0
PICKED_X = 0xbf5c
PICKED_Y = 0xbf5a
PICK_SHIFT = 8

# The radar draws the playable map over 64 pixels: one pixel a tile on
# mission 9's 62-tile map, two on mission 1's 32-tile one. That playable
# map is inset in the 64-tile board the records number by, one tile on
# mission 9 and sixteen on mission 1, which is why the yard at (43,43)
# answers a press at radar pixel 42. A snapshot taken with the window's
# own side has its inset out already, so only the full board adds it back.
RADAR_SPAN = 64
FULL_INSET = 1

# The mode word's high byte: 1 over the bare map, 2 with something held or
# a ghost up, 4 on the list's tab strip and 3 on its item rows.
MODE_SHIFT = 8
LIST_MODES = (3, 4)

A = "y"
B = "b"
TAP_HOLD = 6

# The pad must be let go for two frames before the next press is taken: at
# none the second press on the yard is lost, at two the list opens.
PAD_GAP = 2

# Held, the cursor crosses a tile every 32/3 frames on either axis and on
# both at once, and the release costs no tile: 19 tiles measured 203
# frames, 28 measured 299, 42 measured 448 and 49 measured 523. Left and
# up cross their first tile on the first held frame and so come in one
# tile cheaper, which the cost keeps as slack rather than a second rate.
TILE_FRAMES = 32 / 3

# A held direction bites in eleven frames once the game is running, but in
# fifty-one on the first frame the player has control, so a move gives up
# on a word that has not moved only well past that.
MOVE_LIMIT = 1200
STALL_FRAMES = 90

# Under the ghost the cursor steps whole tiles, so a step begun before the
# release finishes after it: the word drifts one tile up to forty frames on.
STILL_FRAMES = 30
STILL_LIMIT = 240
STEADY_TRIES = 4

# A press takes the unit nearest it before any building under it, so one
# walking past the yard steals the press that would open its list; a retry
# lands once it has gone by.
SELECT_TRIES = 3
PICK_FRAMES = 240
LIST_FRAMES = 600
PICK_TRIES = 3
ORDER_FRAMES = 240
GHOST_FRAMES = 2400
PLACE_FRAMES = 600
PANEL_TRIES = 8
# The list drops a tap now and then, so the edge of a row is only proved
# by three of them in a row going unanswered.
EDGE_TRIES = 3
PANEL_HOLD = 4
PANEL_ANSWER = 120

CONCRETE = "concrete"
ITEMS = {"concrete": 0, "windtrap": 1, "refinery": 2,
         "outpost": 3, "silo": 4, "vehicle": 5}
# What the yard asks, read off its own panel word seat by seat; pick()
# answers the live price so a caller never has to trust this table.
PRICES = {"concrete": 15, "windtrap": 300, "refinery": 400,
          "outpost": 400, "silo": 150, "vehicle": 400}
# The seat a list draws an item on moves as the list grows, so an item is
# named by the two words the panel draws for it: its price and its gear.
BADGES = {"concrete": (15, 40), "windtrap": (300, 400),
          "refinery": (400, 900), "outpost": (400, 1000),
          "silo": (150, 300), "turret": (125, 250),
          "rocket turret": (250, 500), "wall": (50, 140)}
ITEM_COLUMNS = 3
FIRST_ITEM_ROW = 1
EXIT_ROW = 0
EXIT_COLUMN = 0
FIX_COLUMN = 1
STOP_COLUMN = 2

# The price word reads 65535 on the EXIT row and the item's own cost on a
# seat that can be bought; it is stack scratch, so it answers nothing at
# all until the list has drawn a seat once.
PANEL_NONE = 0xffff

GHOST_CROP = 64
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 224
REFUSED_RGB = (168, 0, 0)
ALLOWED_RGB = (0, 168, 0)
GHOST_LEAST = 300

AXIS_BUTTONS = (("left", "right"), ("up", "down"))


def word(run, address):
    return run.word("system", address)


def inset_of(side):
    """A short map is a window centred in the board the records number by."""
    return (state.MAP_FULL - side) // 2


def scale_of(side):
    return RADAR_SPAN // side


def origin_of(side):
    return FULL_INSET if side == state.MAP_FULL else 0


def at(run, side=state.MAP_FULL):
    """Answer the tile a press would act on, from the radar's own words."""
    scale = scale_of(side)
    origin = origin_of(side)
    return ((word(run, BLIP_X) - RADAR_LEFT) // scale + origin,
            (word(run, BLIP_Y) - RADAR_TOP) // scale + origin)


def picked(run, side=state.MAP_FULL):
    inset = inset_of(side)
    return ((word(run, PICKED_X) >> PICK_SHIFT) - inset,
            (word(run, PICKED_Y) >> PICK_SHIFT) - inset)


def reachable(tile, side=state.MAP_FULL):
    """Clamp to the playable map: the blip cannot leave it."""
    origin = origin_of(side)
    return tuple(min(max(part, origin), side - 1 - origin) for part in tile)


def cost_of(here, there):
    """Frames a move over that gap is worth, at the rate measured held."""
    steps = max(abs(there[0] - here[0]), abs(there[1] - here[1]))
    return round(steps * TILE_FRAMES) if steps else 0


def wanted(here, there):
    out = []
    for axis, buttons in enumerate(AXIS_BUTTONS):
        if here[axis] != there[axis]:
            out.append(buttons[1 if here[axis] < there[axis] else 0])
    return out


def tap(run, button, held=TAP_HOLD, gap=PAD_GAP):
    run.release(1)
    run.step(gap)
    run.hold(1, [button])
    run.step(held)
    run.release(1)
    run.step(gap)


def waited_for(run, test, limit):
    """Answer the frames the game took to say so, or None if it never did."""
    began = run.frames()
    try:
        run.run_until(test, limit)
    except RuntimeError:
        return None
    return run.frames() - began


def push_to(run, target, limit, side=state.MAP_FULL):
    """Hold both axes at once and let each go as its own word lands."""
    began = run.frames()
    held = []
    still = 0
    while run.frames() - began < limit:
        here = at(run, side)
        if here == target:
            break
        want = wanted(here, target)
        for button in held:
            if button not in want:
                run.release(1, [button])
        for button in want:
            if button not in held:
                run.hold(1, [button])
        held = want
        run.step(1)
        still = still + 1 if at(run, side) == here else 0
        if still >= STALL_FRAMES:
            break
    run.release(1)


def stand_still(run, limit=STILL_LIMIT, side=state.MAP_FULL):
    """Step until the tile word holds still, and answer where it stopped."""
    here = at(run, side)
    still = 0
    frames = 0
    while still < STILL_FRAMES and frames < limit:
        run.step(1)
        frames += 1
        now = at(run, side)
        still = still + 1 if now == here else 0
        here = now
    return here, frames


def move_to(run, target, steady=False, limit=MOVE_LIMIT,
            side=state.MAP_FULL):
    """Steer onto a tile; steady waits out the ghost's late tile step."""
    target = reachable(tuple(target), side)
    began = run.frames()
    start = at(run, side)
    tries = 0
    landed = start
    while tries < STEADY_TRIES:
        push_to(run, target, limit, side)
        tries += 1
        landed = at(run, side)
        if not steady:
            break
        landed, _ = stand_still(run, STILL_LIMIT, side)
        if landed == target:
            break
    return {"verb": "move_to", "start": start, "target": target,
            "tile": landed, "landed": landed == target, "tries": tries,
            "frames": run.frames() - began, "cost": cost_of(start, target),
            "waited": ("radar_x", "radar_y")}


def let_go(run):
    """Drop what is held: with a unit held a press orders it, never picks."""
    tap(run, B)


def select(run, target, side=state.MAP_FULL):
    """Steer onto a tile and press until the game names what it holds."""
    target = tuple(target)
    began = run.frames()
    answer = None
    tries = 0
    while tries < SELECT_TRIES and answer is None:
        let_go(run)
        moved = move_to(run, target, side=side)
        tap(run, A)
        answer = waited_for(run, lambda: picked(run, side) == target,
                            PICK_FRAMES)
        tries += 1
    return {"verb": "select", "target": target, "moved": moved,
            "picked": picked(run, side), "held": picked(run, side) == target,
            "answered": answer, "tries": tries,
            "frames": run.frames() - began,
            "waited": ("picked_x", "picked_y")}


def slot_at(run, tile, side=state.MAP_FULL):
    """Answer the unit slot standing on that tile, from the board itself."""
    shot = state.Snapshot.of(run, side)
    for slot, unit in shot.units.items():
        if unit["tile"] == tuple(tile):
            return slot, unit
    return None, None


def ordering(run, slot):
    at_order = state.UNIT_BASE + slot * state.UNIT_STRIDE + state.ORDER
    return run.memory("system", at_order, 1)[0]


def order_to(run, verb, target, side=state.MAP_FULL):
    """Press on a tile with a unit held; its order byte says the press took."""
    target = tuple(target)
    began = run.frames()
    slot, unit = slot_at(run, picked(run, side), side)
    moved = move_to(run, target, side=side)
    tap(run, A)
    answer = None
    if slot is not None:
        answer = waited_for(run, lambda: ordering(run, slot) != 0,
                            ORDER_FRAMES)
    return {"verb": verb, "target": target, "slot": slot, "moved": moved,
            "was": unit["tile"] if unit else None,
            "taken": answer is not None, "answered": answer,
            "frames": run.frames() - began, "waited": ("unit order byte",)}


def order_move(run, target, side=state.MAP_FULL):
    return order_to(run, "order_move", target, side)


def order_attack(run, target, side=state.MAP_FULL):
    return order_to(run, "order_attack", target, side)


def unit_tile(run, slot, side=state.MAP_FULL):
    at_unit = state.UNIT_BASE + slot * state.UNIT_STRIDE
    raw = run.memory("system", at_unit, state.UNIT_STRIDE)
    inset = inset_of(side)
    return ((state.record_word(raw, state.UNIT_X) >> state.TILE_SHIFT) - inset,
            (state.record_word(raw, state.UNIT_Y) >> state.TILE_SHIFT) - inset)


def arrived(run, slot, limit=GHOST_FRAMES, side=state.MAP_FULL):
    """Wait out an order; the byte clears where the unit stops."""
    began = run.frames()
    answer = waited_for(run, lambda: ordering(run, slot) == 0, limit)
    return {"verb": "arrived", "slot": slot, "answered": answer,
            "tile": unit_tile(run, slot, side),
            "frames": run.frames() - began}


def yard_of(run, side=state.MAP_FULL):
    """Answer this house's construction yard, the lowest slot it owns."""
    shot = state.Snapshot.of(run, side)
    mine = shot.mine(shot.buildings, shot.house)
    return mine[min(mine)] if mine else None


def mode(run):
    return word(run, MODE) >> MODE_SHIFT


def list_up(run):
    return mode(run) in LIST_MODES


def open_panel(run, tile, side=state.MAP_FULL):
    """Select a building and press again; the screen word says it is up."""
    began = run.frames()
    held, answer, tries = None, None, 0
    while tries < PICK_TRIES and answer is None:
        let_go(run)
        held = select(run, tile, side)
        tap(run, A)
        answer = waited_for(run, lambda: list_up(run), LIST_FRAMES)
        tries += 1
    return {"verb": "open_panel", "tile": tuple(tile), "held": held,
            "open": list_up(run), "answered": answer, "tries": tries,
            "row": word(run, PANEL_ROW), "column": word(run, PANEL_COLUMN),
            "frames": run.frames() - began, "waited": ("mode",)}


def open_build_list(run, side=state.MAP_FULL):
    """Select the yard and press again; the screen word says the list is up."""
    yard = yard_of(run, side)
    if yard is None:
        return {"verb": "open_build_list", "open": False, "why": "no yard"}
    told = open_panel(run, yard["tile"], side)
    return dict(told, verb="open_build_list", yard=yard["tile"])


def walk_to(run, address, want, buttons):
    """Tap one axis of the list until its word reads what is asked."""
    for _ in range(PANEL_TRIES):
        here = word(run, address)
        if here == want:
            return True
        tap(run, buttons[1 if here < want else 0], PANEL_HOLD)
        waited_for(run, lambda: word(run, address) != here, PANEL_ANSWER)
    return word(run, address) == want


def seat_of(item):
    index = ITEMS[item]
    return FIRST_ITEM_ROW + index // ITEM_COLUMNS, index % ITEM_COLUMNS


def seat_walk(run, row, column):
    """Walk from EXIT: the seat words still hold the last order's own."""
    walk_to(run, PANEL_ROW, EXIT_ROW, ("up", "down"))
    walk_to(run, PANEL_COLUMN, EXIT_COLUMN, ("left", "right"))
    return (walk_to(run, PANEL_ROW, row, ("up", "down"))
            and walk_to(run, PANEL_COLUMN, column, ("left", "right")))


def step_to(run, address, button):
    """Tap one way along the list and answer whether the word moved."""
    here = word(run, address)
    for _ in range(EDGE_TRIES):
        tap(run, button, PANEL_HOLD)
        waited_for(run, lambda: word(run, address) != here, PANEL_ANSWER)
        if word(run, address) != here:
            return True
    return False


def badge(run):
    return word(run, PANEL_PRICE), word(run, PANEL_GEAR)


def survey(run):
    """Answer the price and gear the list draws on every seat it has."""
    began = run.frames()
    walk_to(run, PANEL_ROW, EXIT_ROW, ("up", "down"))
    walk_to(run, PANEL_COLUMN, EXIT_COLUMN, ("left", "right"))
    seats = {}
    while True:
        while True:
            seats[(word(run, PANEL_ROW), word(run, PANEL_COLUMN))] = badge(run)
            if not step_to(run, PANEL_COLUMN, "right"):
                break
        walk_to(run, PANEL_COLUMN, EXIT_COLUMN, ("left", "right"))
        if not step_to(run, PANEL_ROW, "down"):
            break
    return {"verb": "survey", "seats": seats, "frames": run.frames() - began,
            "waited": ("panel_row", "panel_column", "panel_price",
                       "panel_gear")}


def seat_for(seats, item):
    """Answer the seat whose two words are that item's, or None."""
    want = BADGES.get(item)
    for seat in sorted(seats):
        if seat[0] != EXIT_ROW and seats[seat] == want:
            return seat
    return None


def item(run):
    """The id of the item the list's cursor sits on, as a record kinds it."""
    return run.memory("system", PANEL_ITEM, 1)[0]


def pick(run, row, column=EXIT_COLUMN):
    """Walk the list's words onto a seat and press; the price says which."""
    began = run.frames()
    seated, price, tries = False, PANEL_NONE, 0
    while tries < PICK_TRIES and not (seated and price != PANEL_NONE):
        seated = seat_walk(run, row, column)
        price = word(run, PANEL_PRICE)
        tries += 1
    if price == PANEL_NONE or not seated:
        return {"verb": "pick", "row": row, "column": column,
                "seated": seated, "price": price, "ordered": False,
                "item": item(run),
                "why": "the seat carries no price" if price == PANEL_NONE
                else "the cursor never reached the seat", "tries": tries,
                "frames": run.frames() - began, "waited": ("panel_price",)}
    chosen = item(run)
    tap(run, A)
    answer = waited_for(run, lambda: not list_up(run), LIST_FRAMES)
    return {"verb": "pick", "row": row, "column": column,
            "seated": seated, "price": price, "tries": tries,
            "item": chosen, "ordered": not list_up(run), "answered": answer,
            "frames": run.frames() - began,
            "waited": ("panel_row", "panel_column", "panel_price", "mode")}


def close_build_list(run):
    """EXIT is the list's own way out; no button on the pad closes it."""
    began = run.frames()
    if not list_up(run):
        return {"verb": "close_build_list", "open": False, "frames": 0}
    walk_to(run, PANEL_ROW, EXIT_ROW, ("up", "down"))
    walk_to(run, PANEL_COLUMN, EXIT_COLUMN, ("left", "right"))
    tap(run, A)
    answer = waited_for(run, lambda: not list_up(run), LIST_FRAMES)
    return {"verb": "close_build_list", "open": list_up(run),
            "answered": answer, "frames": run.frames() - began,
            "waited": ("mode",)}


def fix(run):
    """Press the list's FIX tab; the yard repairs what the game holds."""
    began = run.frames()
    if not list_up(run):
        return {"verb": "fix", "open": False, "why": "no list"}
    walk_to(run, PANEL_ROW, EXIT_ROW, ("up", "down"))
    seated = walk_to(run, PANEL_COLUMN, FIX_COLUMN, ("left", "right"))
    tap(run, A)
    answer = waited_for(run, lambda: not list_up(run), LIST_FRAMES)
    return {"verb": "fix", "seated": seated, "open": list_up(run),
            "answered": answer, "frames": run.frames() - began,
            "waited": ("panel_column", "mode")}


def crop(run):
    """Answer the ghost crop around the cursor, kept inside the frame."""
    left = min(max(word(run, CURSOR_X) - GHOST_CROP // 2, 0),
               SCREEN_WIDTH - GHOST_CROP)
    top = min(max(word(run, CURSOR_Y) - GHOST_CROP // 2, 0),
              SCREEN_HEIGHT - GHOST_CROP)
    return f"{left},{top},{GHOST_CROP},{GHOST_CROP}"


def ghost(run):
    """Answer allowed, refused or None for the footprint under the cursor."""
    box = crop(run)
    if run.colours(box, ALLOWED_RGB, 0) >= GHOST_LEAST:
        return "allowed"
    if run.colours(box, REFUSED_RGB, 0) >= GHOST_LEAST:
        return "refused"
    return None


def ghost_anchor(run):
    """Answer the predicate that holds while either ghost is under it."""
    box = crop(run)
    return " or ".join(
        f"colour {rgb[0]},{rgb[1]},{rgb[2]} within 0 over {box}"
        f" at least {GHOST_LEAST}" for rgb in (ALLOWED_RGB, REFUSED_RGB))


def cell_at(run, tile, side=state.MAP_FULL):
    inset = inset_of(side)
    at_tile = (state.MAP_BASE + (tile[1] + inset) * state.MAP_ROW
               + (tile[0] + inset) * state.MAP_TILE)
    return list(run.memory("system", at_tile, state.MAP_TILE))


def stands(run, building, tile, side=state.MAP_FULL):
    """Answer whether what was ordered now stands on that tile."""
    cell = cell_at(run, tile, side)
    if building == CONCRETE:
        return state.is_concrete(cell)
    return bool(cell[state.FLAGS] & state.FLAG_BUILDING)


def place(run, building, target, limit=GHOST_FRAMES, side=state.MAP_FULL):
    """Wait out the build, steer the ghost onto a tile and set it down."""
    began = run.frames()
    target = reachable(tuple(target), side)
    ready = waited_for(run, ghost_anchor(run), limit)
    if ready is None:
        return {"verb": "place", "building": building, "target": target,
                "placed": False, "why": "no ghost",
                "frames": run.frames() - began}
    moved = move_to(run, target, steady=True, side=side)
    judged = ghost(run)
    if judged != "allowed":
        return {"verb": "place", "building": building, "target": target,
                "moved": moved, "ghost": judged, "ready": ready,
                "placed": False, "why": "refused",
                "frames": run.frames() - began}
    tap(run, A)
    answer = waited_for(run, lambda: stands(run, building, target, side),
                        PLACE_FRAMES)
    return {"verb": "place", "building": building, "target": target,
            "moved": moved, "ghost": judged, "ready": ready,
            "placed": stands(run, building, target, side),
            "answered": answer, "frames": run.frames() - began,
            "waited": ("the map's own cell",)}
