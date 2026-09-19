"""Step 4b: the first year run as a program, one decision a frame, with
the December orders searched from a checkpoint."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import tash
import charts
import engine
import takings
import visitor

run = tash.run

JANUARY = 0
DECEMBER = 11
GATE_OPEN = 1
ORDER_DAY = 3

# A day is about forty frames, so a day watch that has not moved in this
# many frames is the goods modal holding the clock at ROM $10f2a.
MODAL_STILL = 240

LIVELY_KIND = 1
PLEASING_KIND = 2
LIVELY_TILE = (23, 39)
PLEASING_TILE = (23, 38)
HIRE_AFTER = 20

# Measured over the whole first year: 104/100 of what a visit is worth is
# the fare with the best return that still fills the bus.
FARE_OVER_WORTH = 104
WORTH_UNIT = 100
FARE_STEP = 10
GATE_TRIES = 4

ORDERS = ((0, 0), (1, 6), (1, 7), (2, 5), (2, 6), (2, 7),
          (2, 8), (3, 6), (3, 7), (4, 6))
DECEMBER_STATE = "december"


def read_the_charts():
    """The charts, the table behind them, then the year end's numbers."""
    run.step(charts.CHARTS_SETTLE)
    engine.tap(engine.PICK)
    rows = charts.table()
    keys = [charts.chart_key(row) for row in rows]
    frame = run.frames()
    numbers = charts.leave_charts()
    return {"frame": frame, "rank": rows[charts.PLAYER_ROW]["rank"],
            "key": keys[charts.PLAYER_ROW], "best_rival": min(keys[1:]),
            "placings": rows[charts.PLAYER_ROW]["placings"],
            "numbers": numbers}


def by_rank_then_return(seen):
    return seen["rank"], -seen["numbers"]["return"]


class Season:
    """The built park run to the first charts, deciding at every frame."""

    def __init__(self, orders=ORDERS):
        self._orders = orders
        self._ordered = False
        self._day = -1
        self._still = 0
        self._acts = {}
        self._trials = []
        self._asked = 0
        self._searched = 0

    def acts(self):
        return dict(self._acts)

    def trials(self):
        return list(self._trials)

    def searched(self):
        return self._searched

    def best(self):
        return min(self._trials, key=by_rank_then_return)

    def tick(self):
        """One frame: the first rule the park's state answers to, or a step."""
        if self._clock_is_held():
            return self._acted("modal", self._press_on)
        if self._gate_is_shut():
            return self._acted("gate", self._open_the_gate)
        if self._orders_are_due():
            return self._acted("orders", self._place_the_orders)
        if self._fare_is_under_worth():
            return self._acted("fare", self._reprice)
        run.step(1)
        return None

    def to_the_charts(self):
        started = run.frames()
        while run.watch("month") == JANUARY:
            self.tick()
        while run.watch("month") != JANUARY:
            self.tick()
        return run.frames() - started

    def _acted(self, what, doing):
        doing()
        self._acts[what] = self._acts.get(what, 0) + 1
        self._still = 0
        self._day = run.watch("day")
        return what

    def _clock_is_held(self):
        day = run.watch("day")
        self._still = self._still + 1 if day == self._day else 0
        self._day = day
        return self._still >= MODAL_STILL

    def _press_on(self):
        engine.tap(engine.PICK)

    def _gate_is_shut(self):
        if self._acts.get("gate", 0) >= GATE_TRIES:
            return False
        return visitor.park_byte(visitor.OPEN_BYTE) != GATE_OPEN

    def _open_the_gate(self):
        visitor.open_the_park()

    def _fare_target(self):
        return takings.worth_now() * FARE_OVER_WORTH // WORTH_UNIT

    def _fare_is_under_worth(self):
        return self._fare_target() >= self._asked + FARE_STEP

    def _reprice(self):
        self._asked = self._fare_target()
        takings.set_fare(self._asked)

    def _orders_are_due(self):
        return (not self._ordered and run.watch("month") == DECEMBER
                and run.watch("day") >= ORDER_DAY)

    def _hire(self, order):
        lively, pleasing = order
        for kind, count, tile in ((LIVELY_KIND, lively, LIVELY_TILE),
                                  (PLEASING_KIND, pleasing, PLEASING_TILE)):
            if not count:
                continue
            takings.take(takings.STAFF_ICON, kind)
            takings.move_to(tile)
            for _ in range(count):
                engine.tap(engine.PICK, engine.TAP_HOLD, HIRE_AFTER)

    def _trial(self, order):
        run.restore(DECEMBER_STATE)
        self._resume()
        self._hire(order)
        self.to_the_charts()
        seen = read_the_charts()
        seen["order"] = order
        return seen

    def _resume(self):
        self._ordered = True
        self._still = 0
        self._day = run.watch("day")

    def _place_the_orders(self):
        started = run.frames()
        run.checkpoint(DECEMBER_STATE)
        for order in self._orders:
            self._trials.append(self._trial(order))
        self._searched = run.frames() - started
        run.restore(DECEMBER_STATE)
        self._resume()
        self._hire(self.best()["order"])
