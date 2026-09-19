"""Ten scouted rounds at the high-limit roulette, the casino's fastest."""

import os
import sys
import tash

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hand
import scenario

run = tash.run
BALL = 0xD886
RNG = 0x288A
DOUBLE_ZERO = 37
ROUNDS = 10
CHIP = 500
# The banner says MAXIMUM $50000, and that is 100 chips on one square.
STACK_CAP = 100
STRAIGHT_UP = 36 * CHIP
TABLE = (624, 115)
BANNER_FRAMES = 240
SETTLE = 24
PAY_FRAMES = 810
PAY_STEP = 30
PAY_STILL = 3
SPIN_FRAMES = 1
SPIN_LEAD = 400
PAD_NUDGE = 300
REHEARSAL = 1
OPEN_FRAMES = 1
OPEN_AFTER = 4
CLICK_FRAMES = 6
CLICK_AFTER = 0
CLOSE_FRAMES = 1
CLOSE_AFTER = 4
DROP_FRAMES = 1
RACK_HOLD = 0
RACK_SWEEP = 96
CHIP_TRIES = 3
CHIP_SETTLE = 8
# Table-world geometry: a settled hand lands on a 16 pixel lattice and
# only the lattice point named here pays straight up. Measured.
COLUMN_X = 80
COLUMN_PITCH = 32
ROW_Y = (120, 88, 56)
ZERO = (48, 112)
DOUBLE = (48, 64)
AIM_TRIES = 2
AIM_SLACK = 4


def word(address):
    """Answer the byte-swapped word at an address."""
    return int.from_bytes(run.memory("system", address, 2), "little")


def ball():
    """Answer the pocket the ball is in; 37 is the double zero."""
    return word(BALL)


def credited():
    """Wait for the table to finish paying; answer the wallet."""
    run.step(PAY_FRAMES)
    last = scenario.money()
    still = 0
    while still < PAY_STILL:
        run.step(PAY_STEP)
        now = scenario.money()
        still = still + 1 if now == last else 0
        last = now
    return last


def square(number):
    """Answer the table-world point of a straight-up number's square."""
    if number == 0:
        return ZERO
    if number == DOUBLE_ZERO:
        return DOUBLE
    return (COLUMN_X + COLUMN_PITCH * ((number + 2) // 3 - 1),
            ROW_Y[(number - 1) % 3])


def rack(frames):
    """Hold right that long; the $500 rack is the panel's right clamp."""
    if not frames:
        return
    run.hold(1, ["right"])
    run.step(frames)
    run.release(1)


def aim(spot):
    """Point at a square, once, and check the hand arrived before the drop."""
    at = hand.at(run)
    for _ in range(AIM_TRIES):
        at = hand.move_to(run, *spot)
        if (abs(at[0] - spot[0]) <= AIM_SLACK
                and abs(at[1] - spot[1]) <= AIM_SLACK):
            break
    return at


def chip(spot, sweep):
    """Carry one $500 chip from the wallet panel onto the square."""
    hand.press(run, "a", OPEN_FRAMES, OPEN_AFTER)
    rack(sweep)
    hand.press(run, "b", CLICK_FRAMES, CLICK_AFTER)
    hand.press(run, "a", CLOSE_FRAMES, CLOSE_AFTER)
    aim(spot)
    hand.press(run, "b", DROP_FRAMES, 0)


def stake(count, number):
    """Carry that many $500 chips one at a time onto a number's square."""
    spot = square(number)
    want = scenario.money() - count * CHIP
    for _ in range(CHIP_TRIES):
        left = (scenario.money() - want) // CHIP
        if left <= 0:
            return
        sweep = RACK_SWEEP
        for _ in range(left):
            chip(spot, sweep)
            sweep = RACK_HOLD
        run.step(CHIP_SETTLE)


def spin():
    """Genesis A starts the wheel; the pocket is written within SETTLE."""
    was = word(RNG)
    hand.press(run, "y", SPIN_FRAMES, SETTLE)
    return ball(), word(RNG) != was


def scout(pad):
    """Answer the pocket a spin at that offset draws, leaving the state."""
    run.step(pad)
    return spin()


def scouted(name, count):
    """Rehearse the stake for its length, scout on it, then bet on the draw."""
    run.checkpoint(name)
    at = run.frames()
    stake(count, REHEARSAL)
    run.step(SPIN_LEAD)
    pad = run.frames() - at
    drawn, moved = spin()
    while True:
        while not moved:
            run.restore(name)
            pad += PAD_NUDGE
            drawn, moved = scout(pad)
        run.restore(name)
        at = run.frames()
        stake(count, drawn)
        spent = run.frames() - at
        if spent <= pad:
            break
        run.restore(name)
        pad = spent + SPIN_LEAD
        drawn, moved = scout(pad)
    run.step(pad - spent)
    spin()
    return drawn, pad, spent


def walk_to_table():
    """Walk from the floor to the high-limit table and open it."""
    for waypoint in scenario.NORTH:
        scenario.walk(waypoint)
    reached = scenario.walk(TABLE)
    if abs(reached[0] - TABLE[0]) > scenario.NEAR:
        reached = scenario.seek(TABLE)
    scenario.enter()
    run.step(BANNER_FRAMES)
    return reached


def round_at(name, count):
    """Scout the draw, restore, stack the chips on it and spin again."""
    before = scenario.money()
    drawn, pad, spent = scouted(name, count)
    run.mark(name)
    after = credited()
    want = before + count * (STRAIGHT_UP - CHIP)
    run.judge(name, after == want,
              f"pocket {drawn}, {count} chips, ${before} -> ${after}, "
              f"wanted ${want}, pad {pad}, {spent} frames of chips")
    return after


def main():
    """Play to the floor, walk north and run the rounds."""
    run.play(scenario.TAPE)
    run.mark("floor")
    reached = walk_to_table()
    opened = scenario.money()
    run.look()
    print("table", reached, "wallet", opened,
          "frames", run.frames(), flush=True)
    for number in range(ROUNDS):
        count = min(STACK_CAP, scenario.money() // CHIP)
        at = run.frames()
        after = round_at(f"round-{number + 1}", count)
        print(f"round {number + 1} {count} chips -> ${after} "
              f"in {run.frames() - at} frames", flush=True)
    run.look()
    print("done wallet", scenario.money(), "frames", run.frames(), flush=True)


if __name__ == "__main__":
    main()
