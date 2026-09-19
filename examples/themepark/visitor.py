"""Step 2c: one visitor from the bus to the gate, and what turns it back."""

import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import tash
import engine
import takings

run = tash.run
run.pace(0)

# The table the entity dispatcher at ROM $482a0 walks: 204 records of $86
# bytes from 68000 $ffff5b24, slot 0 a header it never ticks.
TABLE_BASE = 0x5b24
TABLE_STRIDE = 0x86
TABLE_SLOTS = 204
FIRST_SLOT = 1

KIND = 0x04
STATE = 0x0c
TIMER = 0x14
POS_X = 0x06
POS_Y = 0x08
PHASE_COUNT = 0x3c
TARGET_X = 0x4d
TARGET_Y = 0x4e
IN_PARK = 0x7c
VISITOR_KIND = 0x02
TILE = 256.0

OPEN_BYTE = 0x1c

SHAPE = {"arm": True, "queue": takings.LONG_QUEUE}
GATE_TILE = (23, 41)
FORECOURT = "40,72,240,120"
LOOK_EVERY = 60
FOLLOW_FRAMES = 1500
SETTLE_FRAMES = 120
POLL_FRAMES = 30
POLL_ROUNDS = 50
PAD_FRAMES = 8
TAP_FRAMES = 4
MONTHS = 6
PHASE = os.environ.get("VISITOR_PHASE", "all")


def byte_at(block, offset):
    """One byte of a work-RAM block, undoing the core's byte swap."""
    return block[offset ^ 1]


def word_at(block, offset):
    """One 68000 word of a work-RAM block."""
    return (byte_at(block, offset) << 8) | byte_at(block, offset + 1)


def long_at(block, offset):
    """One 68000 longword of a work-RAM block."""
    return (word_at(block, offset) << 16) | word_at(block, offset + 2)


def whole_table():
    """Every record of the entity table in one read."""
    return run.memory("system", TABLE_BASE, TABLE_STRIDE * TABLE_SLOTS)


def record(block, slot):
    """One record out of a whole-table read."""
    start = slot * TABLE_STRIDE
    return block[start:start + TABLE_STRIDE]


def one_record(slot):
    """One record read on its own, for following a single visitor."""
    return run.memory("system", TABLE_BASE + slot * TABLE_STRIDE,
                      TABLE_STRIDE)


def position(block):
    """The record's position words, in tiles."""
    return (word_at(block, POS_X) / TILE, word_at(block, POS_Y) / TILE)


def occupied(block):
    """Every slot with a non-zero record, with its kind, state and place."""
    rows = []
    for slot in range(FIRST_SLOT, TABLE_SLOTS):
        held = record(block, slot)
        if any(held):
            where = position(held)
            rows.append((slot, byte_at(held, KIND), byte_at(held, STATE),
                         round(where[0], 2), round(where[1], 2)))
    return rows


def park_byte(offset):
    """One byte of the park record at 68000 $ffff1d7a."""
    block = run.memory("system", takings.RECORD_BASE, takings.RECORD_SIZE)
    return takings.record_byte(block, offset)


def open_the_park():
    """Start held with C tapped reaches the flip at ROM $53d72."""
    run.hold(1, ["start"])
    run.step(PAD_FRAMES)
    panel = run.look()
    run.hold(1, ["start", "a"])
    run.step(TAP_FRAMES)
    run.hold(1, ["start"])
    run.step(PAD_FRAMES)
    flipped = run.look()
    run.release(1)
    run.step(PAD_FRAMES)
    return park_byte(OPEN_BYTE), panel, flipped


def find_a_visitor():
    """Runs until a bus load is in the table, and says which slot holds it."""
    for _ in range(POLL_ROUNDS):
        block = whole_table()
        for slot in range(FIRST_SLOT, TABLE_SLOTS):
            if byte_at(record(block, slot), KIND) == VISITOR_KIND:
                return slot
        run.step(POLL_FRAMES)
    return None


def describe(slot, held, frame):
    """One row of the visitor's life: frame, state, timer, place, target."""
    where = position(held)
    return (f"frame {frame} slot {slot} state {byte_at(held, STATE):#04x}"
            f" timer {word_at(held, TIMER)}"
            f" phase {long_at(held, PHASE_COUNT)}"
            f" at {where[0]:.2f},{where[1]:.2f}"
            f" target {byte_at(held, TARGET_X)},{byte_at(held, TARGET_Y)}"
            f" in_park {byte_at(held, IN_PARK):#04x}")


def follow(label):
    """Every state one visitor takes, frame by frame, with the gate in view."""
    run.mark(label, "follow")
    takings.move_to(GATE_TILE)
    run.step(SETTLE_FRAMES)
    slot = find_a_visitor()
    if slot is None:
        print(f"{label}: no visitor in {POLL_ROUNDS * POLL_FRAMES} frames",
              flush=True)
        return
    seen = None
    states = {}
    for frame in range(FOLLOW_FRAMES):
        held = one_record(slot)
        state = byte_at(held, STATE)
        states[state] = states.get(state, 0) + 1
        if state != seen:
            seen = state
            print(f"{label} {describe(slot, held, frame)}", flush=True)
        if frame % LOOK_EVERY == 0:
            print(f"{label} frame {frame} {run.look(region=FORECOURT)}",
                  flush=True)
        run.step(1)
    print(f"{label} states {sorted(states.items())}"
          f" counters {takings.counters()}", flush=True)


def the_record():
    """The table before the bus and once the load is in the forecourt."""
    run.restore("built")
    run.mark("before the bus", "record")
    print(f"before: {occupied(whole_table())}", flush=True)
    find_a_visitor()
    run.step(SETTLE_FRAMES)
    run.mark("the load is off", "record")
    print(f"after: {occupied(whole_table())}", flush=True)


def the_shut_park():
    """The park as the game hands it over: the visitor never reaches $48cc8."""
    run.restore("built")
    print(f"shut: open byte {park_byte(OPEN_BYTE)}"
          f" fields {takings.record_fields()}", flush=True)
    follow("shut")


def the_open_park():
    """Start and C open the park, and the turnstile decision runs."""
    run.restore("built")
    flag, panel, flipped = open_the_park()
    print(f"open: open byte {flag} panel {panel} flipped {flipped}"
          f" fields {takings.record_fields()}", flush=True)
    run.checkpoint("open")
    follow("open")


def the_money():
    """Months of the open park at the fare the game hands over."""
    run.restore("open")
    fields = takings.record_fields()
    print(f"money: fare {fields['fare']} gate {fields['gate']} rating "
          f"{takings.rating_of(fields, fields['fare'])}", flush=True)
    ends = takings.run_months(MONTHS)
    print(f"money: ends {ends}", flush=True)
    print(f"money: bank {takings.open_the_bank()}", flush=True)
    engine.close_bank()


PHASES = {"record": the_record, "shut": the_shut_park,
          "open": the_open_park, "money": the_money}
ORDER = ["record", "shut", "open", "money"]


def main():
    run.mark("tape", "setup")
    run.play(engine.TAPE)
    run.checkpoint("park")
    built = takings.build_shape(SHAPE)
    run.checkpoint("built")
    print(f"built {built}", flush=True)
    for name in (ORDER if PHASE == "all" else [PHASE]):
        PHASES[name]()


if __name__ == "__main__":
    main()
