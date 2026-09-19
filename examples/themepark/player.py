"""Step 4b: the catalogue built by held drags, the first year run a frame
at a time, and the December orders searched for first place."""

import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import tash
import engine
import season
import takings
import visitor

run = tash.run
run.pace(0)

PHASE = os.environ.get("THEMEPARK_PHASE", "first")
YEARS = int(os.environ.get("THEMEPARK_YEARS", "3"))

# The gate stub ends at 23,41. The spine climbs from it to the ride row at
# 30, the link carries on to the shop row at 19, and the shops hang below.
SPINE = [(23, y) for y in range(41, 28, -1)]
RIDE_ROW = [(x, 30) for x in range(4, 43)]
LINK = [(23, y) for y in range(28, 18, -1)]
SHOP_ROW = [(x, 19) for x in range(4, 43)]

# Every ride and every shop the unresearched catalogue offers, in menu order:
# 2,000 + 4,000 + 5,000 + 20,000 of rides and 1,000 + 2,000 + 2,000 + 5,000
# of shops, priced by the ROM tables at $6ac1a and $6c79a.
RIDES = [(0, (10, 29), (11, 30), (9, 30)),
         (1, (17, 29), (18, 30), (16, 30)),
         (2, (28, 29), (29, 30), (27, 30)),
         (3, (35, 29), (36, 30), (34, 30))]
SHOPS = [(0, [(10, 25), (13, 25)]), (1, [(17, 25), (20, 25)]),
         (2, [(28, 25), (31, 25)]), (3, [(35, 25), (38, 25)])]
CATALOGUE = 41000

PLACE_AFTER = 30
PLACE_TRIES = 8

START_NET = 100000
FIRST = 1
FIRST_FARE = 160
FRAME_BOUND = 21450
NEXT_PARK = 500000
JANUARY = 0
YEAR_FRAMES = 40000
SCREEN_CHANGE = 0.3
SCREEN_POLL = 30
SCREEN_LIMIT = 400
PANEL_AFTER = 150
SELL_WALK = 12


def press_until_bought(tile):
    """park_value is a bill a month end clears, so watch it rise."""
    takings.move_to(tile)
    was = run.watch("park_value")
    for _ in range(PLACE_TRIES):
        engine.tap(engine.PICK, engine.TAP_HOLD, PLACE_AFTER)
        now = run.watch("park_value")
        if now > was:
            return True
        was = now
    return False


def lay_paths():
    run.mark("paths", "build")
    takings.draw_on(engine.PATHS_ICON, SPINE + RIDE_ROW + LINK + SHOP_ROW)


def place_rides():
    run.mark("rides", "build")
    for index, body, entrance, exit_tile in RIDES:
        takings.take(engine.RIDES_ICON, index)
        press_until_bought(body)
        for tile in (entrance, exit_tile):
            takings.move_to(tile)
            engine.tap(engine.PICK, engine.TAP_HOLD, PLACE_AFTER)


def place_shops():
    run.mark("shops", "build")
    for index, tiles in SHOPS:
        takings.take(engine.SHOPS_ICON, index)
        for tile in tiles:
            if press_until_bought(tile):
                break


def build():
    started = run.frames()
    lay_paths()
    place_rides()
    place_shops()
    return {"frames": run.frames() - started, "cash": run.watch("cash"),
            "worth": takings.worth_now(),
            "fare": engine.ticket_price()}


def open_for_business(fare_wanted=None):
    run.mark("open", "build")
    worth = takings.worth_now()
    fare, _ = takings.set_fare(worth if fare_wanted is None else fare_wanted)
    flag, _, _ = visitor.open_the_park()
    return {"open": flag, "fare": fare, "worth": worth,
            "gate": run.watch("gate_chance")}


def wait_for_screen():
    for _ in range(SCREEN_LIMIT):
        run.step(SCREEN_POLL)
        if run.observe()["change"] > SCREEN_CHANGE:
            return True
    return False


