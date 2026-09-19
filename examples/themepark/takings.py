"""Step 2b: what Theme Park wants before a visitor pays, a change at a time."""

import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import tash
import engine

run = tash.run
run.pace(0)

watch = engine.watch
tap = engine.tap

QUEUE_ICON = (288, 138)
STAFF_ICON = (288, 186)

# The gate's two lanes end at 23,41 and at 29,38; the spine joins the first
# and the arm the second. The ride and the shop sit off the spine's top.
SPINE = [(23, tile_y) for tile_y in range(41, 33, -1)]
ARM = [(tile_x, 38) for tile_x in range(24, 31)]
LONG_QUEUE = [(22, 34), (22, 35), (23, 35)]
BLOB = [(tile_x, tile_y) for tile_y in range(41, 33, -1)
        for tile_x in range(20, 29)]
CAMERA_TILE = (23, 38)
GATE_TILE = (23, 41)
GHOST_HOUSE = 3

MOVE_TRIES = 12
MOVE_FRAMES = 240
PLACE_AFTER = 26
BUILD_AFTER = 60
HOLD_FRAMES = 24
WATCH_CHUNK = 100
TEST_MONTHS = 3

# A held direction walks the cursor a tile every four frames, and when it
# is let go the cursor takes one more step three frames later: a move is
# over only once the cursor has stood still for longer than that. Under a
# held tool that last step lays a tile too, so a drag lets the direction
# go one tile early and the step it leaves lands the run's last tile.
# Measured: a run of two or three never takes that step at all and is
# pressed instead, and on the up leg a longer run sometimes does not take
# it either, so whatever the drag did not cross is pressed after it.
DIRECTION = {(1, 0): "right", (-1, 0): "left",
             (0, 1): "down", (0, -1): "up"}
SETTLE_FRAMES = 16
BUTTON_GAP = 2
DRAG_LIMIT = 400
DRAG_MIN = 5

DRAG_ROW = [(tile_x, 30) for tile_x in range(10, 30)]
PAD_FRAMES = 2600
TAP_AFTERS = [60, 26, 12, 6, 2, 0]
PULSE_PERIODS = [0, 2]
DRAG_HOLDS = [12, 24, 40, 56, 72, 88]
BAND_LENGTHS = [5, 9, 17, 21]
DRAG_LEGS = [("right", (1, 0), (10, 30)), ("left", (-1, 0), (29, 30)),
             ("up", (0, -1), (23, 41)), ("down", (0, 1), (23, 20))]

# The park record starts at 68000 $ffff1d7a, which the core keeps byte
# swapped: the byte at 68000 offset n is region offset n xor 1.
RECORD_BASE = 0x1d7a
RECORD_SIZE = 0x180

FARE_POLL = 2
FARE_CLICKS = 400
MENU_REWIND = 8
MENU_STEP_HOLD = 3
MENU_STEP_AFTER = 6
MENU_SETTLE = 24
FARE_TARGETS = [50, 60, 70, 76, 80, 84, 88, 92, 100, 200, 500, 1000]
BEST_FARE = 88

GATE_STEP = 30
GATE_SHOTS = 220
HUNT_WINDOW = 300
HUNT_ROUNDS = 4
HUNT_LIST = 30

PHASE = os.environ.get("TAKINGS_PHASE", "fields")


def move_to(tile):
    """Lands the cursor on exactly that tile: engine.glide overshoots by one."""
    want_x, want_y = tile
    for _ in range(MOVE_TRIES):
        at = (watch("cursor_x"), watch("cursor_y"))
        if at == tile:
            return at
        if at[0] != want_x:
            axis, want = "cursor_x", want_x
            button = "right" if at[0] < want_x else "left"
        else:
            axis, want = "cursor_y", want_y
            button = "down" if at[1] < want_y else "up"
        run.hold(1, [button])
        for _ in range(MOVE_FRAMES):
            run.step(1)
            if watch(axis) == want:
                break
        run.release(1)
        run.step(SETTLE_FRAMES)
    return (watch("cursor_x"), watch("cursor_y"))


