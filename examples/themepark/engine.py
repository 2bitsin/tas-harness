"""Measures a Theme Park year: the build, the takings, the offer, visitors."""

import pathlib

import tash

HERE = pathlib.Path(__file__).resolve().parent
TAPE = HERE / "tapes" / "title-to-park.yaml"

# Genesis A is retro y, Genesis B is retro b: the core's retropad has no c.
PICK = "y"
QUICK_MENU = "b"

TAP_HOLD = 6
TAP_AFTER = 30
STEP_HOLD = 2
STEP_AFTER = 2

# The icon menu's pointer lives in a 640 by 448 space. Row one holds paths,
# queues, rides and shops; the coin icon that opens the bank is row three.
PATHS_ICON = (240, 138)
RIDES_ICON = (336, 138)
SHOPS_ICON = (384, 138)
BANK_ICON = (336, 234)
POINTER_NEAR = 12
POINTER_LIMIT = 160

# The bank screen's arrows and its tick, in the same space.
RESEARCH_UP = (526, 364)
TICKET_UP = (526, 408)
BANK_TICK = (592, 400)
TICKET_PRICE = 0x1d9c
RESEARCH_CLICKS = 8

# The park cursor is in tiles: the gate is 23,41 and the plot runs to 1,1.
GATE_X = 23
GATE_TOP = 41
PATH_TOP = 34
RIDE_TILE = (22, 33)
RIDE_ENTRANCE = (23, 34)
RIDE_EXIT = (21, 34)
SHOP_TILE = (26, 36)
RIDE_TWO_TILE = (29, 33)
RIDE_TWO_ENTRANCE = (29, 34)
RIDE_TWO_EXIT = (27, 33)
GLIDE_LIMIT = 400

# The rides quick menu is CASTLE, TREE HOUSE, MERRYGOROUND, GHOST HOUSE and
# the shops quick menu COFFEE SHOP, MR. WALLEY, BALLOON LAND, HOOK A DUCK.
CASTLE = 0
TREE_HOUSE = 1
COFFEE_SHOP = 0

# A year is about 16,000 frames and the year end replaces the park.
YEAR_CHUNK = 200
YEAR_CHUNKS = 120
SCREEN_CHANGED = 0.45
SETTLE = 200

OVERHEAD_A_MONTH = 395
MONTHS_BILLED = 11
MONTHS_A_YEAR = 12
NEXT_PARK = 500000
DETERMINISM_FRAMES = 300

run = tash.run
run.pace(0)


def tap(button, hold=TAP_HOLD, after=TAP_AFTER):
    run.hold(1, [button])
    run.step(hold)
    run.release(1)
    run.step(after)


def watch(name):
    return run.watch(name)


def ticket_price():
    low, high = run.memory("system", TICKET_PRICE, 2)
    return low | (high << 8)


def point_to(target, limit=POINTER_LIMIT):
    """Walk the menu pointer towards a point; its step is not a whole icon."""
    want_x, want_y = target
    for _ in range(limit):
        at_x, at_y = watch("pointer_x"), watch("pointer_y")
        if abs(at_x - want_x) < POINTER_NEAR:
            if abs(at_y - want_y) < POINTER_NEAR:
                return at_x, at_y
            tap("down" if at_y < want_y else "up", STEP_HOLD, STEP_AFTER)
        else:
            tap("right" if at_x < want_x else "left", STEP_HOLD, STEP_AFTER)
    return watch("pointer_x"), watch("pointer_y")


def glide(button, axis, target):
    """Hold a direction until the park cursor reaches that tile coordinate."""
    run.hold(1, [button])
    for _ in range(GLIDE_LIMIT):
        run.step(2)
        if watch(axis) == target:
            break
    run.release(1)
    run.step(4)
    return watch(axis)


def goto(tile):
    glide("right" if watch("cursor_x") < tile[0] else "left",
          "cursor_x", tile[0])
    glide("down" if watch("cursor_y") < tile[1] else "up",
          "cursor_y", tile[1])
    return watch("cursor_x"), watch("cursor_y")


def take_tool(icon):
    tap("start")
    point_to(icon)
    tap(PICK, TAP_HOLD, 60)


def take_from_quick_menu(icon, steps):
    tap("start")
    point_to(icon)
    tap(QUICK_MENU)
    for _ in range(steps):
        tap("right", 3, 20)
    tap(PICK, TAP_HOLD, 40)


def lay_path():
    """Paths cost nothing; the gate stub ends at 41, the ride sits at 34."""
    take_tool(PATHS_ICON)
    for tile_y in range(GATE_TOP, PATH_TOP - 1, -1):
        goto((GATE_X, tile_y))
        tap(PICK, TAP_HOLD, 26)


def place_ride(steps, body, entrance, exit_tile):
    take_from_quick_menu(RIDES_ICON, steps)
    goto(body)
    tap(PICK, TAP_HOLD, 60)
    goto(entrance)
    tap(PICK, TAP_HOLD, 60)
    goto(exit_tile)
    tap(PICK, TAP_HOLD, 60)


def place_shop(steps, tile):
    take_from_quick_menu(SHOPS_ICON, steps)
    goto(tile)
    tap(PICK, TAP_HOLD, 60)


def open_bank():
    tap("start")
    point_to(BANK_ICON)
    tap(PICK, TAP_HOLD, 120)


def close_bank():
    point_to(BANK_TICK)
    tap(PICK, TAP_HOLD, 120)


def raise_ticket_price():
    """The arrow repeats while held; the price stops at the menu's ceiling."""
    open_bank()
    point_to(TICKET_UP)
    was = -1
    while ticket_price() != was:
        was = ticket_price()
        run.hold(1, [PICK])
        run.step(600)
        run.release(1)
        run.step(20)
    price = ticket_price()
    run.look()
    close_bank()
    return price


