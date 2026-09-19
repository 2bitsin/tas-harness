"""The idle before our last answered press, against the roll the rival gets."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import dice
import engine
import state

TAPE = "examples/monopoly/tapes/title-to-first-roll.yaml"
SEAT = 0
RIVAL = 1
IDLES = 201
PROBE_IDLE = 5
PRESS_LIMIT = 40
SETTLE_AFTER_TAPE = 600
RESET = "watch die_1 equal 7"
ROLLED = "not watch die_1 equal 7"
RESET_LIMIT = 1200
ROLL_LIMIT = 1200
NAMES = ("prompt-0", "prompt-1", "prompt-2")
CALL_SPAN = 400


class Sweep:
    """Our roll prompt, swept over the idle before one press."""

    def __init__(self, run):
        self.run = run
        self.rows = {}

    def begin(self):
        try:
            self.run.restore(NAMES[0])
        except RuntimeError:
            played = self.run.play(TAPE)
            print("tape", played, flush=True)
            self.run.step(SETTLE_AFTER_TAPE)
            self.run.checkpoint(NAMES[0])
        self.run.release(engine.PORT)

    def next_prompt(self, limit=PRESS_LIMIT):
        """Press at every rest until the seat has been round and back."""
        run = self.run
        away = False
        for _ in range(limit):
            engine.settle(run)
            if state.current(run) != SEAT:
                away = True
            elif away and engine.roll_prompt(run):
                return True
            engine.press(run, engine.CONFIRM, after=0)
        return False

    def take(self, name):
        self.next_prompt()
        self.run.checkpoint(name)
        self.run.release(engine.PORT)
        return {"frame": self.run.frames(),
                "register": dice.register_of(self.run),
                "counter": dice.counter_of(self.run),
                "cash": [state.player(self.run, s)["cash"] for s in (0, 1)],
                "square": [state.player(self.run, s)["square"]
                           for s in (0, 1)]}

    def line(self, name, at, idle):
        """Replay the turn with idle frames held back before press `at`."""
        run = self.run
        run.restore(name)
        run.release(engine.PORT)
        seen = None
        for index in range(PRESS_LIMIT):
            engine.settle(run)
            if state.current(run) != SEAT:
                break
            if index == at:
                if idle:
                    run.step(idle)
                seen = (dice.register_of(run), dice.counter_of(run))
            engine.press(run, engine.CONFIRM, after=0)
        try:
            run.run_until(RESET, RESET_LIMIT)
            run.run_until(ROLLED, ROLL_LIMIT)
        except RuntimeError:
            return seen, None
        return seen, state.dice(run)

    def presses(self, name):
        """How many presses our turn takes when every rest is answered."""
        run = self.run
        run.restore(name)
        run.release(engine.PORT)
        count = 0
        for _ in range(PRESS_LIMIT):
            engine.settle(run)
            if state.current(run) != SEAT:
                break
            engine.press(run, engine.CONFIRM, after=0)
            count += 1
        return count

    def answering(self, name, count):
        """The last press of ours whose delay still moves the rival's roll."""
        for at in reversed(range(1, count)):
            early = self.line(name, at, 0)[1]
            late = self.line(name, at, PROBE_IDLE)[1]
            if early and late and early != late:
                return at, early, late
        return None, None, None

    def sweep(self, name, at):
        rows = []
        for idle in range(IDLES):
            seen, got = self.line(name, at, idle)
            rows.append({"idle": idle, "register": seen[0] if seen else None,
                         "counter": seen[1] if seen else None, "dice": got})
            if idle % 20 == 0:
                print("  idle %3d register %5d counter %6d dice %s"
                      % (idle, rows[-1]["register"], rows[-1]["counter"],
                         got), flush=True)
        self.rows[name] = rows
        return rows

    @staticmethod
    def called(rows, span=CALL_SPAN):
        """The clock offset from our press at which the rival draws."""
        best, hits = None, 0
        for offset in range(span):
            got = sum(1 for row in rows if row["dice"]
                      and dice.roll_at(row["register"], offset) == row["dice"])
            if got > hits:
                best, hits = offset, got
        return best, hits

    @staticmethod
    def steps(rows):
        """Collapse the sweep into the runs of idle that share one roll."""
        out = []
        for row in rows:
            if out and out[-1]["dice"] == row["dice"]:
                out[-1]["last"] = row["idle"]
                continue
            out.append({"first": row["idle"], "last": row["idle"],
                        "dice": row["dice"], "register": row["register"],
                        "counter": row["counter"]})
        return out


def report_one(run, sweeper, name, opened):
    count = sweeper.presses(name)
    at, early, late = sweeper.answering(name, count)
    if at is None:
        at = count - 1
    print("%s frame %d register %d counter %d cash %s square %s"
          % (name, opened["frame"], opened["register"], opened["counter"],
             opened["cash"], opened["square"]), flush=True)
    print("%s: %d presses, the answering press is %s (%s -> %s)"
          % (name, count, at, early, late), flush=True)
    rows = sweeper.sweep(name, at)
    steps = sweeper.steps(rows)
    for step in steps:
        print("%s step idle %3d..%-3d dice %s total %s register %5d "
              "counter %6d" % (name, step["first"], step["last"],
                               step["dice"], sum(step["dice"]),
                               step["register"], step["counter"]), flush=True)
    totals = sorted({sum(step["dice"]) for step in steps})
    offset, hits = sweeper.called(rows)
    print("%s: %d steps over %d idle frames, totals %s; called at offset "
          "%s, %d of %d" % (name, len(steps), IDLES, totals, offset, hits,
                            len(rows)), flush=True)
    run.expect("%s: the rival's roll steps with the idle" % name,
               len(steps) > 1, "%d steps over %d idle frames, totals %s"
               % (len(steps), IDLES, totals))
    run.expect("%s: every rival roll called from the press register" % name,
               hits == len(rows), "offset %s, %d of %d"
               % (offset, hits, len(rows)))
    return {"name": name, "presses": count, "at": at, "steps": steps,
            "totals": totals, "opened": opened, "offset": offset,
            "hits": hits}


def main(run):
    sweeper = Sweep(run)
    sweeper.begin()
    told = []
    opened = {"frame": run.frames(), "register": dice.register_of(run),
              "counter": dice.counter_of(run),
              "cash": [state.player(run, s)["cash"] for s in (0, 1)],
              "square": [state.player(run, s)["square"] for s in (0, 1)]}
    told.append(report_one(run, sweeper, NAMES[0], opened))
    for name in NAMES[1:]:
        run.restore(told[-1]["name"])
        run.release(engine.PORT)
        told.append(report_one(run, sweeper, name, sweeper.take(name)))
    run.judge("the rival's roll is a step function of the idle", True,
              "; ".join("%s %d steps, totals %s"
                        % (row["name"], len(row["steps"]), row["totals"])
                        for row in told))
    print(told, flush=True)
    print(run.report(), flush=True)


main(tash.run)