def straight_runs(tiles):
    """Splits tiles into runs of neighbours that keep going the same way."""
    runs = []
    step = None
    for tile in tiles:
        if runs:
            from_last = (tile[0] - runs[-1][-1][0],
                         tile[1] - runs[-1][-1][1])
            if from_last in DIRECTION and step in (None, from_last):
                runs[-1].append(tile)
                step = from_last
                continue
        runs.append([tile])
        step = None
    return runs


def press_run(tiles):
    """A run too short to drag: three tiles or fewer take no last step."""
    stood = []
    for tile in tiles:
        stood.append(move_to(tile))
        tap(engine.PICK, engine.TAP_HOLD, PLACE_AFTER)
    return stood


def drag_on(tiles):
    """One hold lays every tile the tool crosses, four frames apiece."""
    move_to(tiles[0])
    step = (tiles[1][0] - tiles[0][0], tiles[1][1] - tiles[0][1])
    axis = "cursor_x" if step[0] else "cursor_y"
    stop = tiles[-2][0] if step[0] else tiles[-2][1]
    crossed = [(watch("cursor_x"), watch("cursor_y"))]
    run.hold(1, [engine.PICK, DIRECTION[step]])
    for _ in range(DRAG_LIMIT):
        if watch(axis) == stop:
            break
        run.step(1)
        at = (watch("cursor_x"), watch("cursor_y"))
        if at != crossed[-1]:
            crossed.append(at)
    run.release(1, [DIRECTION[step]])
    run.step(SETTLE_FRAMES)
    at = (watch("cursor_x"), watch("cursor_y"))
    if at != crossed[-1]:
        crossed.append(at)
    run.release(1)
    run.step(BUTTON_GAP)
    return crossed


def draw_on(icon, tiles):
    """Takes a tool and drags it along each straight run of those tiles."""
    engine.take_tool(icon)
    crossed, mended = [], []
    for stretch in straight_runs(tiles):
        walked = (drag_on(stretch) if len(stretch) >= DRAG_MIN
                  else press_run(stretch))
        crossed.extend(walked)
        rest = [tile for tile in stretch if tile not in walked]
        mended.extend(rest)
        crossed.extend(press_run(rest))
    missed = [tile for tile in tiles if tile not in crossed]
    over = [tile for tile in crossed if tile not in tiles]
    run.expect(f"{len(tiles)} tiles laid from {tiles[0]} to {tiles[-1]}",
               not missed and not over,
               f"the tool crossed {len(crossed)} tiles"
               + (f", {len(mended)} of them pressed after the drag"
                  if mended else "")
               + (f", missing {missed}" if missed else "")
               + (f", over {over}" if over else ""))
    return crossed


def place_ride_on(steps, body, entrance, exit_tile):
    """One ride from the rides catalogue, with its entrance and exit locked."""
    engine.take_from_quick_menu(engine.RIDES_ICON, steps)
    for tile in (body, entrance, exit_tile):
        move_to(tile)
        tap(engine.PICK, engine.TAP_HOLD, BUILD_AFTER)


def place_shop_on(steps, tile):
    """One shop from the shops catalogue."""
    engine.take_from_quick_menu(engine.SHOPS_ICON, steps)
    move_to(tile)
    tap(engine.PICK, engine.TAP_HOLD, BUILD_AFTER)


def build_shape(shape):
    """Builds one variant of the park and reports what it cost to build."""
    started = run.frames()
    draw_on(engine.PATHS_ICON, shape.get("paths", SPINE)
            + (ARM if shape.get("arm") else []))
    if shape.get("ride", True):
        place_ride_on(shape.get("catalogue", engine.CASTLE), engine.RIDE_TILE,
                      engine.RIDE_ENTRANCE, engine.RIDE_EXIT)
    if shape.get("queue"):
        draw_on(QUEUE_ICON, shape["queue"])
    if shape.get("shop", True):
        place_shop_on(engine.COFFEE_SHOP, engine.SHOP_TILE)
    for tile in shape.get("staff", []):
        engine.take_from_quick_menu(STAFF_ICON, 0)
        move_to(tile)
        tap(engine.PICK, engine.TAP_HOLD, BUILD_AFTER)
    return {"frames": run.frames() - started, "cash": watch("cash"),
            "value": watch("park_value"), "fare": engine.ticket_price()}