def ask_for_the_sale(year):
    """Charts, tick, details, then the sell line in the bottom left corner."""
    run.mark(f"year {year}", "years")
    run.run_until(f"watch month greater {JANUARY}", YEAR_FRAMES)
    run.run_until(f"watch month equal {JANUARY}", YEAR_FRAMES)
    wait_for_screen()
    engine.tap("down", MENU_STEP_HOLD, 20)
    engine.tap(engine.PICK, engine.TAP_HOLD, PANEL_AFTER)
    value = run.watch("sale_value")
    cash = run.watch("cash")
    loan = run.watch("loan")
    offer = 1000 * (2 * value // 1000)
    offered = run.frames()
    for _ in range(SELL_WALK):
        engine.tap("left", MENU_STEP_HOLD, 10)
    engine.tap(engine.PICK, engine.TAP_HOLD, PANEL_AFTER)
    sold = run.watch("cash") >= cash + offer
    engine.tap(engine.PICK, engine.TAP_HOLD, PANEL_AFTER)
    return {"year": year, "value": value, "cash": cash, "loan": loan,
            "offer": offer, "estimate": cash + offer - loan, "sold": sold,
            "frames": offered}


def report_build(built):
    print(f"built in {built['frames']} frames: cash {built['cash']},"
          f" worth {built['worth']}, fare {built['fare']}", flush=True)


def report_open(gate):
    print(f"park open {gate['open']}, fare {gate['fare']} against worth"
          f" {gate['worth']}, gate byte {gate['gate']}", flush=True)
    run.judge("the park is open", gate["open"] == 1,
              f"byte ${visitor.OPEN_BYTE:02x} of the park record reads"
              f" {gate['open']}")


def judge_the_sale(asked):
    best = max(asked, key=lambda row: row["estimate"])
    sold = [row for row in asked if row["sold"]]
    for row in asked:
        print(f"| {row['year']} | {row['frames']} | {row['value']} |"
              f" {row['offer']} | {row['cash']} | {row['loan']} |"
              f" {row['estimate']} | {row['sold']} |", flush=True)
    run.judge("the build bought the whole catalogue",
              asked[0]["value"] == CATALOGUE,
              f"the first year end priced the park at {asked[0]['value']},"
              f" the catalogue at {CATALOGUE}")
    run.judge("the game accepted the sale", bool(sold),
              f"best estimate {best['estimate']} against the {NEXT_PARK}"
              f" the cheapest unowned plot costs")
    run.judge("the net beat the 100,000 the park started with",
              best["estimate"] > START_NET,
              f"{best['estimate']} at year {best['year']}, from"
              f" {START_NET}")
    run.judge("the frames to the best offer", True,
              f"{best['frames']} frames from power on")
    return best


def report_trials(tried):
    for seen in tried:
        print(f"| {seen['order'][0]},{seen['order'][1]} | {seen['rank']} |"
              f" {seen['key']} | {seen['best_rival']} | {seen['placings']} |"
              f" {seen['numbers']['balance']} |"
              f" {seen['numbers']['return']:.3%} |", flush=True)


def judge_first_place(seen, line, wall):
    numbers = seen["numbers"]
    print(f"first place: rank {seen['rank']}, return"
          f" {numbers['return']:.3%}, {line} frames to the charts"
          f" ({wall} run, the search's restores folded off the line), key"
          f" {seen['key']} against the best rival's {seen['best_rival']},"
          f" placings {seen['placings']}", flush=True)
    run.judge("the first year end takes first place", seen["rank"] == FIRST,
              f"rank {seen['rank']}, key {seen['key']}, the best rival"
              f" {seen['best_rival']}")
    run.judge("the money in the park came back with a return",
              numbers["return"] > 0,
              f"value {numbers['value']} plus balance"
              f" {numbers['balance']} less loan {numbers['loan']} is"
              f" {numbers['net']} on {numbers['capital']} put in")
    run.judge("the first charts inside the frames step 4a argued",
              line <= FRAME_BOUND,
              f"{line} frames on the line, {wall} with the search's"
              f" restores, against {FRAME_BOUND}")


def first_place():
    built = build()
    report_build(built)
    gate = open_for_business(FIRST_FARE)
    report_open(gate)
    year = season.Season()
    year.to_the_charts()
    seen = season.read_the_charts()
    report_trials(year.trials())
    print(f"acts {year.acts()}, search {year.searched()} frames over"
          f" {len(year.trials())} orders", flush=True)
    judge_first_place(seen, seen["frame"] - year.searched(),
                      run.frames())
    return seen


def main():
    run.mark("tape", "build")
    played = run.play(engine.TAPE)
    run.expect("the tape reached the park", "watch cash equal 200000",
               f"{played['segments']} segments, {played['frames']} frames")
    if PHASE == "first":
        first_place()
        return
    built = build()
    report_build(built)
    if PHASE == "build":
        return
    gate = open_for_business()
    report_open(gate)
    if PHASE == "open":
        return
    asked = [ask_for_the_sale(year) for year in range(1, YEARS + 1)]
    judge_the_sale(asked)


if __name__ == "__main__":
    main()
