"""Mission 9's base for any house: raised on the map's own rock, every
seat judged in the emulator, and held against the Sardaukar."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import door
import engine
import enemy
import state

SIDE = state.MAP_FULL
TILE_COST = 1
SAND_FIRST = 127
SAND_LAST = 142

SLAB = "concrete"
SILO = "silo"
SLAB_SIDE = 2
SLAB_CORNERS = ((0, 0), (-1, 0), (0, -1), (-1, -1))
SLAB_TARGETS = 3

# The pad is what the slabs cover and the footprint what the ghost sets
# down: a 3 by 2 refinery wants 4 by 2 of concrete, because a slab is two
# tiles square and may not overlap concrete already down.
PADS = {"windtrap": (2, 2), "refinery": (4, 2), "silo": (2, 2),
        "outpost": (2, 2), "turret": (2, 2), "rocket turret": (2, 2)}
FOOTPRINTS = {"windtrap": (2, 2), "refinery": (3, 2), "silo": (2, 2),
              "outpost": (2, 2), "turret": (1, 1), "rocket turret": (1, 1)}

PAD_REACH = 8
PAD_JUDGED = 3
PAD_ROUNDS = 3
PAD_TRIES = 3
TURRET_REACH = 6
OPENING = ("windtrap", "refinery", SILO, "outpost")

SETTLE_FRAMES = 240
PURSE_POLL = 120
PURSE_POLLS = 200
QUIET_POLL = 60
QUIET_POLLS = 20
LAY_POLLS = 24
AWAY_TILES = 5
DELIVERY_POLL = 300
DELIVERY_RISE = 40
TRIAL_CAP = 6000
REFINERY_CAP = 14000
HOLD_POLL = 600
# The game's clock stops the moment the mission ends, so a loop that waits
# on it has to stop with it or it never ends.
STALL_POLLS = 2
HOLD_FRAMES = 120000
TARGET_PURSE = 5000
CLOCK = 0xc590
# The view's own pixel origin: it crosses a tile every 32 pixels, so the
# tile in its top left corner is the word over 32.
VIEW_X = 0xe3be
VIEW_Y = 0xe3c0
VIEW_TILE = 32
CAPACITY = 4
SILO_TRIES = 4
STOOD_WORTH = 1000000
TURRETS = ("rocket turret", "rocket turret", "rocket turret",
           "turret", "turret", "turret")
REPAIR_SHARE = 0.9
DEFEND_RING = 12

# A policy is how many silos the ceiling word is raised with, how many
# turrets stand on the approach, and the purse the hold will not repair
# below: step 3 found those three are one decision, so they are searched
# as one candidate.
POLICIES = ((0, 2, 300), (1, 3, 300), (2, 4, 300), (2, 4, 600),
            (3, 5, 900), (4, 6, 300), (4, 6, 600), (4, 6, 1200))
# The trial has to reach past the wave that takes a base down: Ordos lost
# four buildings to one blast 17,000 frames after its turrets stood.
TRIAL_HOLD = 60000
SCOUT_FRAMES = 14000


def is_sand(terrain):
    return SAND_FIRST <= terrain <= SAND_LAST


def is_spice(terrain):
    return state.SPICE_FIRST <= terrain <= state.SPICE_LAST


def crossable(cell):
    if cell[state.FLAGS] & state.FLAG_BUILDING:
        return False
    return is_sand(cell[state.TERRAIN]) or is_spice(cell[state.TERRAIN])


def is_rock(cell):
    """Rock a slab may cover: a unit on it only keeps the order waiting."""
    return (cell[state.TERRAIN] == state.ROCK
            and not cell[state.FLAGS] & state.FLAG_BUILDING)


def steps(here, there):
    return abs(here[0] - there[0]) + abs(here[1] - there[1])


def footprint(seat, width, height):
    return {(seat[0] + dx, seat[1] + dy)
            for dx in range(width) for dy in range(height)}


def ring(seat, width, height):
    return {(seat[0] + dx, seat[1] + dy)
            for dx in range(-1, width + 1) for dy in range(-1, height + 1)}


def on_board(cell):
    return 0 <= cell[0] < SIDE and 0 <= cell[1] < SIDE


def costs_of(seen):
    return bytes(TILE_COST if crossable(seen[y][x]) else tash.plan.IMPASSABLE
                 for y in range(SIDE) for x in range(SIDE))


def spice_of(seen):
    return [(x, y) for y in range(SIDE) for x in range(SIDE)
            if is_spice(seen[y][x][state.TERRAIN])]


def paved(seen):
    return {(x, y) for y in range(SIDE) for x in range(SIDE)
            if state.is_concrete(seen[y][x])}


def toward(here, there, reach):
    """Answer the tile that many steps from here along the way there."""
    gap = max(abs(there[0] - here[0]), abs(there[1] - here[1])) or 1
    return (here[0] + round((there[0] - here[0]) * reach / gap),
            here[1] + round((there[1] - here[1]) * reach / gap))


def centre_of(records):
    tiles = [rec["tile"] for rec in records]
    return (sum(tile[0] for tile in tiles) // len(tiles),
            sum(tile[1] for tile in tiles) // len(tiles))


def clock(run):
    """The game's own frame long, which a restore rewinds with the state."""
    return run.long("system", CLOCK)