def counters():
    """The park record's own counters, found in the ROM's turnstile routine."""
    return (watch("gate_chance"), watch("peeps_in_park"), watch("peeps_ever"),
            watch("gate_takings"))


def wait_a_month():
    """Runs until the month watch turns over."""
    month = watch("month")
    while watch("month") == month:
        run.step(WATCH_CHUNK)


def run_months(months):
    """Runs whole months, reporting cash and the peep counters at each end."""
    month = watch("month")
    ends = []
    while len(ends) < months:
        run.step(WATCH_CHUNK)
        if watch("month") != month:
            month = watch("month")
            ends.append((month, watch("cash")) + counters())
    return ends


def record_byte(block, offset):
    """One byte of the park record, undoing the core's byte swap."""
    return block[offset ^ 1]


def record_word(block, offset):
    """One 68000 word of the park record."""
    return (record_byte(block, offset) << 8) | record_byte(block, offset + 1)


def record_long(block, offset):
    """One 68000 longword of the park record."""
    return (record_word(block, offset) << 16) | record_word(block, offset + 2)


def record_fields():
    """The fields the entry routine at $48cc8 and the rating at $1a31e read."""
    block = run.memory("system", RECORD_BASE, RECORD_SIZE)
    return {"base": record_byte(block, 0), "gate": record_byte(block, 1),
            "six": record_word(block, 0x06), "fare": record_word(block, 0x22),
            "twenty_six": record_byte(block, 0x26),
            "eight_e": record_long(block, 0x8e),
            "one_oh_e": record_long(block, 0x10e)}


def rating_of(fields, fare):
    """The rating $1a31e returns: a peep enters only when it is below gate."""
    if fields["eight_e"] <= 3:
        return 0
    worth = (((fields["one_oh_e"] >> 1) + fields["eight_e"])
             * fields["six"]) >> 7
    keen = fields["base"] + fields["twenty_six"]
    if worth == 0:
        return keen
    return keen - (fare - worth) * keen // worth


def snap_to_row_one():
    """engine.point_to walks x first and the owned row has nothing right."""
    tap("start")
    run.step(MENU_SETTLE)
    run.hold(1, ["up"])
    run.step(HOLD_FRAMES)
    run.release(1)
    run.step(BUTTON_GAP * 2)


def take(icon, index):
    """The quick menu keeps its highlight, so rewind to zero and count up."""
    snap_to_row_one()
    engine.point_to(icon)
    tap(engine.QUICK_MENU)
    for _ in range(MENU_REWIND):
        tap("left", MENU_STEP_HOLD, MENU_STEP_AFTER)
    for _ in range(index):
        tap("right", MENU_STEP_HOLD, MENU_STEP_AFTER)
    tap(engine.PICK, engine.TAP_HOLD, MENU_SETTLE)


def worth_now():
    """The ROM at $19fa0 prices a visit at this; the fare is judged on it."""
    fields = record_fields()
    return (((fields["one_oh_e"] >> 1) + fields["eight_e"])
            * fields["six"]) >> 7


def open_the_bank():
    """engine.open_bank deadlocks at the panel's foot: snap to row one first."""
    snap_to_row_one()
    engine.point_to(engine.BANK_ICON)
    tap(engine.PICK, engine.TAP_HOLD, 120)
    return run.look()


def set_fare(target):
    """Holds the bank's ticket arrow, which repeats, up to that fare."""
    shot = open_the_bank()
    engine.point_to(engine.TICKET_UP)
    run.hold(1, [engine.PICK])
    for _ in range(FARE_CLICKS):
        if engine.ticket_price() >= target:
            break
        run.step(FARE_POLL)
    run.release(1)
    run.step(BUTTON_GAP)
    fare = engine.ticket_price()
    engine.close_bank()
    return fare, shot


