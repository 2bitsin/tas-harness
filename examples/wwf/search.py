"""Search the input space at point blank for a string that registers a combo."""

import itertools
import os
import pathlib

import tash

HERE = pathlib.Path(__file__).resolve().parent
TAPE = HERE / "tapes" / "title-to-first-bell.yaml"

# README.md, "The moves": the OPTIONS screen's own legend for the six.
BUTTONS = {"P": "l", "RUN": "x", "SP": "r", "K": "y", "BLK": "b", "SK": "a"}
DIRECTIONS = ("F", "B", "D", "U")
SYMBOLS = tuple(BUTTONS) + DIRECTIONS

HOLD_FRAMES = 2
SPACINGS = (2, 6)
TAIL_FRAMES = 80
LENGTHS = (2, 3, 4)
SHORT_LENGTHS = (2, 3)

# README.md, "What comes out, measured": a bare hit and the best single move.
ONE_HIT = 20
TWO_HITS = 2 * ONE_HIT
BEST_SINGLE = 46

# The sprites touch at a gap of 70 and a bare punch reaches to 90; under the
# touching gap the player walks through the opponent and everything whiffs,
# so the approach holds the gap inside that band instead of walking in.
TOUCH = 70
REACH = 90
WALK_TIMEOUT = 600
HIT_TIMEOUT = 40
GROUND_FRAMES = 24
# The ring, from holding one way for 400 frames at the bell.
RING_LEFT = 875
RING_RIGHT = 1318
ROPE_MARGIN = 80
ROPE_PUSHES = 5

# A combo is separate hits close together; two hits more than this many frames
# apart are two exchanges, which is what every crude loop already produces.
LINK_FRAMES = 30
# The banner along the apron, where the game prints FIRST ATTACK and 2X
# DAMAGE, and the health bars and names it prints a hit count over.
BANNER_STRIP = "0,192,256,24"
HUD_STRIP = "0,0,256,40"
SHOT_BUDGET = 24
CLOSEST = 6


def gap(run):
    """How far the opponent is to the right of the player."""
    return run.watch("cpu_x") - run.watch("p1_x")


def forward(run):
    """The pad direction that points at the opponent right now."""
    return "right" if gap(run) > 0 else "left"


def pad(run, symbol):
    """The libretro buttons one symbol of a string presses."""
    if symbol in BUTTONS:
        return [BUTTONS[symbol]]
    if symbol == "F":
        return [forward(run)]
    if symbol == "B":
        return ["left" if forward(run) == "right" else "right"]
    return ["down" if symbol == "D" else "up"]


def name_of(string, spacing, sustain):
    """How a candidate is written in a printed line and in the README."""
    return ("+".join(string) + f" @{spacing}"
            + (" sustained" if sustain else ""))


class Hits:
    """Counts the opponent's separate health drops, frame by frame."""

    def __init__(self, run):
        self.run = run
        self.health = run.watch("cpu_health")
        self.frame = 0
        self.damage = 0
        self.at = []

    def step(self, frames):
        for _ in range(frames):
            self.run.step(1)
            self.frame += 1
            now = self.run.watch("cpu_health")
            if now < self.health:
                if not self.at or self.frame - self.at[-1] > 1:
                    self.at.append(self.frame)
                else:
                    self.at[-1] = self.frame
                self.damage += self.health - now
            self.health = now

    def linked(self):
        """The longest run of hits no further apart than a link allows."""
        best = run = 1 if self.at else 0
        for earlier, later in zip(self.at, self.at[1:]):
            run = run + 1 if later - earlier <= LINK_FRAMES else 1
            best = max(best, run)
        return best


def land(run, symbol):
    """Throw one move and answer the damage it did, 0 when it missed."""
    before = run.watch("cpu_health")
    pressed = pad(run, symbol)
    run.hold(1, pressed)
    run.step(HOLD_FRAMES)
    run.release(1, pressed)
    for _ in range(HIT_TIMEOUT):
        run.step(1)
        if run.watch("cpu_health") != before:
            return before - run.watch("cpu_health")
    return 0


def connects(run, name):
    """Whether a bare punch lands from here, leaving the run where it was."""
    run.checkpoint(name)
    damage = land(run, "P")
    run.restore(name)
    return damage >= ONE_HIT


def close_in(run, name):
    """Hold the gap at point blank until a bare punch lands from a frame."""
    for _ in range(WALK_TIMEOUT):
        away = abs(gap(run))
        if TOUCH <= away <= REACH:
            if connects(run, name):
                return True
            run.step(1)
            continue
        step = forward(run) if away > REACH else pad(run, "B")[0]
        run.hold(1, [step])
        run.step(1)
        run.release(1)
    return False


def apply_string(run, string, spacing, sustain, hits):
    """Press one candidate string, the first symbol held when sustained."""
    held = pad(run, string[0]) if sustain else []
    if held:
        run.hold(1, held)
    for symbol in (string[1:] if sustain else string):
        pressed = pad(run, symbol)
        run.hold(1, pressed)
        hits.step(HOLD_FRAMES)
        loose = [button for button in pressed if button not in held]
        if loose:
            run.release(1, loose)
        hits.step(spacing)
    run.release(1)
    hits.step(TAIL_FRAMES)


