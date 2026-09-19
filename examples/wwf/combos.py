"""Bret Hart's moves as a table, and the code that plays one of them."""

import pathlib

HERE = pathlib.Path(__file__).resolve().parent
TAPE = HERE / "tapes" / "title-to-first-bell.yaml"
HARDEST_TAPE = HERE / "tapes" / "title-to-first-bell-hardest.yaml"

BUTTONS = {"P": "l", "RUN": "x", "SP": "r", "K": "y", "BLK": "b", "SK": "a"}

# The sprites touch at 70 and a bare punch reaches to 90; inside 70 the player
# walks through the opponent and everything whiffs.
TOUCH = 70
REACH = 90
WALK_TIMEOUT = 600
HIT_TIMEOUT = 40
ONE_HIT = 20
# A bare punch is 12 at VERY HARD where it is 20 at MEDIUM, so a context is
# checked for contact, not for a full hit; a blocked hit chips 1.
CONTACT = 8
TAIL_FRAMES = 80
# Two hits further apart than this are two exchanges, not one combo.
LINK_FRAMES = 30
# README.md, "The action id": the player's record while he stands, and the
# frames an id must hold to be the move rather than a step into it.
STANDING = 0x2AEE
MOVE_FRAMES = 6
HELD_FRAMES = 180

# name, inputs as (symbol, hold, gap) frames, frames to the first hit, damage,
# linked hits, action id. F and B read forward from cpu_x - p1_x; every number
# is from README.md, "Bret Hart's published list, move by move".
COMBOS = (
    ("Punch", (("P", 2, 0),), 9, 12, 1, 0x3268),
    ("Kick", (("K", 2, 0),), 10, 21, 1, 0x3D1E),
    ("Super Punch", (("SP", 2, 0),), -1, 0, 0, 0x378C),
    ("Super Kick", (("SK", 2, 0),), -1, 0, 0, 0x4062),
    ("Rolling Uppercut", (("D", 2, 3), ("DF", 2, 3), ("F+SP", 2, 0)),
     20, 32, 1, 0x7DE2),
    ("Eye Rake", (("D", 2, 2), ("DF", 2, 2), ("F+P", 2, 0)),
     43, 26, 1, 0x7A3A),
    ("Quick Uppercut", (("D", 2, 3), ("D+P", 2, 0)), 22, 32, 1, 0x32D8),
    ("Super Flying Kick", (("B", 2, 6), ("B", 2, 6), ("SK", 2, 0)),
     32, 45, 1, 0x3FA4),
    ("Arm Drag", (("B", 2, 2), ("B", 2, 2), ("P", 2, 0)), 60, 22, 1, 0x5BAE),
    ("DDT", (("RUN+F", 20, 0), ("RUN+SP", 2, 0)), 57, 33, 1, 0x6C84),
    ("Head grab", (("F", 2, 2), ("F+SP", 2, 0)), -1, 0, 0, 0x649E),
    ("Head Slam", (("F", 2, 4), ("F+SP", 2, 32), ("P", 2, 0)),
     47, 20, 1, 0x4748),
    ("Slam", (("F", 2, 4), ("F+SP", 2, 32), ("F+P", 2, 0)),
     53, 12, 1, 0x4A22),
    ("Uppercut in the hold",
     (("F", 2, 4), ("F+SP", 2, 32), ("D+SP", 2, 0)), 50, 16, 1, 0x3428),
    ("Face Slam",
     (("F", 2, 4), ("F+SP", 2, 38), ("D", 2, 4), ("DF", 2, 4), ("F+P", 2, 4),
      ("P", 2, 4), ("P", 2, 4), ("P", 2, 0)), 85, 29, 1, 0x74D4),
    ("post-grab string",
     (("F", 2, 4), ("F+SP", 2, 20)) + (("F+P", 2, 4),) * 5 + (("SK", 2, 0),),
     53, 17, 2, 0x4A22),
    ("combo string",
     (("F", 2, 4), ("F+SP", 2, 26)) + (("P", 2, 4), ("K", 2, 4)) * 8,
     47, 34, 1, 0x4748),
    ("grab string", (("F+P", 2, 7),) * 6, 9, 22, 3, 0x3268),
    ("punch string",
     (("P", 2, 4), ("P", 2, 4), ("K", 2, 4), ("SK", 2, 0)), 9, 24, 2, 0x3268),
)


