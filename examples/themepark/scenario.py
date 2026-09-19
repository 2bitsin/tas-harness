"""Theme Park from power on: one ride, a year, then ask to sell the park."""

import pathlib

import tash

HERE = pathlib.Path(__file__).resolve().parent

# Genesis A is retro y, Genesis B is retro b: the core's retropad has no c.
PICK = "y"
QUICK_MENU = "b"

TAP_HOLD = 6
TAP_AFTER = 30
POINT_HOLD = 2
POINT_AFTER = 2

# The icon menu opens with the pointer on the first icon at 240 and the
# icons stand 48 apart; the rides icon is the third.
RIDES_ICON_X = 336

# Fourth entry of the rides quick menu, and the dearest thing a new park can
# buy: 20,000 of park value in one placement.
GHOST_HOUSE_STEPS = 3

# The plot is 46 by 43 tiles; everything below y 33 is the entrance strip,
# the river and the road, and refuses a ride.
BUILD_X = 9
BUILD_Y = 12

GLIDE_LIMIT = 400
YEAR_CHUNK = 400
YEAR_CHUNKS = 60
SCREEN_CHANGED = 0.45

# The sell box sits in the bottom left of the year end details screen, in the
# 640 by 448 space the menu pointer uses.
SELL_BOX_X = 40

run = tash.run
run.pace(0)


def tap(button, hold=TAP_HOLD, after=TAP_AFTER):
    run.hold(1, [button])
    run.step(hold)
    run.release(1)
    run.step(after)


def watch(name):
    return run.observe()["watches"][name]


def point_to(x, limit=60):
    """Walk the menu pointer towards x; its step is not a whole icon."""
    for _ in range(limit):
        at = watch("pointer_x")
        if abs(at - x) < 12:
            return at
        tap("right" if at < x else "left", POINT_HOLD, POINT_AFTER)
    return watch("pointer_x")


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


run.mark("tape")
played = run.play(HERE / "tapes" / "title-to-park.yaml")
run.checkpoint("park")
opening_cash = watch("cash")
run.expect("the tape reached the park", played["segments"] == 5,
           f"{played['segments']} segments, {played['frames']} frames")
run.expect("the park opens with 200,000", opening_cash == 200000,
           f"cash {opening_cash}")
park_frame = run.frames()
print(f"park at frame {park_frame}, cash {opening_cash},"
      f" loan {watch('loan')}, day {watch('day')} month {watch('month')}")

run.mark("build")
tap("start")
point_to(RIDES_ICON_X)
tap(QUICK_MENU)
for _ in range(GHOST_HOUSE_STEPS):
    tap("right", 3, 20)
tap(PICK, TAP_HOLD, 40)
glide("up", "cursor_y", BUILD_Y)
glide("left", "cursor_x", BUILD_X)
placed_at = run.frames()
tap(PICK)
value = watch("park_value")
tap(PICK)
tap(PICK)
built_frame = run.frames()
run.expect("the ghost house stands", value == 20000, f"park value {value}")
print(f"ghost house placed at tile {watch('cursor_x')},{watch('cursor_y')}"
      f" on frame {placed_at}, locked by {built_frame},"
      f" park value {value}, cash {watch('cash')}")
run.look()

run.mark("year")
run.observe()
year_end = None
for _ in range(YEAR_CHUNKS):
    run.step(YEAR_CHUNK)
    if run.observe()["change"] > SCREEN_CHANGED:
        year_end = run.frames()
        break
run.expect("the year ended", year_end is not None)
run.step(200)
print(f"year end charts at frame {year_end},"
      f" {year_end - built_frame} frames after the ride")
run.look()

run.mark("sell")
tap("down", 3, 6)
tap(PICK, TAP_HOLD, 90)
details_frame = run.frames()
sale_value = watch("sale_value")
offer = 1000 * (2 * sale_value // 1000)
print(f"year end details at frame {details_frame}: park value {sale_value},"
      f" cash {watch('cash')}, loan {watch('loan')}, offer {offer}")
run.look()

point_to(SELL_BOX_X, limit=80)
tap(PICK, TAP_HOLD, 90)
sale_frame = run.frames()
print(f"sale screen at frame {sale_frame},"
      f" {sale_frame - park_frame} frames from the empty park")
run.look()

estimate = watch("cash") + offer - watch("loan")
run.judge("the sale was refused", estimate < 500000,
          f"estimated balance {estimate} against a 500,000 next park")
print(f"estimated balance after the sale {estimate}")
