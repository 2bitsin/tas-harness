"""Bret Hart's super combos, the reversal and the gate, proved at VERY HARD."""

import pathlib
import sys

try:
    HERE = pathlib.Path(__file__).resolve().parent
except NameError:
    HERE = pathlib.Path("examples/wwf").resolve()
sys.path.insert(0, str(HERE))

import climb
import combos
import ring
import supers
import tash

WHOLE = 164
FLOOR = 70
GRAB_TRIES = 40
TAIL = 260
HELD_FRAMES = 4
BELL = "supers-bell"
CHARGED = "supers-charged"
FRESH = "supers-fresh"
HOLD_POINT = "supers-hold"
HELD_POINT = "supers-held"
WAIT_HELD = 1200
FALL_WAIT = 2500
DOWN_WAIT = 900
BELL_WAIT = 6000
PIN_MASH = ("P", "K", "SP", "SK")
PROVOCATIONS = ("still", "guard")
LONG, PUBLISHED, MASH = supers.FOLLOWS[0], supers.FOLLOWS[1], supers.FOLLOWS[2]
ROWS = {row[0]: row for row in combos.COMBOS}
CASES = (("P", LONG), ("P", PUBLISHED), ("P", ()), ("SK", PUBLISHED),
         ("SK", ()), ("SP", ()), ("K", ()))
# The two health bars and the meter bar under the left one.
BAR_CROP = "0,16,256,24"
PURPLE = (168, 32, 96)
# Rows 26 to 29, x 58 to 111: the player's meter bar, nothing else.
BAR_INTERIOR = "58,26,54,4"
FLASH_DRAWN = 14
BAR_SETTLE = 40
CHARGER = "Super Punch"
BAR_POINT = "supers-bar"
BAR_MOVES = 40
BAR_SLOPE = 16
BAR_BASE = 8
BAR_SLACK = 2
BAR_LEAST = 6


class Chain:
    """Hits, damage and animation ids from the frame an initiator is typed."""

    def __init__(self, run, place):
        self.run = run
        self.place = place
        self.health = run.watch("cpu_health")
        self.mine = run.watch("p1_health")
        self.damage = 0
        self.taken = 0
        self.at = []
        self.ids = []
        self.frame = 0

    def step(self, frames):
        for _ in range(frames):
            self.run.step(1)
            self.frame += 1
            self.ids.append(self.place.action())
            now = self.run.watch("cpu_health")
            if now < self.health:
                self.at.append(self.frame)
                self.damage += self.health - now
            self.health = now
            mine = self.run.watch("p1_health")
            self.taken += max(0, self.mine - mine)
            self.mine = mine

    def tap(self, pressed, hold=ring.TAP_HOLD, gap=ring.TAP_GAP):
        seen = 0
        self.run.hold(1, pressed)
        for _ in range(hold):
            self.step(1)
            seen |= self.place.code()
        self.run.release(1, pressed)
        for _ in range(gap):
            self.step(1)
            seen |= self.place.code()
        return seen

    def seen(self):
        runs, last = [], None
        for value in self.ids:
            if last != value:
                runs.append([value, 0])
                last = value
            runs[-1][1] += 1
        return {value for value, length in runs
                if length >= HELD_FRAMES and value != combos.STANDING}


def take_hold(run, place, source, name):
    """Grab until the hold state shows, from a context worth grabbing from."""
    run.restore(source)
    for attempt in range(GRAB_TRIES):
        if run.watch("p1_health") < FLOOR:
            run.restore(source)
            run.step(attempt * 13)
        combos.approach(run, "supers-approach")
        if supers.grab(place) >= 0:
            run.checkpoint(name)
            return True
    return False


def play(run, place, button, follow):
    """Type forward, forward, button and then the branch, reading frames."""
    chain = Chain(run, place)
    way = combos.forward(run)
    if not chain.tap([way]) & ring.TOWARD:
        way = ring.OTHER[way]
        chain.tap([way])
    chain.tap([way])
    chain.tap([combos.BUTTONS[button]])
    for name in follow:
        for _ in range(supers.CHAIN_TAPS):
            chain.tap([combos.BUTTONS[name]], supers.CHAIN_HOLD,
                      supers.CHAIN_GAP)
    chain.step(TAIL)
    return chain


