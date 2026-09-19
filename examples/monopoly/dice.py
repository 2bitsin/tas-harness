"""The register at 0x84b8: the frame clocks it, a roll draws twice from it."""

REGISTER = 0x84b8
REGISTER_WIDTH = 2
LOW_TAP = 10
HIGH_TAP = 15
MASK = 0xffff
SEED = 0x1234

FACES = 6
IDLE_FACE = 7
HUMAN_DRAWS = 2
RIVAL_DRAWS = 4

# The vblank line that clocks the register bumps 0x81a0 too (rom 0x2922).
COUNTER = 0x81a0
COUNTER_WIDTH = 4

# Measured: 65 clocks from the press to the draw, 128 from a rival reset.
SAMPLE_DELAY = 65
RIVAL_DELAY = 128
WINDOW = 36


def advance(register, times=1):
    """One clock of the shift register, as the routine at rom 0x119ba."""
    for _ in range(times):
        feedback = ((register >> LOW_TAP) ^ (register >> HIGH_TAP)) & 1
        register = ((register << 1) | feedback) & MASK
    return register


def draw(register, faces=FACES):
    return advance(register) % faces


def die(register):
    return draw(register) + 1


def steps_to_sample(offset):
    return offset + SAMPLE_DELAY


def sample_register(register, offset):
    """The register the roll's first draw reads, from a press offset away."""
    return advance(register, steps_to_sample(offset))


def roll_at(register, offset):
    """The dice a press offset frames after a state whose register is this."""
    sampled = sample_register(register, offset)
    second = die(sampled)
    first = die(advance(sampled))
    return first, second


def total_at(register, offset):
    first, second = roll_at(register, offset)
    return first + second


def window(register, span=WINDOW, offset=0):
    return [roll_at(register, offset + k) for k in range(span)]


def offsets_for(register, total, span=WINDOW, offset=0):
    return [offset + k for k, roll in enumerate(window(register, span, offset))
            if sum(roll) == total]


def register_of(run):
    return int.from_bytes(run.memory("system", REGISTER, REGISTER_WIDTH),
                          "little")


def counter_of(run):
    """The 68000 longword at 0x81a0, out of byte-swapped work ram."""
    raw = run.memory("system", COUNTER, COUNTER_WIDTH)
    return (int.from_bytes(raw[0:2], "little") << 16
            | int.from_bytes(raw[2:4], "little"))
