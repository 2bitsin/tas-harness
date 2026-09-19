"""Atreides missions 1 and 2 from power on, planned over the map array."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import engine
import state

SIDE = state.MAP_WINDOW

# What a harvester crosses: sand with its rock edges, and the spice. Rock and
# anything standing are walls, and both tables read true under the shroud.
SAND_FIRST = 127
SAND_LAST = 142
TILE_COST = 1

# The radar blip clamps at 30 tiles on each axis, so nothing further out can
# be ordered or built on however well it reads.
REACH = 30

# The map's +3 says a pad stands against a building but not whose, and in
# mission 2 the Ordos base offers pads of its own.
PAD_REACH = 6

CONTROL_LIMIT = 1200
SETTLE_FRAMES = 240
PAD_WIDE = 4
PAD_HIGH = 2
TRAP_SIDE = 2
SILO_SIDE = 2
SILOS = 2

# A slab is two tiles square and the cursor sits at its top left, so a bare
# tile is covered from one of four seats.
SLAB_CORNERS = ((0, 0), (-1, 0), (0, -1), (-1, -1))
PAD_TRIES = 4
PAD_JUDGED = 3
QUIET_POLL = 60
QUIET_POLLS = 30
PURSE_POLL = 120
PURSE_POLLS = 200
AWAY_TILES = 4
SPICE_TRIES = 6
DEFEND_RING = 10
HARVEST_POLL = 300
HARVEST_POLLS = 400

# A round trip to the spice and back banks in about fifty polls, so a purse
# that has not moved in more than that has a harvester standing still.
STALL_POLLS = 60
DELIVERY_POLL = 300
DELIVERY_RISE = 40
REFINERY_TRIAL = 12000
SILO_TRIAL = 4000
QUOTA_ONE = 1000
QUOTA_TWO = 2700

# The plate the game draws when a quota is met; its four frames animate and
# the perceptual hash does not.
VICTORY = "perceptual_hash 30b14b4e6e34b8cb within 6"
VICTORY_LIMIT = 6000

# The plate, the region map, the mentat, the score screen and the next
# briefing all advance on Genesis A and on nothing else. The drawn purse
# is the end of them: the cursor word answers while the briefing is still
# up and the building table is still empty.
BRIEF_PRESSES = 24
BRIEF_WAIT = 150
MISSION_TWO = 2
TWO_PURSE = 1200

run = tash.run


def wait_for_control(run):
    """The tape ends inside a zoom; the cursor appears when play starts."""
    for frames in range(CONTROL_LIMIT):
        run.step(1)
        if engine.word(run, engine.CURSOR_X) != 0:
            return frames
    return None


def press_through(run, test, presses=BRIEF_PRESSES):
    """Tap Genesis A until the game's own word answers."""
    pressed = 0
    while pressed < presses and not test():
        engine.tap(run, engine.A)
        engine.waited_for(run, test, BRIEF_WAIT)
        pressed += 1
    return pressed if test() else None


def snap(run):
    return state.Snapshot.of(run, SIDE)


def mine_of(run, table, owner):
    shot = snap(run)
    return shot.mine(getattr(shot, table), owner)


def home(run):
    """Answer my construction yard's tile, from the building table."""
    mine = mine_of(run, "buildings", state.ATREIDES)
    return mine[min(mine)]["tile"]


def board(run):
    return snap(run).seen


def is_sand(terrain):
    return SAND_FIRST <= terrain <= SAND_LAST


def is_spice(terrain):
    return state.SPICE_FIRST <= terrain <= state.SPICE_LAST


def crossable(cell):
    if cell[state.FLAGS] & state.FLAG_BUILDING:
        return False
    return is_sand(cell[state.TERRAIN]) or is_spice(cell[state.TERRAIN])


def costs_of(seen):
    """Answer the map as a cost grid the planner reads."""
    return bytes(TILE_COST if crossable(seen[y][x]) else tash.plan.IMPASSABLE
                 for y in range(SIDE) for x in range(SIDE))


def spice_of(seen):
    return [(x, y) for y in range(REACH) for x in range(REACH)
            if is_spice(seen[y][x][state.TERRAIN])]


def steps(here, there):
    return abs(here[0] - there[0]) + abs(here[1] - there[1])


