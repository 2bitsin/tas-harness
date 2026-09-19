"""The dice called in advance: one register read against twenty rolls."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import dice
import engine
import state

SEAT = 0
RIVAL = 1
PORT = 1
SWEEP = 36
IDLE_SWEEP = 48
ROLLS = 20
BUDGET = 16000
TURN_BUDGET = 1400
SETTLE_AFTER_TAPE = 600
TAPE = HERE / "tapes" / "title-to-first-roll.yaml"
CHECKPOINT = "first-roll"
IDLE = (dice.IDLE_FACE, dice.IDLE_FACE)
STALE_PRESSES = 3

run = tash.run


def dice_now(run):
    raw = run.memory("system", state.DICE, 2)
    return raw[0], raw[1]


def press_and_wait(run, offset):
    """Idle offset frames, press Genesis A, and answer the dice it drew."""
    run.restore(CHECKPOINT)
    run.release(PORT)
    if offset:
        run.step(offset)
    register = dice.register_of(run)
    run.hold(PORT, [engine.CONFIRM])
    run.step(engine.PRESS_FRAMES)
    run.release(PORT, [engine.CONFIRM])
    run.step(dice.SAMPLE_DELAY - engine.PRESS_FRAMES + 4)
    return register, dice_now(run)


def sweep(run, span=SWEEP):
    """Every press frame in a span, against the roll called for it."""
    rows = []
    run.restore(CHECKPOINT)
    run.release(PORT)
    base = dice.register_of(run)
    for offset in range(span):
        register, got = press_and_wait(run, offset)
        rows.append((offset, register, got, dice.roll_at(base, offset),
                     dice.roll_at(register, 0)))
    return base, rows


class Caller:
    """Carries the register from one read and calls every roll before it."""

    def __init__(self, run):
        run.release(PORT)
        self.run = run
        self.register = dice.register_of(run)
        self.ticks = dice.counter_of(run)
        self.index = 0
        self.block = run.memory("system", *state.STATE_BLOCK)
        self.still = 0
        self.pressing = 0
        self.gap = 0
        self.stale = 0
        self.frame = 0
        self.armed = True
        self.presses = []
        self.calls = {}
        self.landed = []
        self.dice = dice_now(run)
        self.seat = state.current(run)

    def clock(self):
        ticks = dice.counter_of(self.run)
        self.register = dice.advance(self.register, ticks - self.ticks)
        self.index += ticks - self.ticks
        self.ticks = ticks

    def call(self, seat, delay):
        at = self.index + delay
        self.calls[at] = (seat, dice.roll_at(self.register, delay
                                             - dice.SAMPLE_DELAY - 1),
                          self.frame)
        return self.calls[at]

    def land(self, got):
        drawn = dice.HUMAN_DRAWS if self.seat == SEAT else dice.RIVAL_DRAWS
        at = self.index + drawn - 1
        called = self.calls.get(at)
        self.landed.append({"frame": self.frame, "seat": self.seat,
                            "dice": got, "called": None if not called
                            else called[1], "clocks": at,
                            "ahead": None if not called
                            else self.frame - called[2]})
        self.index += drawn
        self.register = dice.advance(self.register, drawn)
        self.armed = True
        return self.landed[-1]

    def press(self):
        button = (engine.CANCEL if self.stale >= STALE_PRESSES
                  and engine.in_menu(self.run) else engine.CONFIRM)
        if (button == engine.CONFIRM and self.armed and self.seat == SEAT
                and self.dice == IDLE):
            self.call(SEAT, dice.SAMPLE_DELAY + 1)
            self.armed = False
        self.presses.append((self.frame, self.seat))
        self.run.hold(PORT, [button])
        self.pressing = engine.PRESS_FRAMES

    def tick(self, press=True):
        self.run.step(1)
        self.frame += 1
        self.clock()
        was, self.dice = self.dice, dice_now(self.run)
        self.seat = state.current(self.run)
        if was != self.dice:
            if self.dice == IDLE and self.seat == RIVAL:
                self.call(RIVAL, dice.RIVAL_DELAY)
            elif self.dice != IDLE:
                self.land(self.dice)
        block = self.run.memory("system", *state.STATE_BLOCK)
        self.still = self.still + 1 if block == self.block else 0
        self.stale = self.stale if block == self.block else 0
        self.block = block
        if self.pressing:
            self.pressing -= 1
            if not self.pressing:
                self.run.release(PORT)
                self.gap = engine.MENU_GAP
                self.stale += 1
            return
        if self.gap:
            self.gap -= 1
            return
        if press and self.still >= engine.STILL_FRAMES:
            self.press()
            self.still = 0

    def play(self, rolls, budget):
        while len(self.landed) < rolls and self.frame < budget:
            self.tick()
        return self.landed

    def rolls_of(self, seat):
        return [row for row in self.landed if row["seat"] == seat]


def idled(run, press_at, idle):
    """Hold the press at press_at back by idle frames; answer the rival's
    first roll after it."""
    run.restore(CHECKPOINT)
    caller = Caller(run)
    while caller.frame < press_at - 1:
        caller.tick()
    for _ in range(idle):
        caller.tick(press=False)
    caller.press()
    limit = caller.frame + TURN_BUDGET
    while not caller.rolls_of(RIVAL) and caller.frame < limit:
        caller.tick()
    rolled = caller.rolls_of(RIVAL)
    return rolled[0] if rolled else None


def answering_press(run, presses):
    """The last human press the rival's roll follows: two idles of it draw
    two different rolls."""
    for frame, seat in reversed([p for p in presses if p[1] == SEAT]):
        early, late = idled(run, frame, 0), idled(run, frame, 2)
        if early and late and early["dice"] != late["dice"]:
            return frame, (early["dice"], late["dice"])
    return None, None


def main():
    run.mark("tape")
    played = run.play(TAPE)
    run.step(SETTLE_AFTER_TAPE)
    run.expect("the tape reached the first roll", played["segments"] == 10,
               f"{played['segments']} segments, {played['frames']} frames")
    run.checkpoint(CHECKPOINT)
    run.mark("sweep")
    base, rows = sweep(run)
    for offset, register, got, called, own in rows:
        print("offset %2d register %5d dice %s called %s"
              % (offset, register, got, called), flush=True)
    matched = sum(got == called == own for _, _, got, called, own in rows)
    run.expect(f"{SWEEP} press frames, {SWEEP} rolls called from one read",
               matched == SWEEP, f"{matched} of {SWEEP} match, register "
               f"{base}")
    run.mark("calls")
    run.restore(CHECKPOINT)
    caller = Caller(run)
    landed = caller.play(ROLLS, BUDGET)
    for row in landed:
        print("roll", row, flush=True)
    hit = sum(row["called"] == row["dice"] for row in landed)
    ahead = [row["ahead"] for row in landed if row["ahead"]]
    run.expect(f"{ROLLS} rolls called in advance for both seats",
               hit == len(landed) == ROLLS,
               f"{hit} of {len(landed)} matched, called {min(ahead)} to "
               f"{max(ahead)} frames ahead")
    run.expect("the model still holds the machine's register",
               caller.register == dice.register_of(run),
               f"model {caller.register} after {caller.index} clocks")
    run.mark("idle")
    run.restore(CHECKPOINT)
    scout = Caller(run)
    while not scout.rolls_of(RIVAL) and scout.frame < TURN_BUDGET:
        scout.tick()
    baseline = scout.rolls_of(RIVAL)[0]
    press_at, moved = answering_press(run, scout.presses)
    run.expect("the rival's roll follows the human's last answered press",
               press_at is not None,
               f"the press at frame {press_at} draws {moved} where the run "
               f"drew {baseline['dice']}")
    idles = [idled(run, press_at, k) for k in range(IDLE_SWEEP)]
    for k, row in enumerate(idles):
        print("idle", k, row, flush=True)
    chosen = sum(row is not None and row["called"] == row["dice"]
                 for row in idles)
    totals = sorted({sum(row["dice"]) for row in idles if row})
    run.expect(f"{IDLE_SWEEP} idle frames choose {IDLE_SWEEP} rival rolls",
               chosen == IDLE_SWEEP,
               f"{chosen} of {IDLE_SWEEP} called, totals {totals}")
    run.judge("the dice are a choice, not a hazard", True,
              f"{matched} of {SWEEP} press frames, {hit} of {len(landed)} "
              f"rolls and {chosen} of {IDLE_SWEEP} idle frames called from "
              f"one register read")
    run.report()


main()
