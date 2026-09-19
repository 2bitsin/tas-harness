"""A path across a room's floor: press-walks and the turning point between."""

import state
import tash
import world

# Measured: a held direction walks one step and stops, so a walk is a run
# of presses and the turning points are the stops that run makes.
PRESS = 24
GAP = 8
TICK = 2
STOPS = 20
LEG = 14
LANE = 32
REST_STILL = 16
REST_LIMIT = 900
WAYS = ("left", "right", "up", "down")
ACROSS = {"left": ("down", "up"), "right": ("down", "up"),
          "up": ("right", "left"), "down": ("right", "left")}
TAKEN = 1 << 20
SEED = "floor-from"
TURNED = "turned"


class Floor:
    """Walks Shaggy across a room's floor by presses, to take a doorway."""

    def __init__(self, run, cursor):
        self._run = run
        self._cursor = cursor
        self._state = state.State(run)
        self._paths = {}
        self._trials = 0
        self._frames = 0

    def frame(self):
        return self._run.observe()["frame"]

    def at(self):
        """Shaggy's own x word, the only position word that holds still."""
        return self._state.shaggy()[0]

    def rest(self, limit=REST_LIMIT):
        """Step until Shaggy's x word is still and the pad is read again."""
        was, still = self.at(), 0
        for _ in range(limit // TICK):
            self._run.step(TICK)
            now = self.at()
            still = still + 1 if now == was else 0
            was = now
            if still > REST_STILL and self._state.live():
                break
        return was

    def span(self, word):
        """The doorway box's world x, the unit Shaggy's own word is in."""
        box = self._cursor.box(word)
        if box is None:
            return None
        return (box[0] * world.CELL, (box[0] + box[2]) * world.CELL)

    def row(self):
        """The y the doorway test reads, which his own walk keeps."""
        return self._state.standing()[1]

    def outside(self, cell, start, span):
        """How far past a box a cell stands: over is positive, under not."""
        if cell < start:
            return cell - start
        return max(cell - (start + span - 1), 0)

    def apart(self, word):
        """How far outside the box he stands, in cells, on each axis."""
        box = self._cursor.box(word)
        if box is None:
            return None
        return (self.outside(self.at() // world.CELL, box[0], box[2]),
                self.outside(self.row() // world.CELL, box[1], box[3]))

    def gap(self, word):
        """How far Shaggy stands from the box the crossing is read on."""
        off = self.apart(word)
        if off is None:
            return 0
        return (abs(off[0]) + abs(off[1])) * world.CELL

    def onto(self, word):
        """The way that closes the box's larger gap, measured step 1k."""
        off = self.apart(word)
        if off is None or not any(off):
            return None
        wide, tall = off
        if abs(tall) > abs(wide):
            return "up" if tall > 0 else "down"
        return "left" if wide > 0 else "right"

    def press(self, way, was):
        """One press of a direction: one step of the game's own walk."""
        self._run.hold(1, [way])
        for _ in range(PRESS // TICK):
            self._run.step(TICK)
            if self._state.room() != was:
                break
        self._run.release(1)
        if self._state.room() == was:
            self._run.step(GAP)
        return self._state.room()

    def march(self, way, many):
        """Press a way that many times, or until the room turns."""
        was = self._state.room()
        self._cursor.lower_cursor()
        for _ in range(many):
            if self.press(way, was) != was:
                return TURNED
        return "spent"

    def hold_for(self, here, there):
        """The direction a measured edge between two rooms is crossed on."""
        rooms = {name: one["word"] for name, one in world.ROOMS.items()}
        for one in world.EDGES:
            if (rooms.get(one["room"]) == here
                    and rooms.get(one["goes"]) == there):
                return one["hold"]
        return None

    def ways(self, here, there, word=None):
        """Every way to try, the box's own direction going first."""
        one = self.onto(word) if word else None
        if one is None:
            one = self.hold_for(here, there)
        if one is None:
            return WAYS
        return (one,) + tuple(way for way in WAYS if way != one)

    def take(self, path, want):
        """Walk the turning point's own leg, then press the way across."""
        first, many, way = path
        if first is not None and many:
            if self.march(first, many) == TURNED:
                return self._state.room() == want
        return self.march(way, LEG) == TURNED

    def worth(self, was, want, word):
        """What a try is worth: the crossing taken, or the x gap left."""
        here = self._state.room()
        if here == want:
            return TAKEN
        if here != was:
            return -TAKEN
        return -self.gap(word)

    def count(self, got):
        self._trials += got.trials
        self._frames += got.frames
        return got

    def sift(self, paths, was, want, word):
        """Search the turning points, scoring each by the room it reaches."""
        def apply(run, one):
            self.take(one, want)

        def score(run):
            return self.worth(was, want, word)

        got = self.count(tash.search(list(paths), apply, score))
        if got.best is not None and got.score >= TAKEN:
            return got.best
        return None

    def hunt(self, word, want, was):
        """First the ways from where he stands, then a turning point too."""
        ways = self.ways(was, want, word)
        rounds = [[(None, 0, way) for way in ways]]
        for way in ways:
            rounds.append([(first, many, way) for first in ACROSS[way]
                           for many in range(1, STOPS)])
        for paths in rounds:
            found = self.sift(paths, was, want, word)
            if found is not None:
                return found
        return None

    def replay(self, path, want):
        """Walk a remembered path again: one trial, and no search."""
        self._trials += 1
        base = self.frame()
        self.take(path, want)
        self._frames += self.frame() - base
        return self._state.room() == want

    def cross(self, word, want):
        """Take a walk-on crossing by the path this arrival needs."""
        base, was = self.frame(), self._state.room()
        self._cursor.wait_for_play()
        self.rest()
        key = (was, self.at() // LANE, word)
        known, found = self._paths.get(key), None
        self._run.checkpoint(SEED)
        if known is not None:
            found = known if self.replay(known, want) else None
            if found is None:
                self._run.restore(SEED)
        if found is None:
            found = self.hunt(word, want, was)
            if found is None:
                self._run.restore(SEED)
            else:
                self.take(found, want)
                self._paths[key] = found
        self._run.forget(SEED)
        return {"done": self._state.room() == want, "way": found,
                "room": self._state.room(), "frames": self.frame() - base}

    def tally(self):
        """The trials and the frames every crossing search has spent."""
        return {"trials": self._trials, "frames": self._frames,
                "paths": dict(self._paths)}