def set_research():
    open_bank()
    point_to(RESEARCH_UP)
    for _ in range(RESEARCH_CLICKS):
        tap(PICK, 4, 8)
    run.look()
    close_bank()


def build(shape):
    """Builds one park shape from the live park and reports what it cost."""
    started = run.frames()
    lay_path()
    place_ride(CASTLE, RIDE_TILE, RIDE_ENTRANCE, RIDE_EXIT)
    place_shop(COFFEE_SHOP, SHOP_TILE)
    if shape == "two rides":
        place_ride(TREE_HOUSE, RIDE_TWO_TILE, RIDE_TWO_ENTRANCE,
                   RIDE_TWO_EXIT)
    price = raise_ticket_price()
    if shape == "research":
        set_research()
    run.look()
    return {"frames": run.frames() - started, "price": price,
            "value": watch("park_value"), "cash": watch("cash")}


def wait_for_year_end(hunt=None):
    """Steps to the year end, asking a hunt for a riser at every month end."""
    month = watch("month")
    months = []
    run.observe()
    for _ in range(YEAR_CHUNKS):
        run.step(YEAR_CHUNK)
        if watch("month") != month:
            month = watch("month")
            months.append((month, watch("cash")))
            if hunt is not None:
                hunt.step("increased")
        changed = run.observe()["change"] > SCREEN_CHANGED
        if changed and len(months) >= MONTHS_A_YEAR:
            run.step(SETTLE)
            return run.frames(), months
    return None, months


def read_year_end():
    """Dismisses whatever the year end opened with and reads the details."""
    tap(PICK, TAP_HOLD, 120)
    run.look()
    tap("down", 3, 6)
    tap(PICK, TAP_HOLD, 120)
    run.look()
    value = watch("sale_value")
    return {"value": value, "cash": watch("cash"), "loan": watch("loan"),
            "offer": 1000 * (2 * value // 1000)}


def shoot_catalogue():
    """Leaves the year end and shows what the rides quick menu now holds."""
    tap(PICK, TAP_HOLD, 150)
    tap("start")
    point_to(RIDES_ICON)
    tap(QUICK_MENU, TAP_HOLD, 60)
    return run.look()


def measure(shape, hunt=None):
    """One shape: build it, keep the checkpoint, and read its year end."""
    run.mark(shape, "shape")
    run.restore("park")
    built = build(shape)
    run.checkpoint(f"built {shape}")
    print(f"{shape}: built in {built['frames']} frames, cash {built['cash']},"
          f" park value {built['value']}, ticket price {built['price']}",
          flush=True)
    year_end, months = wait_for_year_end(hunt)
    run.expect(f"{shape} reached a year end", year_end is not None)
    read = read_year_end()
    overheads = OVERHEAD_A_MONTH * MONTHS_BILLED
    spent = built["cash"] - read["cash"]
    takings = overheads + built["value"] - spent
    estimate = read["cash"] + read["offer"] - read["loan"]
    row = dict(built, shape=shape, months=months, year_end=year_end,
               overheads=overheads, takings=takings, estimate=estimate, **read)
    print(f"{shape}: spent {spent} over the year", flush=True)
    print(f"{shape}: year end at frame {year_end}, cash {read['cash']},"
          f" value {read['value']}, loan {read['loan']},"
          f" offer {read['offer']}, takings {takings},"
          f" estimated balance {estimate}", flush=True)
    print(f"{shape}: cash at the month ends {months}", flush=True)
    if shape == "research":
        print(f"{shape}: rides catalogue {shoot_catalogue()}", flush=True)
    return row


def determinism(shape):
    """Plays 300 frames straight, then the same 300 from the restore."""
    run.restore(f"built {shape}")
    run.step(DETERMINISM_FRAMES)
    straight = tash.exact_hash()
    run.restore(f"built {shape}")
    run.step(DETERMINISM_FRAMES)
    restored = tash.exact_hash()
    print(f"straight  {straight:016x}", flush=True)
    print(f"restored  {restored:016x}", flush=True)
    run.judge("300 frames straight match 300 restored", straight == restored,
              f"{straight:016x} against {restored:016x}")
    return straight, restored


def years_to_sale(row):
    """How many repeats of this year reach the 500,000 the next park costs."""
    gain = row["takings"] - row["overheads"]
    if gain <= 0:
        return None
    return -(-(NEXT_PARK - row["estimate"]) // gain)


def main():
    run.mark("tape")
    played = run.play(TAPE)
    run.expect("the tape reached the park", played["segments"] == 5,
               f"{played['segments']} segments, {played['frames']} frames")
    run.expect("the park opens with 200,000", watch("cash") == 200000)
    run.checkpoint("park")
    print(f"park at frame {run.frames()}, cash {watch('cash')}", flush=True)

    hunt = run.hunt("system", width=2, endian="little")
    hunt.step("changed")
    rows = [measure("one ride and a shop", hunt)]
    counters = [c for c in hunt.candidates(64) if c[1] < 250]
    print(f"visitor candidates {hunt.candidates(8)}", flush=True)
    print(f"of visitor size {counters}", flush=True)
    determinism("one ride and a shop")
    rows.append(measure("research"))
    rows.append(measure("two rides"))
    for row in rows:
        print(f"| {row['shape']} | {row['frames']} | {row['cash']} |"
              f" {row['takings']} | {row['overheads']} | {row['value']} |"
              f" {row['offer']} | {years_to_sale(row)} |", flush=True)


if __name__ == "__main__":
    main()