def gap(run):
    """How far the opponent is to the right of the player."""
    return run.watch("cpu_x") - run.watch("p1_x")


def forward(run):
    """The pad direction that points at the opponent right now."""
    return "right" if gap(run) > 0 else "left"


def back(run):
    """The pad direction that points away from the opponent right now."""
    return "left" if forward(run) == "right" else "right"


def pad(run, symbol):
    """The libretro buttons one symbol of a string presses."""
    pressed = []
    for name in symbol.split("+"):
        if name == "-":
            continue
        if name in BUTTONS:
            pressed.append(BUTTONS[name])
        elif name == "F":
            pressed.append(forward(run))
        elif name == "B":
            pressed.append(back(run))
        elif name == "DF":
            pressed.extend(["down", forward(run)])
        else:
            pressed.append("down" if name == "D" else "up")
    return pressed


class Landed:
    """What one move did: its action ids, the frames it hit on, the damage."""

    def __init__(self, run):
        self.run = run
        self.health = run.watch("cpu_health")
        self.frame = 0
        self.damage = 0
        self.at = []
        self.runs = [[run.watch("p1_action"), 0, 0]]
        self.meter = run.watch("p1_meter")
        self.mine = run.watch("p1_health")
        self.hurt = None

    def step(self, frames):
        for _ in range(frames):
            self.run.step(1)
            self.frame += 1
            action = self.run.watch("p1_action")
            if self.runs[-1][0] == action:
                self.runs[-1][1] += 1
            else:
                self.runs.append([action, 1, self.frame])
            mine = self.run.watch("p1_health")
            if mine < self.mine and self.hurt is None:
                self.hurt = self.frame
            self.mine = mine
            self.meter = max(self.meter, self.run.watch("p1_meter"))
            now = self.run.watch("cpu_health")
            if now < self.health:
                if self.at and self.frame - self.at[-1] <= 1:
                    self.at[-1] = self.frame
                else:
                    self.at.append(self.frame)
                self.damage += self.health - now
            self.health = now

    def hits(self):
        """The longest run of hits no further apart than a link allows."""
        longest = chain = 1 if self.at else 0
        for earlier, later in zip(self.at, self.at[1:]):
            chain = chain + 1 if later - earlier <= LINK_FRAMES else 1
            longest = max(longest, chain)
        return longest

    def first(self):
        return self.at[0] if self.at else -1

    def action(self, neutral=(), frames=MOVE_FRAMES):
        """The first id the move settles on, before any blow it takes."""
        for value, length, start in self.runs:
            if self.hurt is not None and start >= self.hurt:
                return None
            if (length >= frames and value != STANDING
                    and value not in neutral):
                return value
        return None

    def seen(self):
        """Every id the move held long enough to be a state of its own."""
        return {value for value, length, _ in self.runs
                if length >= MOVE_FRAMES}


def idles(run, name, frames=TAIL_FRAMES):
    """The action ids the player passes through doing nothing from here."""
    run.checkpoint(name)
    landed = Landed(run)
    landed.step(frames)
    run.restore(name)
    return landed.seen()


def connects(run, name):
    """Whether a bare punch touches from here, leaving the run where it was."""
    run.checkpoint(name)
    before = run.watch("cpu_health")
    run.hold(1, ["l"])
    run.step(2)
    run.release(1, ["l"])
    damage = 0
    for _ in range(HIT_TIMEOUT):
        run.step(1)
        if run.watch("cpu_health") != before:
            damage = before - run.watch("cpu_health")
            break
    run.restore(name)
    return damage >= CONTACT


def approach(run, name="approach"):
    """Hold the gap at point blank until a bare punch lands from a frame."""
    for _ in range(WALK_TIMEOUT):
        away = abs(gap(run))
        if TOUCH <= away <= REACH:
            if connects(run, name):
                return True
            run.step(1)
            continue
        step = forward(run) if away > REACH else back(run)
        run.hold(1, [step])
        run.step(1)
        run.release(1)
    return False


def perform(run, combo):
    """Play one row of the table from here and answer what it landed."""
    landed = Landed(run)
    for symbol, hold, gap_frames in combo[1]:
        pressed = pad(run, symbol)
        run.hold(1, pressed)
        landed.step(hold)
        run.release(1, pressed)
        landed.step(gap_frames)
    run.release(1)
    landed.step(TAIL_FRAMES)
    return landed