def evidence(chain, before, after, whole):
    return (f"meter {before} -> {after}, opponent {whole} at the hold,"
            f" {len(chain.at)} hits for"
            f" {chain.damage} damage at frames {chain.at}, {chain.taken}"
            f" taken, ids {sorted(hex(v) for v in chain.seen())}")


def prove_case(run, place, source, tag, button, follow):
    name = f"{HOLD_POINT}-{tag}-{button}"
    if not take_hold(run, place, source, name):
        run.judge(f"{tag} meter: F,F,{button} from the hold", False,
                  "no hold could be taken from this context")
        return
    before = place.meter()
    whole = run.watch("cpu_health")
    chain = play(run, place, button, follow)
    fired = supers.INITIATORS.get(button) in chain.seen()
    wanted = tag == "full" and button in supers.INITIATORS
    text = (f"F,F,{button}"
            + (" + " + ",".join(follow) if follow else "")
            + f": {evidence(chain, before, place.meter(), whole)}")
    run.judge(f"{tag} meter: F,F,{button} from the hold", fired == wanted,
              text)
    print(f"{tag} {text}", flush=True)


def provoke(run, place, kind):
    """Frames until the opponent's hold shows while waiting that way."""
    if kind == "guard":
        run.hold(1, [combos.BUTTONS["BLK"]])
    found = -1
    for waited in range(WAIT_HELD):
        run.step(1)
        if place.held():
            found = waited
            break
        if 0 in (run.watch("p1_health"), run.watch("cpu_health")):
            break
    run.release(1)
    return found


def prove_reversal(run, place):
    """Take one hold both ways from the same frame, and compare."""
    told = []
    for kind in PROVOCATIONS:
        run.restore(BELL)
        combos.approach(run, "supers-approach")
        found = provoke(run, place, kind)
        if found < 0:
            told.append((kind, found, 0, 0, 0, False))
            continue
        run.checkpoint(HELD_POINT)
        chain = Chain(run, place)
        supers.reverse(place)
        chain.step(TAIL)
        fired = supers.REVERSAL in chain.seen()
        run.restore(HELD_POINT)
        still = Chain(run, place)
        still.step(TAIL)
        told.append((kind, found, chain.taken, chain.damage, still.taken,
                     fired))
    numbers = "; ".join(f"{kind}: held after {found} frames, F,F,SP lost"
                        f" {taken} while dealing {dealt} and standing lost"
                        f" {stood}, {hex(supers.REVERSAL)} seen {fired}"
                        for kind, found, taken, dealt, stood, fired in told)
    run.judge("the reversal out of the opponent's hold",
              any(fired and taken < stood
                  for _, _, taken, _, stood, fired in told), numbers)
    print("reversal " + numbers, flush=True)


def purple(run):
    """The most purple the bar's crop shows over a flash's worth of it."""
    most = 0
    for _ in range(BAR_SETTLE):
        most = max(most, run.colours(BAR_INTERIOR, PURPLE))
        run.step(1)
    return most


def prove_bar(run, place):
    """Count the bar's purple at every drawn value the meter walks up."""
    run.restore(BELL)
    rows = {}
    for _ in range(BAR_MOVES):
        if 0 in (run.watch("p1_health"), run.watch("cpu_health")):
            bell(run)
        rows.setdefault(place.drawn(), purple(run))
        if place.drawn() > FLASH_DRAWN:
            break
        combos.approach(run, BAR_POINT)
        combos.perform(run, ROWS[CHARGER])
    run.restore(CHARGED)
    flashing = purple(run)
    walked = {value: count for value, count in rows.items() if count}
    numbers = ", ".join(f"{value}: {count}"
                        for value, count in sorted(rows.items()))
    run.judge("the bar is 16 * drawn - 8 purple, and none under COMBO!",
              flashing == 0 and len(walked) >= BAR_LEAST
              and all(abs(count - BAR_SLOPE * value + BAR_BASE)
                      <= BAR_SLACK for value, count in walked.items()),
              f"purple pixels of {BAR_INTERIOR} by drawn value --"
              f" {numbers}; at drawn {place.drawn()}, with COMBO! up,"
              f" {flashing}")
    print(f"bar {numbers}; COMBO! at drawn {place.drawn()}: {flashing}",
          flush=True)