class Sweep:
    """Applies every string from one checkpoint and keeps what stood out."""

    def __init__(self, run, name, hittable):
        self.run = run
        self.name = name
        self.hittable = hittable
        self.tried = 0
        self.beat_two = 0
        self.linked = 0
        self.best = []
        self.table = []
        self.shots = []
        run.checkpoint(name)

    def trial(self, string, spacing, sustain):
        self.run.restore(self.name)
        mine = self.run.watch("p1_health")
        hits = Hits(self.run)
        apply_string(self.run, string, spacing, sustain, hits)
        return hits, mine - self.run.watch("p1_health")

    def run_over(self, lengths, openers):
        for length in lengths:
            for opener in openers:
                for rest in itertools.product(SYMBOLS, repeat=length - 1):
                    for spacing in SPACINGS:
                        for sustain in (False, True):
                            self.record((opener,) + rest, spacing, sustain)
        return self

    def record(self, string, spacing, sustain):
        hits, taken = self.trial(string, spacing, sustain)
        self.tried += 1
        label = name_of(string, spacing, sustain)
        chain = hits.linked()
        self.best.append((hits.damage, chain, label))
        self.best.sort(key=lambda row: (-row[0], -row[1]))
        del self.best[CLOSEST:]
        if hits.damage <= TWO_HITS and chain < 2:
            return
        self.beat_two += hits.damage > TWO_HITS
        self.linked += chain >= 2
        print(f"{self.name}: {label} -> {hits.damage} damage over {chain}"
              f" linked hits at {hits.at}, {taken} taken", flush=True)
        if hits.damage > BEST_SINGLE or chain >= 2:
            self.table.append((hits.damage, chain, label))
            if len(self.shots) < SHOT_BUDGET:
                self.shots.append(self.run.look(region=BANNER_STRIP))
                self.shots.append(self.run.look(region=HUD_STRIP))

    def report(self):
        print(f"{self.name}: {self.tried} strings tried,"
              f" {self.beat_two} beat {TWO_HITS} damage,"
              f" {self.linked} linked two hits", flush=True)
        for damage, chain, label in self.best:
            print(f"CLOSEST {self.name} {damage} {chain} {label}", flush=True)


def contexts(run):
    """The checkpoints the search applies its strings from."""
    made = []
    run.restore("bell")
    made.append(Sweep(run, "standing", close_in(run, "standing")))

    run.restore("standing")
    land(run, "P")
    made.append(Sweep(run, "stunned", False))

    run.restore("standing")
    land(run, "SK")
    run.step(GROUND_FRAMES)
    made.append(Sweep(run, "grounded", False))

    run.restore("standing")
    for _ in range(ROPE_PUSHES):
        if not (RING_LEFT + ROPE_MARGIN < run.watch("cpu_x")
                < RING_RIGHT - ROPE_MARGIN):
            break
        land(run, "P")
        if not close_in(run, "pushing"):
            break
    print("ropes: opponent at", run.watch("cpu_x"), flush=True)
    made.append(Sweep(run, "ropes", close_in(run, "ropes")))
    return made


def lengths_from(name, fallback):
    """The string lengths to sweep, shortened for a rehearsal."""
    setting = os.environ.get(name)
    if not setting:
        return fallback
    return tuple(int(number) for number in setting.split(","))


def names_from(name, fallback):
    """A slice of the sweep, so the long one can run as several processes."""
    setting = os.environ.get(name)
    if not setting:
        return fallback
    return tuple(setting.split(","))


def main():
    run = tash.run
    long_lengths = lengths_from("WWF_LENGTHS", LENGTHS)
    short_lengths = lengths_from("WWF_SHORT_LENGTHS", SHORT_LENGTHS)
    openers = names_from("WWF_OPENERS", SYMBOLS)
    wanted = names_from("WWF_CONTEXTS", ())
    run.play(TAPE)
    run.checkpoint("bell")
    made = contexts(run)
    print("contexts:", ", ".join(
        f"{one.name}{'' if one.hittable else ' (no bare hit)'}"
        for one in made), flush=True)
    registered = []
    for sweep in made:
        if wanted and sweep.name not in wanted:
            continue
        want = long_lengths if sweep.name == "standing" else short_lengths
        sweep.run_over(want, openers).report()
        registered.extend((sweep.name, damage, chain, label)
                          for damage, chain, label in sweep.table)
    registered.sort(key=lambda row: (-row[2], -row[1]))
    for name, damage, chain, label in registered:
        print(f"TABLE {name} {damage} {chain} {label}", flush=True)
    run.judge("the input space was swept", True,
              f"{len(registered)} strings beat {BEST_SINGLE} damage"
              f" or linked two hits")


main()