FIELD_SHAPES = [
    ("the bare park", None),
    ("paths only", {"ride": False, "shop": False}),
    ("paths and a ride", {"shop": False}),
    ("paths, ride, shop", {}),
    ("both lanes, railings, shop", {"arm": True, "queue": LONG_QUEUE}),
    ("a ghost house", {"arm": True, "queue": LONG_QUEUE,
                       "catalogue": GHOST_HOUSE}),
]

SHAPES = [
    ("one lane", {}),
    ("both lanes", {"arm": True}),
    ("both lanes, no ride", {"arm": True, "ride": False}),
    ("paths only", {"arm": True, "ride": False, "shop": False}),
    ("railings to the ride", {"arm": True, "queue": LONG_QUEUE}),
    ("a blob of path, no ride", {"paths": BLOB, "ride": False, "shop": False}),
    ("four staff on the path", {"arm": True, "queue": LONG_QUEUE,
                                "staff": [(23, 37), (23, 38), (23, 39),
                                          (23, 40)]}),
    ("a ghost house, a year", {"arm": True, "queue": LONG_QUEUE,
                               "catalogue": GHOST_HOUSE, "months": 12}),
]


def try_shape(name, shape):
    """One variant from the bare park: build it, hold it, say what it took."""
    run.mark(name, "shape")
    run.restore("park")
    built = build_shape(shape)
    ends = run_months(shape.get("months", TEST_MONTHS))
    spent = built["cash"] - ends[-1][1]
    took = spent - built["value"] - engine.OVERHEAD_A_MONTH * len(ends)
    move_to(CAMERA_TILE)
    run.step(60)
    print(f"{name}: built in {built['frames']} frames, value {built['value']},"
          f" fare {built['fare']}, month ends {ends}, took {-took}"
          f" {run.look()}", flush=True)


def field_sweep():
    """What each thing built does to the rating the entry decision compares."""
    for name, shape in FIELD_SHAPES:
        run.restore("park")
        if shape is not None:
            build_shape(shape)
        fields = record_fields()
        print(f"{name}: {fields} value {watch('park_value')} rating "
              f"{rating_of(fields, fields['fare'])}", flush=True)


def fare_sweep():
    """The fare against a month end's takings, from one built park."""
    run.restore("park")
    build_shape({"arm": True, "queue": LONG_QUEUE})
    run.checkpoint("working")
    for target in FARE_TARGETS:
        run.restore("working")
        fare, shot = set_fare(target)
        fields = record_fields()
        ends = run_months(TEST_MONTHS)
        print(f"fare {target} -> {fare} rating {rating_of(fields, fare)}"
              f" gate {fields['gate']} ends {ends} {shot}", flush=True)


def watch_the_gate():
    """The fare that puts the rating under the gate byte, seen at the gate."""
    run.restore("park")
    build_shape({"arm": True, "queue": LONG_QUEUE})
    fare, shot = set_fare(BEST_FARE)
    print(f"fare {fare} fields {record_fields()} {shot}", flush=True)
    for _ in range(2):
        wait_a_month()
    move_to(GATE_TILE)
    run.step(60)
    for shot_number in range(GATE_SHOTS):
        print(f"gate {shot_number} frame {run.frames()} month {watch('month')}"
              f" day {watch('day')} {counters()} {run.look()}", flush=True)
        run.step(GATE_STEP)


def hunt_a_bus():
    """A hunt for the gate's counter: flat between buses, up over a month."""
    run.restore("park")
    build_shape({"arm": True, "queue": LONG_QUEUE})
    for _ in range(2):
        wait_a_month()
    hunt = run.hunt("system", width=2, endian="little")
    for _ in range(HUNT_ROUNDS):
        run.step(HUNT_WINDOW)
        hunt.step("equal")
        wait_a_month()
        print(f"month {watch('month')} survivors {hunt.step('increased')}",
              flush=True)
    print(f"hunt {hunt.candidates(HUNT_LIST)}", flush=True)


