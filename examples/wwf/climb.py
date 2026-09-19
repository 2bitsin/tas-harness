"""Filling the combo meter: a search over the move table, meter a frame."""

import combos
import tash

ROWS = {row[0]: row for row in combos.COMBOS}
STEPS = ("step in", "step out", "guard")
CANDIDATES = tuple(ROWS) + STEPS
STEP_FRAMES = 8
GUARD_FRAMES = 24
REST_FRAMES = 16
METER_WEIGHT = 60.0
TAKEN_WEIGHT = 2.0
BAND_WEIGHT = 0.1
DECISIONS = 60
BELL_FRAMES = 3000
WHOLE = 164


class Climb:
    """Plays whatever the search says raises the meter fastest from here."""

    def __init__(self, run, place):
        self.run = run
        self.place = place
        self.last = (0, 0, 1)
        self.decisions = 0
        self.trials = 0

    def apply(self, run, name):
        started = run.frames()
        before = (run.watch("p1_health"), self.place.meter())
        if name in ROWS:
            combos.perform(run, ROWS[name])
        elif name == "guard":
            run.hold(1, [combos.BUTTONS["BLK"]])
            run.step(GUARD_FRAMES)
            run.release(1)
            run.step(REST_FRAMES)
        else:
            way = (combos.forward(run) if name == "step in"
                   else combos.back(run))
            run.hold(1, [way])
            run.step(STEP_FRAMES)
            run.release(1)
            run.step(REST_FRAMES)
        self.last = (before[0] - run.watch("p1_health"),
                     self.place.meter() - before[1], run.frames() - started)

    def score(self, run):
        taken, gained, frames = self.last
        away = abs(combos.gap(run))
        band = max(0, away - combos.REACH) + max(0, combos.TOUCH - away)
        return (METER_WEIGHT * gained - TAKEN_WEIGHT * taken
                - BAND_WEIGHT * band) / max(1, frames)

    def bell(self):
        """Step one frame at a time until both wrestlers are whole again."""
        for waited in range(BELL_FRAMES):
            self.run.step(1)
            if (self.run.watch("p1_health") == WHOLE
                    and self.run.watch("cpu_health") == WHOLE):
                return waited
        return -1

    def fill(self, report=None):
        """Decide until the meter is at the gate's threshold or time runs."""
        run = self.run
        for decision in range(DECISIONS):
            if self.place.loaded():
                return True
            if run.watch("p1_health") == 0 or run.watch("cpu_health") == 0:
                self.bell()
                continue
            found = tash.search(CANDIDATES, self.apply, self.score)
            self.apply(run, found.best)
            self.decisions += 1
            self.trials += found.trials
            if report:
                report(decision, found.best, self.place.meter(),
                       self.place.drawn(), found.trials)
        return self.place.loaded()