def open_of(run, house):
    """Restore that house's opening, or drive the door and take one."""
    name = f"{house}-9-open"
    try:
        run.restore(name)
    except RuntimeError:
        door.enter_password(run, house)
        run.step(door.SCREEN_FRAMES)
        run.checkpoint(name)
    return name


class Base:
    """One house's mission 9 base: the opening, the turrets and the hold."""

    def __init__(self, run):
        self.run = run
        shot = self.snapshot()
        self.house = shot.house
        self.name = state.HOUSE_NAMES[shot.house]
        self.yard = self.home(shot)
        self.orders = []
        self.searches = []
        self.losses = []
        self.seats = {}
        self.standing = 0
        self.full = {}
        self.repairs = []
        self.aimed = {}
        self.guards = 0
        self.scored = 0
        self.trials = []
        self.army = enemy.Army(self.yard)
        self.reserve = POLICIES[0][2]
        self.judged = True

    def tick(self, frames=HOLD_POLL):
        """Step and answer whether the mission's own clock moved with it."""
        was = clock(self.run)
        self.run.step(frames)
        return clock(self.run) > was

    def snapshot(self):
        return state.Snapshot.of(self.run, SIDE)

    def home(self, shot):
        mine = shot.mine(shot.buildings, shot.house)
        return mine[min(mine)]["tile"] if mine else None

    def mine(self, table):
        shot = self.snapshot()
        return shot.mine(getattr(shot, table), shot.house)

    def purse(self):
        return self.snapshot().purses[self.house]

    def room(self):
        """The ceiling word: a refinery and every silo raise it by 1,000."""
        return engine.word(self.run, state.PURSE_BASE + CAPACITY
                           + self.house * state.PURSE_STRIDE)

    def board(self):
        return self.snapshot().seen

    def enemy(self, table):
        shot = self.snapshot()
        return {slot: rec for slot, rec in getattr(shot, table).items()
                if rec["owner"] == state.SARDAUKAR}

    def pads(self, seen, width, height):
        return [seat for seat in state.seats(seen, width, height, is_rock)
                if steps(seat, self.yard) <= PAD_REACH]

    def ranked(self, width, height, toward_spice):
        """Rank the rock my base touches: at the spice, or at the yard."""
        seen = self.board()
        seats = self.pads(seen, width, height)
        if not toward_spice:
            return sorted(seats, key=lambda seat: steps(seat, self.yard))
        spread = tash.plan.distances(costs_of(seen), SIDE, SIDE,
                                     spice_of(seen))
        reached = [(index % SIDE, index // SIDE, reach)
                   for index, reach in enumerate(spread)
                   if reach != tash.plan.UNREACHED]

        def cost(seat):
            middle = (seat[0] + width // 2, seat[1] + height // 2)
            return min((reach + abs(middle[0] - x) + abs(middle[1] - y)
                        for x, y, reach in reached),
                       default=tash.plan.UNREACHED)
        return sorted(seats, key=cost)

    def forget_list(self):
        """A restore puts a different list behind the yard's seats."""
        self.seats = {}
        self.standing = -1

    def survey(self):
        """The list as the yard draws it, re-read whenever it has grown."""
        standing = len(self.mine("buildings"))
        if standing != self.standing or not self.seats:
            told = engine.survey(self.run)
            self.seats = told["seats"]
            self.standing = standing
        return self.seats

    def afford(self, item):
        return self.afford_price(engine.BADGES[item][0])

    def afford_price(self, price):
        """Step until the purse holds that many credits, or give up."""
        for _ in range(PURSE_POLLS):
            if self.purse() >= price:
                return True
            self.run.step(PURSE_POLL)
        return False

    def quiet(self, seat, width, height):
        """Step until no unit of anyone's stands on the pad or beside it."""
        beside = ring(seat, width, height)
        for _ in range(QUIET_POLLS):
            seen = self.board()
            if not any(seen[y][x][state.FLAGS] & state.FLAG_UNIT
                       for x, y in beside if on_board((x, y))):
                return True
            self.run.step(QUIET_POLL)
        return False

    def clear(self, seat, width, height):
        """Walk my own units off a pad, to the yard's far side from it."""
        away = engine.reachable(
            toward(self.yard, (2 * self.yard[0] - seat[0],
                               2 * self.yard[1] - seat[1]), AWAY_TILES), SIDE)
        beside = ring(seat, width, height)
        for rec in self.mine("units").values():
            if rec["tile"] in beside:
                engine.select(self.run, rec["tile"], SIDE)
                engine.order_move(self.run, away, SIDE)

    def order(self, item, targets, seat=None):
        """Buy one item off the list and set it on the first seat allowed."""
        opened = engine.open_build_list(self.run, SIDE)
        if not opened.get("open"):
            return dict(opened, item=item, placed=False, why="no list")
        seat = seat or engine.seat_for(self.survey(), item)
        if seat is None:
            engine.close_build_list(self.run)
            return {"item": item, "placed": False, "why": "not on the list"}
        bought = engine.pick(self.run, seat[0], seat[1])
        if not bought.get("ordered"):
            return dict(bought, item=item, placed=False, why="not ordered")
        laid = {"placed": False, "why": "refused"}
        for target in targets:
            laid = engine.place(self.run, item, target, side=SIDE)
            if laid.get("placed"):
                break
        if not laid.get("placed"):
            engine.let_go(self.run)
        return dict(laid, item=item, seat=seat, price=bought.get("price"))

    def build(self, item, targets, seat=None):
        """One order, with the frames and the credits it really cost."""
        began, purse = clock(self.run), self.purse()
        row = self.order(item, targets, seat)
        self.run.step(SETTLE_FRAMES)
        row["paid"] = purse - self.purse()
        row["frames"] = clock(self.run) - began
        self.orders.append(row)
        return row

    def slabs(self, seat, width, height):
        """The slab seats that pave a pad, the widest cover first."""
        seen = self.board()
        laid = paved(seen)
        pad = footprint(seat, width, height)
        seats = {}
        for tile in pad - laid:
            for corner in SLAB_CORNERS:
                at = (tile[0] + corner[0], tile[1] + corner[1])
                cover = footprint(at, SLAB_SIDE, SLAB_SIDE)
                if cover & laid or not all(on_board(one) for one in cover):
                    continue
                if not all(is_rock(seen[one[1]][one[0]]) for one in cover):
                    continue
                if not state.touches(seen, at, SLAB_SIDE, SLAB_SIDE):
                    continue
                if any(seen[one[1]][one[0]][state.FLAGS] & state.FLAG_UNIT
                       for one in cover):
                    continue
                seats[at] = len(cover & pad)
        return sorted(seats, key=lambda at: (-seats[at], at))

    def bare(self, seat, width, height):
        return footprint(seat, width, height) - paved(self.board())

    def lay(self, seat, width, height):
        """Slab that pad one square at a time until no tile is bare."""
        for _ in range(LAY_POLLS):
            if not self.bare(seat, width, height):
                return True
            seats = self.slabs(seat, width, height)
            if not seats:
                self.run.step(QUIET_POLL)
                continue
            if not self.afford(SLAB):
                return False
            if not self.build(SLAB, seats[:SLAB_TARGETS]).get("placed"):
                return False
        return not self.bare(seat, width, height)

    def raise_on(self, item, seat):
        """Pave that pad, set the building on it, say whether it stands."""
        width, height = PADS[item]
        self.clear(seat, width, height)
        if not self.lay(seat, width, height):
            return False
        self.quiet(seat, *FOOTPRINTS[item])
        if not self.afford(item):
            return False
        wide = FOOTPRINTS[item][0]
        heads = [(seat[0] + dx, seat[1]) for dx in range(width - wide + 1)]
        return bool(self.build(item, heads).get("placed"))

    def first_load(self, limit):
        """Step until the purse climbs: a harvester has delivered."""
        least = self.purse()
        for _ in range(max(limit // DELIVERY_POLL, 1)):
            self.run.step(DELIVERY_POLL)
            purse = self.purse()
            if purse > least + DELIVERY_RISE:
                return clock(self.run)
            least = min(least, purse)
        return None

    def trial(self, item, seat, cap):
        """One candidate seat, played out in the emulator and timed."""
        began, stood = clock(self.run), False
        try:
            stood = self.raise_on(item, seat)
            if stood and item == "refinery":
                stood = self.first_load(cap) is not None
        except RuntimeError:
            pass
        self.scored = ((STOOD_WORTH if stood else 0)
                       - (clock(self.run) - began))

    def choose(self, item, batch, cap):
        """Let the emulator judge one batch of pads, and say if any stood."""
        if len(batch) < 2 or not self.judged:
            best, found = (batch[0] if batch else None), None
        else:
            found = tash.search(batch,
                                lambda run, seat: self.trial(item, seat, cap),
                                lambda run: self.scored)
            best = found.best
        self.searches.append({"item": item, "judged": batch, "chose": best,
                              "score": found.score if found else None,
                              "trials": found.trials if found else 0,
                              "frames": found.frames if found else 0})
        stood = bool(found and found.score >= STOOD_WORTH)
        return (best if best is not None else
                (batch[0] if batch else None)), stood

    def raise_item(self, item):
        """Judge the ranked pads a batch at a time until one of them stands."""
        seats = self.ranked(*PADS[item], toward_spice=item == "refinery")
        cap = REFINERY_CAP if item == "refinery" else TRIAL_CAP
        best, stood, judged = None, False, 0
        while judged < PAD_ROUNDS * PAD_JUDGED and not stood:
            batch = seats[judged:judged + PAD_JUDGED]
            if not batch:
                break
            best, stood = self.choose(item, batch, cap)
            judged += len(batch)
        fall = ([] if best is None else [best])
        fall += [seat for seat in seats[judged:] if seat != best][:PAD_TRIES]
        for seat in fall:
            if self.raise_on(item, seat):
                return True
        return False

    def stock(self, how_many):
        """Silos raise the purse's ceiling word; how many is searched."""
        raised = 0
        for _ in range(min(how_many, SILO_TRIES)):
            if not self.raise_item(SILO):
                break
            raised += 1
        return {"silos": raised, "room": self.room(),
                "stocked": clock(self.run)}

    def raise_opening(self, told):
        """The windtrap, the refinery, a silo and the outpost, in order."""
        for item in OPENING:
            told[item] = {"standing": self.raise_item(item),
                          "frame": clock(self.run)}
            if item == "refinery":
                told["first delivery"] = self.first_load(REFINERY_CAP)
        return told

    def worst(self, shot):
        """The yard while it is hurt, else my most hurt building."""
        mine = shot.mine(shot.buildings, self.house)
        for rec in mine.values():
            self.full[rec["kind"]] = max(self.full.get(rec["kind"], 0),
                                         rec["health"])
        hurt = sorted((rec["tile"] != self.yard,
                       rec["health"] / self.full[rec["kind"]], rec["slot"])
                      for rec in mine.values()
                      if rec["health"]
                      < self.full[rec["kind"]] * REPAIR_SHARE)
        return mine[hurt[0][2]] if hurt else None

    def repair(self, shot):
        """Select the worst hurt building and press its own FIX tab."""
        rec = self.worst(shot)
        purse = shot.purses[self.house]
        if rec is None or purse < self.reserve:
            return None
        began, was = clock(self.run), rec["health"]
        opened = engine.open_panel(self.run, rec["tile"], SIDE)
        if not opened.get("open"):
            engine.let_go(self.run)
            return None
        engine.fix(self.run)
        self.run.step(HOLD_POLL)
        now = self.mine("buildings").get(rec["slot"])
        told = {"frame": began, "what": rec["name"], "was": was,
                "full": self.full[rec["kind"]],
                "healed": (now["health"] - was) if now else None,
                "paid": purse - self.purse(),
                "frames": clock(self.run) - began}
        self.repairs.append(told)
        return told

    def approach(self, came=None):
        """The tile the first wave came over, or the enemy's own centre."""
        if came:
            middle = (sum(tile[0] for tile in came) // len(came),
                      sum(tile[1] for tile in came) // len(came))
        else:
            theirs = self.enemy("units") or self.enemy("buildings")
            if not theirs:
                return None
            middle = centre_of(list(theirs.values()))
        return toward(self.yard, middle, TURRET_REACH)

    def scout(self, frames):
        """Hold with no hand at all until a wave moves, then rewind to here."""
        name = f"{self.name}-9-scout"
        self.run.checkpoint(name)
        watch = enemy.Army(self.yard)
        began, stalls = clock(self.run), 0
        while clock(self.run) < began + frames and stalls < STALL_POLLS:
            watch.poll(self.snapshot(), clock(self.run), self.house)
            if watch.approach():
                break
            stalls = 0 if self.tick() else stalls + 1
        told = {"frames": clock(self.run) - began, "came": watch.approach(),
                "waves": watch.waves(), "ended": stalls >= STALL_POLLS}
        self.run.restore(name)
        self.run.forget(name)
        return told

    def turrets(self, item, how_many, came=None):
        """Put turrets between the yard and where the waves come over."""
        came = self.approach(came)
        placed = []
        for _ in range(how_many):
            seen = self.board()
            seats = sorted(self.pads(seen, *PADS[item]),
                           key=lambda seat: steps(seat, came)
                           + steps(seat, self.yard))
            if not seats:
                break
            seat, _ = self.choose(item, seats[:PAD_JUDGED], TRIAL_CAP)
            if seat is None or not self.raise_on(item, seat):
                break
            placed.append(seat)
        return placed

    def watch_losses(self, shot, was):
        """Record what fell since the last poll, and the frame it fell."""
        now = {slot: rec["name"]
               for slot, rec in shot.mine(shot.buildings, self.house).items()}
        units = {slot: rec["name"]
                 for slot, rec in shot.mine(shot.units, self.house).items()}
        for slot, name in was[0].items():
            if slot not in now:
                self.losses.append({"frame": clock(self.run),
                                    "what": name, "kind": "building"})
        for slot, name in was[1].items():
            if slot not in units:
                self.losses.append({"frame": clock(self.run),
                                    "what": name, "kind": "unit"})
        return now, units

    def raider(self, shot):
        """The Sardaukar unit nearest my yard, inside the ring I answer."""
        theirs = [rec for rec in shot.units.values()
                  if rec["owner"] == state.SARDAUKAR]
        near = sorted((steps(rec["tile"], self.yard), rec["health"],
                       rec["tile"]) for rec in theirs)
        for reach, health, at in near:
            cell = shot.seen[at[1]][at[0]]
            if reach <= DEFEND_RING and not cell[state.FLAGS] \
                    & state.FLAG_BUILDING:
                return at
        return None

    def guard(self, shot):
        """Send every unit of mine that is not aimed there already."""
        near = self.raider(shot)
        if near is None:
            return 0
        sent = 0
        for slot, rec in shot.mine(shot.units, self.house).items():
            if rec["kind"] == state.HARVESTER or self.aimed.get(slot) == near:
                continue
            engine.select(self.run, rec["tile"], SIDE)
            engine.order_move(self.run, near, SIDE)
            self.aimed[slot] = near
            sent += 1
        self.guards += sent
        return sent

    def kept(self):
        """Every record's length, so a search's trials can be cut off it."""
        return {name: len(getattr(self, name))
                for name in ("orders", "searches", "losses", "repairs")}

    def fresh(self, kept):
        """Cut the trials off the records; the run is back where it began."""
        for name, size in kept.items():
            del getattr(self, name)[size:]
        self.aimed.clear()
        self.guards = 0
        self.forget_list()
        self.army = enemy.Army(self.yard)

    def defend(self, how_many, came=None):
        """Turrets between the yard and where the first wave came over."""
        placed = []
        for item in TURRETS[:how_many]:
            placed += self.turrets(item, 1, came)
        return placed

    def policy(self, candidate, came, until):
        """Play one whole candidate out: the silos, the turrets, the hold."""
        silos, turrets, reserve = candidate
        self.reserve = reserve
        self.forget_list()
        began = clock(self.run)
        told = {"candidate": candidate}
        try:
            told.update(self.stock(silos))
            told["turrets"] = self.defend(turrets, came)
            self.hold(began + until, told)
        except RuntimeError:
            pass
        self.scored = (len(self.mine("buildings")) * STOOD_WORTH
                       + clock(self.run) - began)
        self.trials.append({"candidate": candidate, "score": self.scored,
                            "stood": told.get("stood"),
                            "fell": told.get("fell"),
                            "ran": clock(self.run) - began,
                            "turrets": told.get("turrets")})
        return told

    def spend(self, came, until=TRIAL_HOLD):
        """Search the three spending choices as one candidate, in the game."""
        self.judged = False
        found = tash.search(
            list(POLICIES),
            lambda run, one: self.policy(one, came, until),
            lambda run: self.scored)
        self.judged = True
        return found

    def hold(self, until, told, keep=None, hand=True):
        """Poll to that frame: watch the attack, repair, count what falls."""
        was = ({slot: rec["name"] for slot, rec
                in self.mine("buildings").items()},
               {slot: rec["name"] for slot, rec in self.mine("units").items()})
        stalls = 0
        while clock(self.run) < until and stalls < STALL_POLLS:
            if hand:
                engine.close_build_list(self.run)
            shot = self.snapshot()
            self.army.poll(shot, clock(self.run), self.house)
            was = self.watch_losses(shot, was)
            if not was[0]:
                told["fell"] = clock(self.run)
                return told
            if (told.get("purse") is None
                    and shot.purses[self.house] >= TARGET_PURSE):
                told["purse"] = clock(self.run)
            if keep and len(was[0]) > told.get("most", 0):
                told["most"] = len(was[0])
                told["stands"] = clock(self.run)
                self.run.checkpoint(keep)
            was_at = clock(self.run)
            if hand and (self.repair(shot) is not None
                         or self.guard(shot)):
                stalls = 0 if clock(self.run) > was_at else stalls + 1
            else:
                stalls = 0 if self.tick() else stalls + 1
        told["ended"] = stalls >= STALL_POLLS
        told["stood"] = len(was[0])
        told["standing"] = sorted(was[0].values())
        told["guards"] = self.guards
        return told


def play_house(run, house, until=HOLD_FRAMES, told=None):
    """One house's mission 9: the base, the spending searched, the hold."""
    open_of(run, house)
    began = clock(run)
    line = Base(run)
    told = {} if told is None else told
    told.update({"house": house, "yard": line.yard, "opening": began,
                 "line": line})
    run.mark(f"{house} opening", group=house)
    line.raise_opening(told)
    run.mark(f"{house} base", group=house)
    told["base"] = clock(run)
    run.checkpoint(f"{house}-9-base")
    told["scout"] = line.scout(SCOUT_FRAMES)
    kept = line.kept()
    found = line.spend(told["scout"]["came"])
    told["spent"] = {"chose": found.best, "score": found.score,
                     "trials": found.trials, "frames": found.frames,
                     "tried": line.trials}
    line.fresh(kept)
    silos, turrets, reserve = found.best
    line.reserve = reserve
    told.update(line.stock(silos))
    told["turrets"] = line.defend(turrets, told["scout"]["came"])
    told["defended"] = clock(run)
    run.checkpoint(f"{house}-9-defended")
    run.mark(f"{house} turrets", group=house)
    line.hold(began + until, told, keep=f"{house}-9-standing")
    told["end"] = clock(run)
    told.update({"orders": line.orders, "searches": line.searches,
                 "losses": line.losses, "repairs": line.repairs,
                 "waves": line.army.waves(), "period": line.army.period(),
                 "kills": line.army.kills, "worms": line.army.worms,
                 "enemy": line.army.rows})
    run.judge(f"{house}'s base stands at {until} frames",
              told.get("fell") is None,
              f"{told.get('stood')} buildings, {len(told['losses'])} losses")
    return told


def hold_from(run, house, name, frames, hand=True, park=None):
    """Restore a base and hold it; with no hand it is the enemy alone."""
    run.restore(name)
    line = Base(run)
    if park is not None:
        engine.move_to(run, park, steady=True, side=SIDE)
    began = clock(run)
    told = {"house": house, "from": name, "opening": began, "park": park,
            "view": (engine.word(run, VIEW_X) // VIEW_TILE,
                     engine.word(run, VIEW_Y) // VIEW_TILE)}
    line.hold(began + frames, told, hand=hand)
    told.update({"losses": line.losses, "repairs": line.repairs,
                 "waves": line.army.waves(), "period": line.army.period(),
                 "kills": line.army.kills, "end": clock(run),
                 "enemy": line.army.rows, "worms": line.army.worms})
    return told


def main():
    run = tash.run
    told = {}
    for house in door.HOUSES:
        told[house] = play_house(run, house)
        print(told[house], flush=True)
    run.judge("every house's base stands at the hold's end",
              all(row.get("fell") is None for row in told.values()),
              ", ".join(f"{name} {row.get('stood')} buildings"
                        for name, row in told.items()))
    run.report()


if __name__ == "__main__":
    main()