def is_rock(cell):
    """Rock a slab may cover: a unit on it only keeps the order waiting."""
    return (cell[state.TERRAIN] == state.ROCK
            and not cell[state.FLAGS] & state.FLAG_BUILDING)


def pads(seen, width, height, yard):
    """Answer the rock footprints my own base already touches."""
    return [seat for seat in state.seats(seen, width, height, is_rock)
            if seat[0] + width <= REACH and seat[1] + height <= REACH
            and steps(seat, yard) <= PAD_REACH]


def sanded(costs, spread):
    """Answer every crossable cell the spice reaches, with what it costs."""
    return [(index % SIDE, index // SIDE, reach)
            for index, reach in enumerate(spread)
            if reach != tash.plan.UNREACHED]


def ranked(seats, width, height, reached):
    """A pad is as far from the spice as the sand it crosses to reach it."""
    def cost(seat):
        middle = (seat[0] + width // 2, seat[1] + height // 2)
        return min((reach + abs(middle[0] - x) + abs(middle[1] - y)
                    for x, y, reach in reached), default=tash.plan.UNREACHED)
    return sorted(seats, key=cost), {seat: cost(seat) for seat in seats}


def plan_pads(run, width, height, yard):
    """Rank every candidate pad by the round trip a harvester would make."""
    seen = board(run)
    costs = costs_of(seen)
    spread = tash.plan.distances(costs, SIDE, SIDE, spice_of(seen))
    return ranked(pads(seen, width, height, yard), width, height,
                  sanded(costs, spread))


def footprint(seat, width, height):
    return {(seat[0] + dx, seat[1] + dy)
            for dx in range(width) for dy in range(height)}


def ring(seat, width, height):
    return {(seat[0] + dx, seat[1] + dy)
            for dx in range(-1, width + 1) for dy in range(-1, height + 1)}


def flags(run, tiles):
    seen = board(run)
    return [seen[y][x][state.FLAGS] for x, y in tiles
            if 0 <= x < SIDE and 0 <= y < SIDE]


def on_board(cell):
    return 0 <= cell[0] < SIDE and 0 <= cell[1] < SIDE


def paved(seen):
    """Answer every tile of the board that already carries a slab."""
    return {(x, y) for y in range(SIDE) for x in range(SIDE)
            if state.is_concrete(seen[y][x])}


def bare(run, seat, width, height):
    """Answer the pad's tiles carrying no concrete; a unit keeps a slab off."""
    return footprint(seat, width, height) - paved(board(run))


def slabs(run, seat, width, height):
    """Answer the slab seats that pave the pad, the widest cover first."""
    laid = paved(board(run))
    pad = footprint(seat, width, height)
    seats = {}
    for tile in pad - laid:
        for corner in SLAB_CORNERS:
            at = (tile[0] + corner[0], tile[1] + corner[1])
            cover = footprint(at, TRAP_SIDE, TRAP_SIDE)
            if cover & laid or not all(on_board(cell) for cell in cover):
                continue
            seats[at] = len(cover & pad)
    return sorted(seats, key=lambda at: (-seats[at], at))


def quiet(run, seat, width, height):
    """Step until no unit stands on the pad or beside it."""
    for _ in range(QUIET_POLLS):
        if not any(flag & state.FLAG_UNIT
                   for flag in flags(run, ring(seat, width, height))):
            return True
        run.step(QUIET_POLL)
    return False


def clear(run, seat, width, height, away):
    """Walk my own units off a pad; a slab laid over one leaves a hole."""
    beside = ring(seat, width, height)
    for rec in mine_of(run, "units", state.ATREIDES).values():
        at = rec["tile"]
        if at in beside:
            engine.select(run, at, SIDE)
            engine.order_move(run, away, SIDE)


def afford(run, item):
    """Step until the purse covers that item; the yard refuses it under."""
    for _ in range(PURSE_POLLS):
        if run.watch("credits") >= engine.PRICES[item]:
            return True
        run.step(PURSE_POLL)
    return False


def settle(run, purse):
    """A build is paid 50 credits every 60 frames, so let the purse land."""
    run.step(SETTLE_FRAMES)
    return purse - run.watch("credits")


def order_and_place(run, item, seats):
    """Buy one item from the yard and set it on the first seat allowed."""
    opened = engine.open_build_list(run, SIDE)
    if not opened.get("open"):
        return dict(opened, item=item, placed=False, why="no list")
    row, column = engine.seat_of(item)
    bought = engine.pick(run, row, column)
    if not bought.get("ordered"):
        return dict(bought, item=item, placed=False, why="not ordered")
    laid = {"placed": False, "why": "refused"}
    for seat in (seats() if callable(seats) else seats):
        laid = engine.place(run, item, seat, side=SIDE)
        if laid.get("placed"):
            break
    return dict(laid, item=item, seat=laid.get("target"),
                price=bought.get("price"))


def build(run, item, seats):
    """Order one building and place it; answer its frames and its price."""
    started, purse = run.frames(), run.watch("credits")
    row = order_and_place(run, item, seats)
    row["paid"] = settle(run, purse)
    row["frames"] = run.frames() - started
    return row


def lay(run, seat, width, height, orders):
    """Slab a pad one square at a time until no tile of it is left bare."""
    for _ in range(width * height):
        seats = slabs(run, seat, width, height)
        if not seats:
            return True
        quiet(run, seats[0], TRAP_SIDE, TRAP_SIDE)
        afford(run, "concrete")
        orders.append(build(run, "concrete", seats))
        if not orders[-1].get("placed"):
            break
    return not bare(run, seat, width, height)


def raise_on(run, seat, item, width, height, yard, orders):
    """Pave that pad and put the building on it; answer whether it stands."""
    away = (yard[0], yard[1] - AWAY_TILES)
    clear(run, seat, width, height, away)
    if not lay(run, seat, width, height, orders):
        return False
    afford(run, item)
    heads = [(seat[0] + dx, seat[1]) for dx in range(max(width - 1, 1))]
    orders.append(build(run, item, heads))
    return bool(orders[-1].get("placed"))


def first_load(run, limit):
    """Step until the purse climbs: the harvester's first load has landed."""
    least = run.watch("credits")
    for _ in range(max(limit // DELIVERY_POLL, 1)):
        run.step(DELIVERY_POLL)
        purse = run.watch("credits")
        if purse > least + DELIVERY_RISE:
            return run.frames()
        least = min(least, purse)
    return None


def to_first_load(run, seat, yard):
    """One trial: pave, put the refinery down and wait for its first load."""
    started = run.frames()
    landed = None
    try:
        if raise_on(run, seat, "refinery", PAD_WIDE, PAD_HIGH, yard, []):
            landed = first_load(run,
                                REFINERY_TRIAL - (run.frames() - started))
    except RuntimeError:
        landed = None
    spent = run.frames() - started
    if landed is None and spent < REFINERY_TRIAL:
        run.step(REFINERY_TRIAL - spent)


def to_silo(run, seat, yard):
    """One trial: pave that pad and put a silo on it, or spend the cap."""
    started = run.frames()
    standing = False
    try:
        standing = raise_on(run, seat, "silo", SILO_SIDE, SILO_SIDE, yard, [])
    except RuntimeError:
        standing = False
    spent = run.frames() - started
    if not standing and spent < SILO_TRIAL:
        run.step(SILO_TRIAL - spent)


def fewest_frames(run):
    """Every trial starts on one frame, so the earliest one is the best."""
    return -run.frames()


def judged(run, seats, trial, yard):
    """Let the emulator judge the top pads; the run is left where it was."""
    if len(seats) < 2:
        return seats[0] if seats else None, None
    found = tash.search(seats, lambda run, seat: trial(run, seat, yard),
                        fewest_frames)
    return found.best, found


def choose(run, width, height, trial, yard, told):
    """Rank the pads by the planner, then judge the best few in the core."""
    order, costs = plan_pads(run, width, height, yard)
    if not order:
        return None
    top = order[:PAD_JUDGED]
    best, found = judged(run, top, trial, yard)
    told.append({"pad": f"{width} by {height}", "pads": len(order),
                 "ranked": [(seat, costs[seat]) for seat in top],
                 "chose": best, "trials": found.trials if found else 0,
                 "searched": found.frames if found else 0})
    return best if best is not None else order[0]


def raise_base(run, yard, told):
    """The windtrap is first: the refinery joins the list only behind it."""
    orders = []
    order, costs = plan_pads(run, TRAP_SIDE, TRAP_SIDE, yard)
    trap = order[-1] if order else None
    told.append({"pad": f"{TRAP_SIDE} by {TRAP_SIDE}", "pads": len(order),
                 "ranked": [(seat, costs[seat]) for seat in order[-1:]],
                 "chose": trap, "trials": 0, "searched": 0})
    raise_on(run, trap, "windtrap", TRAP_SIDE, TRAP_SIDE, yard, orders)
    pad = choose(run, PAD_WIDE, PAD_HIGH, to_first_load, yard, told)
    raise_on(run, pad, "refinery", PAD_WIDE, PAD_HIGH, yard, orders)
    return pad, trap, orders


def add_silo(run, yard, orders, told):
    seat = choose(run, SILO_SIDE, SILO_SIDE, to_silo, yard, told)
    raise_on(run, seat, "silo", SILO_SIDE, SILO_SIDE, yard, orders)
    return seat


def reapers(mine, known):
    """The type byte at +51 reads 16 for a harvester and nothing else."""
    return known | {slot for slot, rec in mine.items()
                    if rec["kind"] == state.HARVESTER}


def nearest_spice(costs, at, fields):
    """Answer the spice a route reaches, the nearest when none does."""
    near = sorted(fields, key=lambda tile: steps(tile, at))[:SPICE_TRIES]
    best, cheapest = None, tash.plan.UNREACHED
    for tile in near:
        walked = tash.plan.route(costs, SIDE, SIDE, at, tile)
        if walked["cost"] < cheapest:
            best, cheapest = tile, walked["cost"]
    if best is None and near:
        return near[0]
    return best


def door(seen, pad, width, height):
    """Answer the tile beside a refinery a harvester unloads from."""
    beside = ring(pad, width, height) - footprint(pad, width, height)
    for tile in sorted(beside, key=lambda at: steps(at, pad)):
        if on_board(tile) and crossable(seen[tile[1]][tile[0]]):
            return tile
    return None


def send(run, seat, units):
    """Order those units onto that tile, one at a time."""
    sent = 0
    for slot in sorted(units):
        standing = mine_of(run, "units", state.ATREIDES)
        if slot not in standing:
            continue
        engine.select(run, standing[slot]["tile"], SIDE)
        engine.order_move(run, seat, SIDE)
        sent += 1
    return sent


def raider(run, yard):
    """The Ordos unit nearest my base, the weakest of them breaking a tie."""
    seen = board(run)
    near = sorted((steps(rec["tile"], yard), rec["health"], rec["tile"])
                  for rec
                  in mine_of(run, "units", state.ORDOS).values())
    for reach, health, at in near:
        if reach <= DEFEND_RING and not (seen[at[1]][at[0]][state.FLAGS]
                                         & state.FLAG_BUILDING):
            return at
    return None


def harvest(run, quota, yard, pad):
    """Keep every delivered harvester on the spice and the raiders off."""
    told = {"quota": None, "orders": 0, "sent": []}
    working, aimed, reaping = set(), {}, set()
    most, stalled = run.watch("credits"), 0
    for _ in range(HARVEST_POLLS):
        engine.close_build_list(run)
        mine = mine_of(run, "units", state.ATREIDES)
        was = len(told["sent"])
        reaping = reapers(mine, reaping)
        idle = [slot for slot in sorted(reaping)
                if slot in mine and slot not in working]
        if idle:
            seen = board(run)
            costs = costs_of(seen)
            fields = spice_of(seen)
            unload = door(seen, pad, PAD_WIDE, PAD_HIGH)
            for slot in idle:
                at = unload or mine[slot]["tile"]
                field = nearest_spice(costs, at, fields)
                if field is None:
                    continue
                told["sent"].append((slot, field))
                send(run, field, [slot])
                working.add(slot)
        near = raider(run, yard)
        guard = [slot for slot in mine
                 if slot not in reaping and aimed.get(slot) != near]
        sent = send(run, near, guard) if near and guard else 0
        aimed.update({slot: near for slot in guard})
        told["orders"] += sent
        if not sent and len(told["sent"]) == was:
            run.step(HARVEST_POLL)
        purse = run.watch("credits")
        if purse >= quota:
            told["quota"] = run.frames()
            return told
        stalled = 0 if purse > most else stalled + 1
        most = max(most, purse)
        if stalled >= STALL_POLLS:
            working, stalled = set(), 0
    return told


def victory(run, name, told):
    run.mark(name)
    won = run.run_until(VICTORY, VICTORY_LIMIT)
    run.expect(f"{name} is up", VICTORY, f"{won} frames on, {told}")
    return run.frames()


def mission_one(run, told):
    """The tape in, the base up on the planner's pad, and the harvest."""
    run.mark("tape")
    played = run.play(HERE / "tapes" / "title-to-mission-1.yaml")
    waited = wait_for_control(run)
    run.expect("the tape reached mission 1", played["segments"] == 3,
               f"{played['segments']} segments, {played['frames']} frames")
    run.expect("the purse opens on 990", "watch credits equal 990",
               f"control came {waited} frames after the tape")
    control = run.frames()
    yard = home(run)
    run.mark("base one", group="build")
    pad, trap, orders = raise_base(run, yard, told["pads"])
    for row in orders:
        run.mark(f"{row['item']} at {row.get('seat')}", group="build")
    run.expect("three buildings stand", "watch buildings equal 3",
               f"yard {yard}, pad {pad}, windtrap {trap}")
    standing = run.frames()
    run.mark("harvest one")
    reaped = harvest(run, QUOTA_ONE, yard, pad)
    run.expect("mission 1's quota is met", "watch credits greater 999",
               f"the quota came at frame {reaped['quota']}, "
               f"{reaped['orders']} defence orders, "
               f"{len(reaped['sent'])} harvester orders")
    return {"control": control, "yard": yard, "pad": pad, "trap": trap,
            "base": standing, "orders": orders, "harvest": reaped,
            "victory": victory(run, "mission 1's plate", reaped["quota"])}


def mission_two(run, told):
    """The briefing tape, the base, two silos, and the defended harvest."""
    run.mark("briefing")
    began = run.frames()
    pressed = press_through(run, lambda: run.watch("mission") == MISSION_TWO
                            and run.watch("credits_shown") == TWO_PURSE)
    waited = wait_for_control(run)
    control = run.frames()
    run.expect("the briefing reached mission 2", pressed is not None,
               f"{pressed} presses over {run.frames() - began} frames")
    run.expect("mission 2 is up", "watch mission equal 2",
               f"control came {waited} frames after the briefing")
    yard = home(run)
    run.mark("base two", group="build")
    pad, trap, orders = raise_base(run, yard, told["pads"])
    silos = [add_silo(run, yard, orders, told["pads"]) for _ in range(SILOS)]
    for row in orders:
        run.mark(f"{row['item']} at {row.get('seat')}", group="build")
    standing = mine_of(run, "buildings", state.ATREIDES)
    run.expect("the base and both silos stand", len(standing) == 5,
               f"yard {yard}, pad {pad}, windtrap {trap}, silos {silos}")
    raised = run.frames()
    run.mark("harvest two")
    reaped = harvest(run, QUOTA_TWO, yard, pad)
    run.expect("mission 2's quota is met", "watch credits greater 2699",
               f"the quota came at frame {reaped['quota']}, "
               f"{reaped['orders']} defence orders, "
               f"{len(reaped['sent'])} harvester orders")
    return {"control": control, "yard": yard, "pad": pad, "trap": trap,
            "base": raised, "silos": silos, "orders": orders,
            "harvest": reaped,
            "victory": victory(run, "mission 2's plate", reaped["quota"])}


def play(run):
    told = {"pads": []}
    told["one"] = mission_one(run, told)
    told["two"] = mission_two(run, told)
    return told


def main():
    line = play(run)
    plate = run.observe()
    print(f"plate exact {plate['exact']} "
          f"perceptual {plate['perceptual']}", flush=True)
    run.judge("both missions are won", True,
              f"mission 1 at {line['one']['victory']}, "
              f"mission 2 at {line['two']['victory']}")
    for row in line["one"]["orders"] + line["two"]["orders"]:
        print(row, flush=True)
    for row in line["pads"]:
        print(row, flush=True)
    print(line, flush=True)
    run.report()


if __name__ == "__main__":
    main()
