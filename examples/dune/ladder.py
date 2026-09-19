"""The yard's list read as words: the item id under the cursor names every
seat, and the rungs are raised in the tech tree's own order."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import base
import engine
import state

PAD_SIDE = 4
PAD_REACH = 12
ANCHORS_TRIED = 10
WAIT_POLL = 600
WAIT_POLLS = 200
SETTLE = 240
STOOD = 1000000
FRAME_WORTH = 100
SEAT_SETTLE = 12
OPEN_TRIES = 8
YARD_SIDE = 2
LEAST_SEATS = 4
YARD_SEATS = 12
LIST_MISSES = 4
# The tech tree this step walks, in the order it walks it.
WANTED = (3, 4, 5, 6, 7, 11, 2)
HARVESTER_BADGE = (300, 150)
REFINERY = 12
# The film's line replays step 4's chosen spending rather than searching
# it again: one silo, three turrets, and no repair under 300 credits.
FILM_HOUSE = "harkonnen"
FILM_POLICY = (1, 3, 300)
FILM_BASE = 26000
FILM_UNTIL = 50000
FILM_ORDER = (11, 2)
FILM_KEEP = "harkonnen-9-step6"
# What each rung covers, measured off the records it leaves: the pad is
# what the slabs pave and it is never smaller than the footprint.
PADS = {1: (2, 2), 3: (4, 2), 5: (2, 2), 7: (2, 2), 9: (2, 2), 10: (2, 2),
        12: (4, 2), 15: (2, 2), 16: (2, 2), 17: (2, 2), 18: (2, 2)}
PAD_WIDE = (4, 4)
PADS_TRIED = 3
HEADS_TRIED = 4


class Ladder:
    """One house's climb: every seat the list draws, and what each makes."""

    def __init__(self, line, wants=WANTED):
        self.line = line
        self.run = line.run
        self.wants = wants
        self.rungs = []
        self.raised = set()
        self.named = {}
        self.trials = []
        self.lists = []
        self.opens = []
        self.scored = 0

    def read_list(self):
        """Walk every seat of the open list, reading its three words."""
        engine.walk_to(self.run, engine.PANEL_ROW, engine.EXIT_ROW,
                       ("up", "down"))
        engine.walk_to(self.run, engine.PANEL_COLUMN, engine.EXIT_COLUMN,
                       ("left", "right"))
        seats = {}
        while True:
            while True:
                self.run.step(SEAT_SETTLE)
                seat = (engine.word(self.run, engine.PANEL_ROW),
                        engine.word(self.run, engine.PANEL_COLUMN))
                price, gear = engine.badge(self.run)
                seats[seat] = (price, gear, engine.item(self.run))
                if not engine.step_to(self.run, engine.PANEL_COLUMN, "right"):
                    break
            engine.walk_to(self.run, engine.PANEL_COLUMN, engine.EXIT_COLUMN,
                           ("left", "right"))
            if not engine.step_to(self.run, engine.PANEL_ROW, "down"):
                break
        return seats

    def open_list(self, tile=None):
        """Open a list and prove it drew its rows; it is left open."""
        for _ in range(OPEN_TRIES):
            self.run.release(1)
            engine.let_go(self.run)
            self.line.quiet(self.line.yard if tile is None else tile,
                            YARD_SIDE, YARD_SIDE)
            if tile is None:
                opened = engine.open_build_list(self.run, base.SIDE)
            else:
                opened = engine.open_panel(self.run, tile, base.SIDE)
            seats = self.read_list() if opened.get("open") else {}
            self.opens.append({"frame": base.clock(self.run), "tile": tile,
                               "open": opened.get("open"),
                               "why": opened.get("why"), "drew": len(seats)})
            if len(seats) >= (LEAST_SEATS if tile else YARD_SEATS):
                return seats
            engine.close_build_list(self.run)
        return {}

    def catalog(self, tile=None):
        """The list a building of mine draws, seat by seat, then closed."""
        seats = self.open_list(tile)
        if not seats:
            return {}
        engine.close_build_list(self.run)
        self.lists.append({"frame": base.clock(self.run), "tile": tile,
                           "seats": seats})
        return seats

    def order(self, seat, targets, item=None):
        """Pick that seat off a list that drew, then let the ghost judge."""
        seats = self.open_list()
        if not seats:
            return {"placed": False, "why": "the list never drew"}
        if item is not None and seats.get(seat, (0, 0, None))[2] != item:
            seat = next((one for one in sorted(seats)
                         if seats[one][2] == item), None)
            if seat is None:
                engine.close_build_list(self.run)
                return {"placed": False, "why": "the seat lost its item"}
        bought = engine.pick(self.run, seat[0], seat[1])
        if not bought.get("ordered"):
            return dict(bought, placed=False, why="not ordered")
        laid = {"placed": False, "why": "refused"}
        for target in targets:
            laid = engine.place(self.run, "rung", target, side=base.SIDE)
            if laid.get("placed"):
                break
        if not laid.get("placed"):
            engine.let_go(self.run)
        return dict(laid, seat=seat, price=bought.get("price"),
                    item=bought.get("item"), seated=bought.get("seated"))

    def room_at(self, seen, tile):
        """How much of the widest footprint a tile could carry is free."""
        free = 0
        for dy in range(PAD_SIDE - 1):
            for dx in range(PAD_SIDE - 1):
                at = (tile[0] + dx, tile[1] + dy)
                if not base.on_board(at):
                    continue
                cell = seen[at[1]][at[0]]
                if base.is_rock(cell) or state.is_free_concrete(cell):
                    free += 1
        return free

    def anchors(self):
        """My own concrete first, then rock my base touches, widest first."""
        seen = self.line.board()
        out = []
        for y in range(base.SIDE):
            for x in range(base.SIDE):
                cell = seen[y][x]
                paved = state.is_free_concrete(cell)
                if not paved and not base.is_rock(cell):
                    continue
                if base.steps((x, y), self.line.yard) > PAD_REACH:
                    continue
                if not state.touches(seen, (x, y), 1, 1):
                    continue
                out.append((not paved, -self.room_at(seen, (x, y)),
                            base.steps((x, y), self.line.yard), (x, y)))
        return [row[3] for row in sorted(out)]

    def afford(self, price, until=None):
        """Hold the base, repairing and guarding, until the purse holds that."""
        told = {}
        was = self.line.reserve
        # A rung's price is the purse's floor while saving for it: the hold
        # repairs down to the reserve, so a lower one never saves up.
        self.line.reserve = max(was, price)
        try:
            for _ in range(WAIT_POLLS):
                if self.line.purse() >= price:
                    return True
                if until is not None and base.clock(self.run) >= until:
                    return False
                self.line.hold(base.clock(self.run) + WAIT_POLL, told)
                if told.get("fell") is not None or told.get("ended"):
                    return False
            return self.line.purse() >= price
        finally:
            self.line.reserve = was

    def standing(self):
        return {slot: rec["kind"]
                for slot, rec in self.line.mine("buildings").items()}

    def pave(self, item):
        """Lay concrete on the best pad my base owns, so a rung stands whole."""
        width, height = PADS.get(item, PAD_WIDE)
        for seat in self.line.ranked(width, height,
                                     item == REFINERY)[:PADS_TRIED]:
            self.line.clear(seat, width, height)
            if self.line.lay(seat, width, height):
                return seat
        return None

    def heads(self, item):
        """Pave a pad and answer the tiles the ghost may take inside it."""
        pad = self.pave(item)
        if pad is None:
            return self.anchors()[:ANCHORS_TRIED]
        width, height = PADS.get(item, PAD_WIDE)
        return [(pad[0] + dx, pad[1] + dy)
                for dy in range(max(height - 1, 1))
                for dx in range(max(width - 1, 1))][:HEADS_TRIED]

    def buy(self, seat, words, until=None, anchors=None):
        """Pave a pad, buy that seat and set the rung down on the concrete."""
        began = base.clock(self.run)
        if anchors is None:
            anchors = self.heads(words[2])
        if not self.afford(words[0], until):
            return {"seat": seat, "words": words, "placed": False,
                    "why": "the purse never held the price",
                    "frames": base.clock(self.run) - began}
        was, purse = self.standing(), self.line.purse()
        self.raised.add(words[2])
        told = self.order(seat, anchors, words[2])
        self.run.step(SETTLE)
        told["paid"] = purse - self.line.purse()
        made = [slot for slot in self.standing() if slot not in was]
        record = self.line.mine("buildings").get(made[0]) if made else None
        return {"seat": seat, "words": words, "placed": told.get("placed"),
                "why": told.get("why"), "tile": told.get("target"),
                "kind": record["kind"] if record else None,
                "name": record["name"] if record else None,
                "health": record["health"] if record else None,
                "paid": told.get("paid"), "seated": told.get("seated"),
                "tries": told.get("tries"), "item": told.get("item"),
                "ghost": told.get("ghost"),
                "frames": base.clock(self.run) - began}

    def trial(self, seat, words):
        """One seat bought in the emulator, to be thrown away by the restore."""
        told = self.buy(seat, words)
        if told.get("placed"):
            told["offers"] = self.catalog(told["tile"])
        self.trials.append(told)
        self.named[(seat, words)] = told
        self.scored = (STOOD if told.get("placed") else 0) - told["frames"]

    def name_seats(self, seats):
        """Prove the id of every seat by buying it in its own restore."""
        unknown = [seat for seat in sorted(seats)
                   if seats[seat][0] not in (0, engine.PANEL_NONE)
                   and (seat, seats[seat]) not in self.named]
        if unknown:
            tash.search(unknown,
                        lambda run, seat: self.trial(seat, seats[seat]),
                        lambda run: self.scored)
            self.line.forget_list()
        return {seat: self.named.get((seat, seats[seat])) for seat in seats}

    def wanted(self, seats):
        """The first rung of the tree this climb has not tried to raise."""
        for kind in self.wants:
            if kind in self.raised:
                continue
            for seat in sorted(seats):
                if seats[seat][2] == kind:
                    return seat, seats[seat], kind
        return None, None, None

    def climb(self, until, keep=None, hires=True):
        """Read the list and raise the next rung, over and over."""
        missed = 0
        while base.clock(self.run) < until:
            seats = self.catalog()
            if not seats:
                missed += 1
                if missed > LIST_MISSES:
                    break
                continue
            seat, words, kind = self.wanted(seats)
            if seat is None:
                break
            told = self.buy(seat, words, until)
            told["wanted"] = kind
            told["standing"] = len(self.line.mine("buildings"))
            told["purse"] = self.line.purse()
            self.rungs.append(told)
            print(told, flush=True)
            if not told.get("placed"):
                break
            told["offers"] = self.catalog(told["tile"])
            if hires and HARVESTER_BADGE in [words[:2] for words
                                             in told["offers"].values()]:
                told["hired"] = self.hire(told["tile"])
            if keep:
                self.run.checkpoint(keep)
        return self.rungs

    def tree(self, orders, until, kept, hires=False):
        """Search the orders a climb could take; the emulator judges each."""
        def once(run, order):
            self.wants = order
            self.raised.clear()
            self.rungs = []
            began = base.clock(self.run)
            rungs = self.climb(until, hires=hires)
            ran = base.clock(self.run) - began
            self.scored = (sum(1 for row in rungs if row.get("placed"))
                           * STOOD + self.line.purse() - ran // FRAME_WORTH)
            self.trials.append({"order": order, "score": self.scored,
                                "rungs": rungs, "purse": self.line.purse(),
                                "stood": len(self.line.mine("buildings")),
                                "ran": ran,
                                "end": base.clock(self.run)})
            print(self.trials[-1], flush=True)

        return tash.search(list(orders), once, lambda run: self.scored,
                           restored=lambda run: self.line.fresh(kept))

    def hire(self, tile, mark=HARVESTER_BADGE):
        """Buy one unit off a factory of mine: its panel, its seat, no ghost."""
        seats = self.open_list(tile)
        if not seats:
            return None
        for seat in sorted(seats):
            if seats[seat][:2] == mark:
                return engine.pick(self.run, seat[0], seat[1])
        engine.close_build_list(self.run)
        return None


def play_ladder(run, house, policy, base_until, until, wants=WANTED):
    """One house's mission 9 from the door: the base held, then the ladder."""
    base.open_of(run, house)
    began = base.clock(run)
    line = base.Base(run)
    told = {"house": house, "opening": began, "policy": policy}
    run.mark(f"{house} opening", group=house)
    line.raise_opening(told)
    told["scout"] = line.scout(base.SCOUT_FRAMES)
    silos, turrets, reserve = policy
    line.reserve = reserve
    told.update(line.stock(silos))
    told["turrets"] = line.defend(turrets, told["scout"]["came"])
    run.mark(f"{house} turrets", group=house)
    line.hold(began + base_until, told, keep=f"{house}-9-film")
    run.mark(f"{house} ladder", group=house)
    climb = Ladder(line, wants)
    told["rungs"] = climb.climb(until, hires=False)
    told["lists"] = climb.lists
    told["opens"] = climb.opens
    told["end"] = base.clock(run)
    return told


def main():
    run = tash.run
    told = play_ladder(run, FILM_HOUSE, FILM_POLICY, FILM_BASE, FILM_UNTIL,
                       FILM_ORDER)
    run.checkpoint(FILM_KEEP)
    raised = sum(1 for row in told["rungs"] if row.get("placed"))
    print({key: told[key] for key in told if key != "line"}, flush=True)
    run.judge(f"the {FILM_HOUSE} ladder raised a rung above the base",
              bool(raised), f"{raised} rungs, ending at {told['end']}")
    print("tape", run.tape(), "line", run.line(), flush=True)
    run.report()


if __name__ == "__main__":
    main()