def shop_screen():
    """The shop's own screen, which the quick menu opens on its highlight."""
    run.restore("park")
    build_shape({"arm": True, "queue": LONG_QUEUE})
    for _ in range(2):
        wait_a_month()
    tap("start")
    run.step(40)
    tap(engine.PICK, engine.TAP_HOLD, 150)
    print(f"shop {run.look()}", flush=True)



def measured_year():
    """One year of the corrected park at the fare that lowers the rating."""
    run.restore("park")
    built = build_shape({"arm": True, "queue": LONG_QUEUE})
    fare, shot = set_fare(BEST_FARE)
    ends = run_months(12)
    value = watch("park_value")
    print(f"year: built in {built['frames']} frames, cash {built['cash']},"
          f" fare {fare}, value {value}, ends {ends},"
          f" fields {record_fields()} {shot} {run.look()}", flush=True)


def view_of(lay, pose):
    """Lays a band on the bare park and parks the cursor on one tile the
    same number of frames into the run every time, so the date, the
    ticker and the clock cannot be what two views differ by."""
    began = run.frames()
    run.restore("park")
    laid = lay()
    move_to(pose)
    run.step(max(PAD_FRAMES - (run.frames() - began), 0))
    return laid


def change_after(lay, pose, shot):
    view_of(lay, pose)
    return tash.change_from(shot)["changed"]


def lay_by_hold(button, frames, start):
    engine.take_tool(engine.PATHS_ICON)
    move_to(start)
    return hold_for(button, frames)


def lay_by_press(tiles):
    engine.take_tool(engine.PATHS_ICON)
    for tile in tiles:
        move_to(tile)
        tap(engine.PICK, engine.TAP_HOLD, PLACE_AFTER)
    return tiles


def pressed_view(tiles, pose):
    """The way already trusted, and the two numbers that say what its own
    view is worth: the floor two identical layings differ by, and what
    leaving the tile under the cursor out is worth."""
    view_of(lambda: lay_by_press(tiles), pose)
    shot = run.look()
    floor = change_after(lambda: lay_by_press(tiles), pose, shot)
    short = [tile for tile in tiles if tile != pose]
    return shot, floor, change_after(lambda: lay_by_press(short), pose, shot)


def hold_for(button, frames):
    """Holds tool and direction that many frames, reading every one."""
    crossed = [(watch("cursor_x"), watch("cursor_y"))]
    run.hold(1, [engine.PICK, button])
    for _ in range(frames):
        run.step(1)
        at = (watch("cursor_x"), watch("cursor_y"))
        if at != crossed[-1]:
            crossed.append(at)
    run.release(1, [engine.PICK])
    run.step(BUTTON_GAP)
    run.release(1)
    run.step(SETTLE_FRAMES)
    return crossed


def drag_candidates():
    """How far one hold of the tool and a direction walks the cursor: the
    tiles are the watches, read on every frame of the hold."""
    for button, step, start in DRAG_LEGS:
        for frames in DRAG_HOLDS:
            run.restore("park")
            crossed = lay_by_hold(button, frames, start)
            stood = (watch("cursor_x"), watch("cursor_y"))
            print(f"| {button} | {frames} | {len(crossed)} |"
                  f" {frames / len(crossed):.1f} | {crossed[-1]} | {stood} |",
                  flush=True)


def band_of(start, step, length):
    return [(start[0] + step[0] * n, start[1] + step[1] * n)
            for n in range(length)]