def fall(run, place):
    """Stand still until the health is gone, and answer the meter there."""
    for _ in range(FALL_WAIT):
        run.step(1)
        if run.watch("p1_health") == 0:
            return place.meter()
    return -1


def bell(run):
    """Run on to the next bell, both wrestlers whole again."""
    for waited in range(BELL_WAIT):
        run.step(1)
        if run.watch("p1_health") == WHOLE == run.watch("cpu_health"):
            return waited
    return -1


def rise(run, place):
    """Mash from the mat and answer the taps until the feet are back."""
    pressed = [combos.BUTTONS[name] for name in PIN_MASH]
    for waited in range(DOWN_WAIT):
        place.tap(pressed)
        if run.watch("p1_health") > 0:
            return waited
    return -1


def prove_pin(run, place):
    """The meter against the fall being lost, the two by two of both."""
    told = []
    for tag, source in (("full ", CHARGED), ("empty", BELL)):
        for falls in (1, 2):
            run.restore(source)
            at = -1
            for i in range(falls):
                at = fall(run, place)
                if i + 1 < falls:
                    bell(run)
            waited = rise(run, place)
            told.append((f"{tag} fall {falls}", at, waited, place.meter(),
                         run.watch("p1_health"), run.watch("cpu_health")))
    numbers = "; ".join(f"{tag}: meter {at} at the fall, feet back after"
                        f" {waited} taps with meter {left}, health {mine}"
                        f" and opponent {other}"
                        for tag, at, waited, left, mine, other in told)
    deciding, first = told[1], told[0]
    run.judge("a full meter buys one rise on the fall that decides",
              deciding[3] == 0 < deciding[4] < WHOLE
              and deciding[2] < first[2], numbers)
    print("pin " + numbers, flush=True)


def main():
    run = tash.run
    run.mark("tape")
    played = run.play(combos.HARDEST_TAPE)
    watches = run.observe()["watches"]
    run.expect("the tape reached the first bell at VERY HARD",
               played["segments"] == 5 and watches["skill"] == 10,
               f"{played['segments']} segments, {played['frames']} frames,"
               f" skill {watches['skill']}")
    base = ring.find(run)
    place = ring.Ring(run, base)
    run.expect("the player's object is found", base is not None,
               f"object {base and hex(base)}, record {place.record()},"
               f" meter at {hex(ring.RECORDS + 14 * place.record() + 0xA)}")
    run.checkpoint(BELL)
    run.mark("charge")
    climber = climb.Climb(run, place)

    def report(decision, best, meter, drawn, trials):
        print(f"  charge d{decision} {best}: meter {meter} drawn {drawn}"
              f" ({trials} trials)", flush=True)

    filled = climber.fill(report)
    run.judge("the meter reaches the gate's threshold", filled,
              f"meter {place.meter()}, drawn {place.drawn()} after"
              f" {climber.decisions} decisions and {climber.trials} trials;"
              f" the gate at ROM $3ef52 compares it with {ring.FULL}")
    run.checkpoint(CHARGED)
    run.look(path=None, region=BAR_CROP)
    run.look()
    waited = climber.bell()
    run.judge("the meter survives the fall and the bell", place.loaded(),
              f"meter {place.meter()} at the next bell after {waited}"
              f" frames, both wrestlers at {run.watch('cpu_health')}")
    run.checkpoint(FRESH)
    run.mark("combos")
    for button, follow in CASES:
        prove_case(run, place, FRESH, "full", button, follow)
    run.restore(BELL)
    for button, follow in (("P", PUBLISHED), ("SK", ())):
        prove_case(run, place, BELL, "empty", button, follow)
    run.mark("reversal")
    prove_reversal(run, place)
    run.mark("pin")
    prove_pin(run, place)
    run.mark("bar")
    prove_bar(run, place)
    run.report()


main()
