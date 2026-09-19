"""The words Scooby-Doo Mystery keeps for the room, cursor, verb and line."""

# 0x6b4 is the object the status line names, which is not always the box
# the hit test reports at 0x6ae; 0x6b6 holds the second object of a pair.
ROOM = 0x6ac
HOVER = 0x6ae
TARGET = 0x6b4
SECOND = 0x6b6
VERB = 0x6b8
CURSOR_X = 0x6dc
CURSOR_Y = 0x6de
SHAGGY_X = 0x4dc
SHAGGY_Y = 0x4e0

# The y the doorway walk-on test divides by eight; 0x4e0 wanders (1k).
SHAGGY_ROW = 0x4f4

# One byte: 1 while the pad moves the crosshair, 0 while it moves Shaggy.
AIMING = 0xab5

# One byte, 1 while the pad is read and 0 through every scripted window:
# the walk a click starts, the scene it plays and the caption it ends on.
LIVE = 0x630

# Null terminated: the hovered object, then the verb once one is picked.
LINE = 0x8bc
LINE_MAX = 40

LOBBY = 16
HALLWAY = 9
CAFE = 13

PULL = 1
TAKE = 2
OPEN = 3
SHUT = 4
USE = 5
GIVE = 6
PUSH = 7
TALK = 8
EAT = 9
LOOK = 10

VERB_NAME = {PULL: "Pull", TAKE: "Take", OPEN: "Open", SHUT: "Shut",
             USE: "Use", GIVE: "Give", PUSH: "Push", TALK: "Talk to",
             EAT: "Eat", LOOK: "Look at"}

WORD_BYTES = 2
SIGN_BIT = 0x8000
WORD_WRAP = 0x10000


class State:
    """Every word the player reads, over one live run."""

    def __init__(self, run):
        self._run = run

    def word(self, address, signed=True):
        raw = self._run.memory("system", address, WORD_BYTES)
        value = int.from_bytes(raw, "little")
        if signed and value >= SIGN_BIT:
            return value - WORD_WRAP
        return value

    def room(self):
        return self.word(ROOM, signed=False)

    def verb(self):
        return self.word(VERB, signed=False)

    def hover(self):
        """The object the status line names, the second of a pair included."""
        return self.word(SECOND if self.pending() else TARGET, signed=False)

    def pending(self):
        return self.word(SECOND, signed=False) and self.word(TARGET,
                                                             signed=False)

    def cursor(self):
        return self.word(CURSOR_X), self.word(CURSOR_Y)

    def live(self):
        return bool(self._run.memory("system", LIVE ^ 1, 1)[0])

    def aiming(self):
        raw = self._run.memory("system", AIMING ^ 1, WORD_BYTES)
        return bool(raw[0])

    def shaggy(self):
        return self.word(SHAGGY_X), self.word(SHAGGY_Y)

    def standing(self):
        """Where the doorway test reads him: his own x and 0x4f4."""
        return self.word(SHAGGY_X), self.word(SHAGGY_ROW)

    def line(self):
        return self._run.string("system", LINE, LINE_MAX)
