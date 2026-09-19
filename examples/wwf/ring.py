"""The wrestler object, the combo meter and the pad, read one frame at a
time.  Offsets are measured against Genesis Plus GX's byte-swapped work
ram, where a word reads low byte first at the even offset."""

import combos

ACTION = 0xB8
STATE = 0x64
RECORD = 0x5C
SCRIPT = 0x10C
PAD = 0x190
HOLDING = 0x10
HELD = 0x11
RECORDS = 0xB312
RECORD_STRIDE = 14
METER_IN_RECORD = 0xA
DRAWN_IN_RECORD = 0xC
DOWN = 0x9
# ROM $3ef78 loads 16 into the value the gate compares the meter with.
FULL = 16
TOWARD = 0x8
AWAY = 0x4
WORD = 2
ACTION_BLOCK = (0x8000, 0x2000)
PUNCH_FRAMES = 14
TAP_HOLD = 2
TAP_GAP = 2
OTHER = {"right": "left", "left": "right"}


class Ring:
    """Every word a per-frame decision reads, and the presses it makes."""

    def __init__(self, run, base):
        self.run = run
        self.base = base

    def word(self, address):
        raw = self.run.memory("system", address, WORD)
        return raw[0] | (raw[1] << 8)

    def long(self, address):
        return (self.word(address) << 16) | self.word(address + WORD)

    def action(self):
        return self.word(self.base + ACTION)

    def state(self):
        return self.word(self.base + STATE)

    def script(self):
        return self.long(self.base + SCRIPT)

    def code(self):
        return self.word(self.base + PAD)

    def record(self):
        return self.word(self.base + RECORD)

    def meter(self):
        return self.word(RECORDS + RECORD_STRIDE * self.record()
                         + METER_IN_RECORD)

    def drawn(self):
        return self.word(RECORDS + RECORD_STRIDE * self.record()
                         + DRAWN_IN_RECORD)

    def loaded(self):
        return self.meter() >= FULL

    def holding(self):
        return self.state() == HOLDING

    def held(self):
        return self.state() == HELD

    def down(self):
        return self.state() == DOWN

    def tap(self, pressed):
        """Press, let go, and answer every code the object read."""
        seen = 0
        self.run.hold(1, pressed)
        for _ in range(TAP_HOLD):
            self.run.step(1)
            seen |= self.code()
        self.run.release(1, pressed)
        for _ in range(TAP_GAP):
            self.run.step(1)
            seen |= self.code()
        return seen

    def toward(self):
        """Tap a direction and answer the one the object reads as forward."""
        way = combos.forward(self.run)
        if self.tap([way]) & TOWARD:
            return way, True
        return OTHER[way], False


def find(run):
    """The player's object this match, from the animation a punch plays."""
    base, size = ACTION_BLOCK
    run.checkpoint("ring-calibrate")
    standing = run.memory("system", base, size)
    run.hold(1, [combos.BUTTONS["P"]])
    run.step(TAP_HOLD)
    run.release(1)
    seen = []
    for _ in range(PUNCH_FRAMES):
        run.step(1)
        seen.append(run.memory("system", base, size))
    run.restore("ring-calibrate")
    punch = next(row[5] for row in combos.COMBOS if row[0] == "Punch")
    for offset in range(0, size - 1, WORD):
        if standing[offset] | (standing[offset + 1] << 8) != combos.STANDING:
            continue
        if any(block[offset] | (block[offset + 1] << 8) == punch
               for block in seen):
            return base + offset - ACTION
    return None
