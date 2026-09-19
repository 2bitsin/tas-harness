"""Mission 9's income: refineries and harvesters bought against the hold's
own spending, each candidate judged by the purse at one game frame."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import base
import state

REFINERY = 12
SILO = 17
FACTORY = 3
HARVESTER = "harvester"
IDLE = "idle"
HARVESTER_BADGE = (300, 150)
FACTORY_KIND = 4
PER = 12000
YARD_WORTH = 1000000
# A credit is worth a hundred game frames: the hold earns 655 of them
# per 12,000, so the term ranks the fast candidate first, never above
# the base that stands.
FRAME_WORTH = 100
SETTLE = 240
RATE_NAME = "income-rate"
RATE_FRAMES = PER
IDLE_FRAMES = 5745

# What each candidate spends, in the order it spends it: the empty one is
# step 4's policy alone, and idle spends a purchase's frames buying
# nothing, so the frames and the credits can be told apart.
CANDIDATES = ((),
              (IDLE,),
              (REFINERY,),
              (REFINERY, REFINERY),
              (FACTORY, HARVESTER),
              (FACTORY, HARVESTER, HARVESTER),
              (REFINERY, FACTORY, HARVESTER),
              (FACTORY, HARVESTER, REFINERY))


def named(item):
    return item if isinstance(item, str) else state.BUILD_NAMES[item]


class Income:
    """One house's income: what is bought, and the purse it leaves behind."""

    def __init__(self, line, climb):
        self.line = line
        self.climb = climb
        self.run = line.run
        self.trials = []
        self.kept = None
        self.scored = 0

    def factories(self):
        return [rec["tile"] for rec in self.line.mine("buildings").values()
                if rec["kind"] == FACTORY_KIND]

    def harvesters(self):
        return sum(1 for rec in self.line.mine("units").values()
                   if rec["kind"] == state.HARVESTER)

    def yard_stands(self):
        return any(rec["tile"] == self.line.yard
                   for rec in self.line.mine("buildings").values())

    def seat_of(self, seats, item):
        for seat in sorted(seats):
            if seats[seat][2] == item:
                return seat, seats[seat]
        return None, None

    def idle(self, until):
        """Spend a purchase's frames in the list, buying nothing at all."""
        began = base.clock(self.run)
        while base.clock(self.run) - began < IDLE_FRAMES:
            if not self.climb.catalog():
                break
            if base.clock(self.run) >= until:
                break
        return {"what": IDLE, "bought": False, "why": "nothing was ordered",
                "frames": base.clock(self.run) - began}

    def hire(self, until):
        """One harvester off a factory of mine, or why there is none."""
        mine = self.factories()
        began, purse = base.clock(self.run), self.line.purse()
        if not mine:
            return {"what": HARVESTER, "bought": False, "why": "no factory",
                    "frames": 0}
        if not self.climb.afford(HARVESTER_BADGE[0], until):
            return {"what": HARVESTER, "bought": False,
                    "why": "the purse never held the price",
                    "frames": base.clock(self.run) - began}
        told = self.climb.hire(mine[0], HARVESTER_BADGE) or {}
        self.run.step(SETTLE)
        return {"what": HARVESTER, "bought": bool(told.get("ordered")),
                "why": told.get("why"), "tile": mine[0],
                "paid": purse - self.line.purse(),
                "harvesters": self.harvesters(),
                "frames": base.clock(self.run) - began}

    def raise_one(self, item, until):
        """Buy one item off the yard's list by its own id, and set it down."""
        if item == IDLE:
            return self.idle(until)
        if item == HARVESTER:
            return self.hire(until)
        seats = self.climb.catalog()
        seat, words = self.seat_of(seats, item)
        if seat is None:
            return {"what": named(item), "bought": False,
                    "why": "not on the list", "frames": 0}
        told = self.climb.buy(seat, words, until)
        return dict(told, what=named(item), bought=bool(told.get("placed")),
                    harvesters=self.harvesters())

    def measured(self):
        """Income alone: the purse over a hands-off window, then rewound."""
        kept = self.line.kept()
        self.run.checkpoint(RATE_NAME, scratch=True)
        began, purse, room = (base.clock(self.run), self.line.purse(),
                              self.line.room())
        harvesters = self.harvesters()
        self.line.hold(began + RATE_FRAMES, {}, hand=False)
        frames = base.clock(self.run) - began
        told = {"rose": self.line.purse() - purse, "frames": frames,
                "was": purse, "room": room, "kept": self.line.room() == room,
                "harvesters": harvesters, "left": self.harvesters(),
                "capped": self.line.purse() >= self.line.room() - 1}
        told["rate"] = round(told["rose"] * PER / frames) if frames else None
        self.run.restore(RATE_NAME)
        self.run.forget(RATE_NAME)
        self.line.fresh(kept)
        return told

    def earn(self, until, told, step=PER):
        """Hold to that frame and answer what the purse did each sample."""
        rows = []
        while base.clock(self.run) < until:
            began, purse = base.clock(self.run), self.line.purse()
            probe = self.measured() if not rows else None
            self.line.hold(min(began + step, until), told)
            frames = base.clock(self.run) - began
            rows.append({"at": base.clock(self.run), "probe": probe,
                         "purse": self.line.purse(), "was": purse,
                         "frames": frames, "room": self.line.room(),
                         "harvesters": self.harvesters(),
                         "standing": len(self.line.mine("buildings"))})
            if told.get("fell") is not None or not frames:
                break
        return rows

    def probe(self, rows):
        """The income the hands-off window read once the buying was done."""
        for row in rows:
            if row["probe"] is not None:
                return row["probe"]
        return None

    def play(self, candidate, until):
        """One candidate: what it buys, then the hold that pays for it."""
        began, purse = base.clock(self.run), self.line.purse()
        told = {"candidate": tuple(named(one) for one in candidate),
                "began": began, "was": self.harvesters(), "bought": []}
        try:
            for item in candidate:
                told["bought"].append(self.raise_one(item, until))
            told["spent"] = purse - self.line.purse()
            told["ready"] = base.clock(self.run)
            told["samples"] = self.earn(until, told)
        except RuntimeError as refusal:
            told["refused"] = str(refusal)
            told["samples"] = told.get("samples", [])
        told.update({"purse": self.line.purse(), "room": self.line.room(),
                     "harvesters": self.harvesters(),
                     "probe": self.probe(told["samples"]),
                     "yard": self.yard_stands(),
                     "standing": len(self.line.mine("buildings")),
                     "lost": [row for row in self.line.losses
                              if row["frame"] >= began],
                     "end": base.clock(self.run)})
        self.scored = (told["purse"] + (YARD_WORTH if told["yard"] else 0)
                       - (told["end"] - began) // FRAME_WORTH)
        told["score"] = self.scored
        self.trials.append(told)
        print(told, flush=True)
        return told

    def choose(self, until, candidates=CANDIDATES):
        """Let the emulator judge each spending, and answer the best."""
        self.kept = self.line.kept()
        found = tash.search(
            list(candidates),
            lambda run, one: self.play(one, until),
            lambda run: self.scored,
            restored=lambda run: self.line.fresh(self.kept))
        self.line.fresh(self.kept)
        return found