def band_checks():
    """What `drag_on` lays, against the same tiles pressed one by one."""
    for button, step, start in DRAG_LEGS:
        for length in BAND_LENGTHS:
            tiles = band_of(start, step, length)
            pose = tiles[len(tiles) // 2]
            shot, floor, short = pressed_view(tiles, pose)
            crossed = []

            def drag():
                crossed.extend(drag_on(tiles))
                return crossed

            dragged = change_after(
                lambda: (engine.take_tool(engine.PATHS_ICON), drag()),
                pose, shot)
            missed = [tile for tile in tiles if tile not in crossed]
            over = [tile for tile in crossed if tile not in tiles]
            print(f"| {button} | {length} | {len(crossed)} | {crossed[-1]} |"
                  f" {len(missed)} | {len(over)} | {floor:.5f} |"
                  f" {dragged:.5f} | {short:.5f} |", flush=True)


def start_row():
    engine.take_tool(engine.PATHS_ICON)
    move_to(DRAG_ROW[0])


def tap_row(after):
    """The old way: to each tile, tap the tool, wait that many frames."""
    start_row()
    for tile in DRAG_ROW:
        move_to(tile)
        tap(engine.PICK, engine.TAP_HOLD, after)


def hold_row(order):
    """The tool and the direction held, in each of the three orders."""
    start_row()
    if order == "tool first":
        run.hold(1, [engine.PICK])
        run.step(BUTTON_GAP)
        run.hold(1, ["right"])
    elif order == "direction first":
        run.hold(1, ["right"])
        run.step(BUTTON_GAP)
        run.hold(1, [engine.PICK])
    else:
        run.hold(1, [engine.PICK, "right"])
    for _ in range(DRAG_LIMIT):
        if watch("cursor_x") == DRAG_ROW[-2][0]:
            break
        run.step(1)
    run.release(1, ["right"])
    run.step(SETTLE_FRAMES)
    run.release(1)
    run.step(BUTTON_GAP)


def pulse_row(period):
    """The direction held and the tool tapped, in case a hold lays one."""
    start_row()
    run.hold(1, ["right"])
    for _ in range(DRAG_LIMIT):
        run.hold(1, [engine.PICK])
        run.step(engine.TAP_HOLD)
        run.release(1, [engine.PICK])
        run.step(period)
        if watch("cursor_x") >= DRAG_ROW[-1][0]:
            break
    run.release(1)
    run.step(SETTLE_FRAMES)


def row(name, lay, pose, shot):
    """One line of the table: the way, its frames, and the band it left."""
    began = run.frames()
    run.restore("park")
    lay()
    frames = run.frames() - began
    stood = (watch("cursor_x"), watch("cursor_y"))
    move_to(pose)
    run.step(max(PAD_FRAMES - (run.frames() - began), 0))
    print(f"| {name} | {frames} | {frames / len(DRAG_ROW):.1f} | {stood} |"
          f" {tash.change_from(shot)['changed']:.5f} |", flush=True)


def drag_sweep():
    """Every way of laying one twenty-tile band, against the same band."""
    drag_candidates()
    band_checks()
    pose = DRAG_ROW[len(DRAG_ROW) // 2]
    shot, floor, short = pressed_view(DRAG_ROW, pose)
    print(f"| pressed again | | | | {floor:.5f} |", flush=True)
    print(f"| pressed, {pose} left out | | | | {short:.5f} |", flush=True)
    row("nothing laid, the tool and the walk only", start_row, pose, shot)
    for after in TAP_AFTERS:
        row(f"tap, {after} frames after",
            lambda after=after: tap_row(after), pose, shot)
    for order in ("both at once", "tool first", "direction first"):
        row(f"hold, {order}", lambda order=order: hold_row(order), pose, shot)
    for period in PULSE_PERIODS:
        row(f"direction held, tap every {period}",
            lambda period=period: pulse_row(period), pose, shot)
    row("draw_on", lambda: draw_on(engine.PATHS_ICON, DRAG_ROW), pose, shot)


PHASES = {"fields": field_sweep, "fares": fare_sweep, "gate": watch_the_gate,
          "hunt": hunt_a_bus, "shop": shop_screen, "drag": drag_sweep,
          "year": measured_year}


def main():
    run.mark("tape")
    run.play(engine.TAPE)
    run.checkpoint("park")
    if PHASE == "shapes":
        for name, shape in SHAPES:
            try_shape(name, shape)
        return
    PHASES[PHASE]()


if __name__ == "__main__":
    main()
