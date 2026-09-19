"""The crosshair, the verb bar and the caption, at the frames they measure."""

import cartridge
import floor
import script
import state
import world

# Genesis A is retro y and Genesis B is retro b; the core's pad has no c,
# which is retro a.
A = "y"
B = "b"
C = "a"

# The crosshair walks two pixels a frame under a held direction, flat, and
# settles one pixel further on the release.
PIXELS_A_FRAME = 2
SETTLE = 1

SCREEN_WIDTH = 256
SCREEN_HEIGHT = 224
SCENE_TOP = 20
SCENE_FLOOR = 160
SCENE_BOX = (0, 24, 244, 152)
BAR_ROW_TOP = 180
BAR_ROW_BOTTOM = 196

# The bar snaps the crosshair to a cell forty logical pixels wide, so a
# column is aimed at from its own centre.
BAR_CELL = 40
BAR_COLUMN = (28, 68, 108, 148, 188)
BAR_TOP_ROW = 180
BAR_LOW_ROW = 196
BAR_VERB = {state.TAKE: (0, 0), state.LOOK: (1, 0), state.OPEN: (2, 0),
            state.PUSH: (3, 0), state.EAT: (4, 0), state.GIVE: (0, 1),
            state.TALK: (1, 1), state.SHUT: (2, 1), state.PULL: (3, 1),
            state.USE: (4, 1)}

CLICK_FRAMES = 6
VERB_TRIES = 3
BAR_TRIES = 6
BAR_NUDGE = 6
PANEL_SWAPS = 3
BAR_DROPS = 4
BAR_SNAP = 4
PANEL_WALK = (("left",) * 4 + ("right",) * 4 + ("down",)
              + ("left",) * 4 + ("right",) * 4 + ("up",))
PANEL_TURNS = 8
CLICK_AFTER = 20
LIFT_FRAMES = 60
PROBE_FRAMES = 2
MOVE_TRIES = 4
MODE_TRIES = 3
MODE_PROBE = 8
WALK_LIMIT = 1200
PLACE_NEAR = 6
SLIDE_COST = 72
RAISE_TRIES = 8
CLAMP_X = 247
CLICK_TRIES = 3
AIM_RADIUS = 6
AIM_STEP = 2
SURVEY_STEP = 4
SWEEP_ROWS = (76, 60, 92, 44, 108, 124, 140, 28)
REACH_SPAN = 80
REACH_WALKS = 5
SETTLE_FRAMES = 4
LINE_LAG = 12
NAME_TRIES = 6

# Where the crosshair is parked to press b without clicking anything: b over
# a hotspot is that hotspot's action, not the mode toggle.
EMPTY_SPOT = ((4, 24), (128, 24), (244, 24), (4, 140), (244, 140))

# The verb bar's own blue fills the bottom strip whenever the player has the
# room: 1,587 pixels of it in every room measured, none in a cutscene.
BAR_STRIP = "0,170,256,42"
BAR_RGB = (96, 100, 232)
BAR_LEAST = 800

# A caption is green over black across the top two rows; 638 pixels of it on
# the line measured, and none once it clears.
CAPTION_STRIP = "0,0,256,20"
CAPTION_RGB = (32, 100, 0)
CAPTION_LEAST = 40

# A caption answers a press edge, never a hold: mashed one frame down and one
# up it goes in 36 frames from the click, against 372 untouched.
MASH_DOWN = 1
MASH_UP = 1
CAPTION_DRAW = 80
CAPTION_WAIT = 200
SCENE_ONSET = 90
ACTION_LIMIT = 1200
REST_TICKS = 44
STEADY_TICKS = 12
CAPTION_LIMIT = 600
ROOM_LIMIT = 900
# The dumb waiter's ride turns the room word 750 frames after the
# click, the longest acted crossing the hotel has.
CROSS_LIMIT = 1500
# The dumb waiter takes two: the first Use calls Scooby over, the
# second rides.
CROSS_TRIES = 2
CUTSCENE_LIMIT = 20000
# The hotel's reaction scenes answer b in 20 frames and ignore Start,
# which costs them the 216 frames they take untouched; the arrival cutscene
# is the other way about and the tape mashes Start at it.
CROSS_LEFT = 8
STILL_TICKS = 8
BY_CENTRE = "centre"
BY_SWEEP = "sweep"
BY_MISS = "miss"
PAIR_SETTLE = 20
JOIN = {state.USE: "with", state.GIVE: "to"}
LIVE_AGAIN = "watch live equal 1"
SKIP_BUTTONS = (B, A)
SKIP_DOWN = 2
SKIP_UP = 2
SKIP_PROBE = 60

# The inventory shares the bar's two rows and five columns; `a` swaps the
# bar between the ten verbs and the panel.
PANEL_ROW = (BAR_TOP_ROW, BAR_LOW_ROW)
PANEL_CELL = (BAR_COLUMN[0], BAR_TOP_ROW)


class Cursor:
    """The crosshair, and every press the scene answers."""

    def __init__(self, run):
        self._run = run
        self._state = state.State(run)
        self._cart = cartridge.Cartridge(run)
        self._script = script.Script(run)
        self._floor = floor.Floor(run, self)
        self._looks = []

    def frame(self):
        return self._run.observe()["frame"]

    def bar(self):
        return self._run.colours(BAR_STRIP, BAR_RGB, 8)

    def caption(self):
        return self._run.colours(CAPTION_STRIP, CAPTION_RGB, 8)

    def playing(self):
        return self.bar() >= BAR_LEAST

    def tap(self, button, frames=CLICK_FRAMES, after=CLICK_AFTER):
        self._run.hold(1, [button])
        self._run.step(frames)
        self._run.release(1)
        self._run.step(after)

    def raised(self):
        """Answer the byte that says the pad aims instead of walking."""
        return self._state.aiming()

    def empty(self, spots=EMPTY_SPOT):
        """Rest the crosshair where no hotspot answers, so b is not a click."""
        if not self._state.hover():
            return self._state.cursor()
        for spot in spots:
            self.move(*spot)
            self._run.step(SETTLE_FRAMES)
            if not self._state.hover():
                return self._state.cursor()
        return None

    def confirmed(self):
        """Hold a direction and back: the crosshair moves, he does not."""
        was, stood = self._state.cursor(), self._state.shaggy()[0]
        away, back = ("left", "right") if was[0] > CLAMP_X // 2 \
            else ("right", "left")
        self._run.hold(1, [away])
        self._run.step(MODE_PROBE)
        self._run.release(1)
        self._run.step(1)
        moved = self._state.cursor() != was
        walked = self._state.shaggy()[0] != stood
        self._run.hold(1, [back])
        self._run.step(MODE_PROBE)
        self._run.release(1)
        self._run.step(1)
        return moved and not walked

    def raise_cursor(self, tries=MODE_TRIES):
        """Genesis A toggles the crosshair; a held frame proves it."""
        for _ in range(tries):
            if self.confirmed():
                return True
            self.tap(A)
        return self.confirmed()

    def lower_cursor(self, tries=MODE_TRIES):
        """The same toggle the other way, so a held direction walks Shaggy."""
        for _ in range(tries):
            if not self.confirmed():
                return True
            self.tap(A)
        return not self.confirmed()

    def lift(self, limit=LIFT_FRAMES):
        """Walk the crosshair out of the bar, where it snaps to cells."""
        self._run.hold(1, ["up"])
        was = None
        for _ in range(limit):
            self._run.step(1)
            at = self._state.cursor()[1]
            if at <= SCENE_FLOOR or at == was:
                break
            was = at
        self._run.release(1)
        self._run.step(1)
        return self._state.cursor()

    def move(self, x, y, tries=MOVE_TRIES):
        """Walk the crosshair at the measured two pixels a frame."""
        self.raise_cursor()
        if self._state.cursor()[1] > SCENE_FLOOR and y <= SCENE_FLOOR:
            self.lift()
        for _ in range(tries):
            at = self._state.cursor()
            across, down = x - at[0], y - at[1]
            if not across and not down:
                break
            held = []
            if across:
                held.append("right" if across > 0 else "left")
            if down:
                held.append("down" if down > 0 else "up")
            steps = [abs(across) // PIXELS_A_FRAME,
                     abs(down) // PIXELS_A_FRAME]
            both = min(steps) if len(held) == 2 else 0
            if both:
                self._run.hold(1, held)
                self._run.step(both)
                self._run.release(1)
            for axis, name in enumerate(held):
                rest = steps[axis] - both
                if rest > 0:
                    self._run.hold(1, [name])
                    self._run.step(rest)
                    self._run.release(1)
            self._run.step(1)
        return self._state.cursor()

    def enter_bar(self, tries=BAR_DROPS, limit=LIFT_FRAMES):
        """Drop the crosshair into the bar, where it snaps to the cells."""
        self.raise_cursor()
        for _ in range(tries):
            if self._state.cursor()[1] >= BAR_TOP_ROW - BAR_SNAP:
                break
            self._run.hold(1, ["down"])
            was, still = None, 0
            for _ in range(limit):
                self._run.step(PROBE_FRAMES)
                at = self._state.cursor()[1]
                still = still + 1 if at == was else 0
                was = at
                if at >= BAR_TOP_ROW - BAR_SNAP or still > STILL_TICKS:
                    break
            self._run.release(1)
            self._run.step(1)
        return self._state.cursor()

    def nudge(self, way, frames=BAR_NUDGE):
        """One cell of bar travel; inside the bar a hold snaps, not walks."""
        self._run.hold(1, [way])
        self._run.step(frames)
        self._run.release(1)
        self._run.step(1)
        return self._state.cursor()

    def bar_cell(self):
        """Which bar cell the crosshair snapped to, as column and row."""
        at = self._state.cursor()
        column = (at[0] - BAR_COLUMN[0] + BAR_CELL // 2) // BAR_CELL
        row = 0 if at[1] < (BAR_TOP_ROW + BAR_LOW_ROW) // 2 else 1
        return (min(max(column, 0), len(BAR_COLUMN) - 1), row)

    def pick_verb(self, verb, tries=BAR_TRIES):
        """Walk the bar cell by cell, then click until the word says it."""
        for _ in range(PANEL_SWAPS):
            if self.verb_cell(verb, tries) == verb:
                break
            self.tap(C)
        return self._state.verb()

    def verb_cell(self, verb, tries=BAR_TRIES):
        """One attempt at the bar, whichever face of it is showing."""
        column, row = BAR_VERB[verb]
        self.enter_bar()
        for _ in range(tries * len(BAR_COLUMN)):
            now = self.bar_cell()
            if now == (column, row):
                break
            if now[0] != column:
                self.nudge("right" if column > now[0] else "left")
            else:
                self.nudge("down" if row > now[1] else "up")
            if self._state.cursor()[1] <= SCENE_FLOOR:
                self.enter_bar()
        for _ in range(tries):
            self.tap(B)
            if self._state.verb() == verb:
                break
        return self._state.verb()

    def rest_on(self, wanted, box, step=2):
        """Raster the crosshair until the status line reads that, or None."""
        left, top, right, bottom = box
        for y in range(top, bottom + 1, step):
            for x in range(left, right + 1, step):
                self.move(x, y)
                if self._state.line() == wanted:
                    return self._state.cursor()
        return None

    def rest_on_any(self, prefix, box, step=2):
        """Raster until the status line starts with that; answer both."""
        left, top, right, bottom = box
        for y in range(top, bottom + 1, step):
            self.move(left, y)
            self._run.hold(1, ["right"])
            was = -1
            while True:
                self._run.step(1)
                x = self._state.cursor()[0]
                if x >= right or x == was:
                    break
                was = x
                if self._state.line().startswith(prefix):
                    self._run.release(1)
                    self._run.step(1)
                    return self._state.line(), self._state.cursor()
            self._run.release(1)
            self._run.step(1)
        return None, None

    def sweep_row(self, y, box=SCENE_BOX):
        """Hold right across one row; answer every hovered word's samples."""
        left, _, right, _ = box
        self.move(left, y)
        if not self.raised():
            return {}
        self._run.hold(1, ["right"])
        found = {}
        was = -1
        while True:
            self._run.step(1)
            at = self._state.cursor()
            if at[0] == was or at[0] > right:
                break
            was = at[0]
            hover = self._state.hover()
            if hover:
                found.setdefault(hover, []).append(at)
        self._run.release(1)
        self._run.step(1)
        return found

    def record(self, word):
        """One object's live record, unswapped out of the byte-swapped ram."""
        at = world.OBJECTS + (word - world.OBJECT_FIRST) * world.OBJECT_BYTES
        raw = bytes(self._run.memory("system", at, world.OBJECT_BYTES))
        return bytes(raw[i ^ 1] for i in range(len(raw)))

    def field(self, word, at, width=2):
        rec = self.record(word)
        return int.from_bytes(rec[at:at + width], "big")

    def actor(self, word):
        return bool(self.field(word, world.OBJECT_FLAGS, 1)
                    & world.ACTOR_BIT)

    def fact(self, name):
        """Whether a world fact holds now, read off the object array."""
        one = world.FACT_WORDS.get(name)
        if one is None or one["word"] is world.UNKNOWN:
            return None
        inside = one["at"] - self.at(one["word"])
        width = 1 if inside == world.OBJECT_VERB else state.WORD_BYTES
        return self.field(one["word"], inside, width) == one["value"]

    def at(self, word):
        """Where an object's record starts in the array."""
        return world.OBJECTS + (word - world.OBJECT_FIRST) * world.OBJECT_BYTES

    def here(self, word):
        return self.field(word, world.OBJECT_ROOM) == self._state.room()

    def default_verb(self, word):
        return self.field(word, world.OBJECT_VERB, 1)

    def box(self, word):
        """The hotspot's box in cells, straight out of the cartridge."""
        if self.actor(word) or not self.here(word):
            return None
        return self._cart.box(self.field(word,
                                         world.OBJECT_BOX))

    def placed(self):
        """Every object's room off the array; 0 is nowhere and 1 is carried."""
        count = self._state.word(world.OBJECT_COUNT, signed=False)
        span = (count + 1) * world.OBJECT_BYTES
        raw = bytes(self._run.memory("system", world.OBJECTS, span))
        raw = bytes(raw[i ^ 1] for i in range(len(raw)))
        at = world.OBJECT_ROOM
        return {world.OBJECT_FIRST + index:
                int.from_bytes(raw[index * world.OBJECT_BYTES + at:][:2],
                               "big")
                for index in range(count + 1)}

    def present(self):
        """Every object the cartridge has placed in this room, word to name."""
        room = self._state.room()
        return {word: self._cart.name(word)
                for word, where in self.placed().items() if where == room}

    def carried(self):
        """Every object in the inventory, which is the room word one."""
        return {word: self._cart.name(word)
                for word, where in self.placed().items()
                if where == world.CARRIED}

    def whereis(self, name):
        """Which rooms hold an object of that name, nowhere aside."""
        return sorted({where for word, where in self.placed().items()
                       if self._cart.name(word) == name
                       and where != world.NOWHERE})

    def named(self, name):
        """Every word in this room the cartridge calls by that name."""
        return sorted(word for word, called in self.present().items()
                      if called == name)

    def name(self, word):
        """What the cartridge calls that object."""
        return self._cart.name(word)

    def plain(self, name):
        """A name with its case and spacing dropped, for loose matching."""
        return "".join(c for c in (name or "").lower() if c.isalnum())

    def like(self, name):
        """Words in this room the guide's name reaches, exactly or loosely."""
        want = self.plain(name)
        if not want:
            return []
        found = self.named(name)
        if found:
            return found
        return sorted(word for word, called in self.present().items()
                      if self.plain(called) and
                      (want in self.plain(called) or self.plain(called)
                       in want))

    def providers(self, word):
        """Every object and verb whose script puts that object in a room."""
        return self._script.givers().get(word, ())

    def wordsof(self, name):
        """Every object word the cartridge gives that name, anywhere."""
        want = self.plain(name)
        return tuple(word for word, called in self._cart.names().items()
                     if self.plain(called) == want)

    def instead(self, word, verb):
        """A pair keeps its verb: the block is the first object's, not this."""
        if verb in JOIN or self.handles(word, verb):
            return verb
        skip = (state.LOOK, world.WALK_ON) + tuple(JOIN)
        other = [one for one, _ in self._script.verbs(word)
                 if one not in skip]
        return other[0] if len(set(other)) == 1 else verb

    def camera(self):
        return (self._state.word(world.CAMERA_X, signed=False),
                self._state.word(world.CAMERA_Y, signed=False))

    def onscreen(self, cell):
        """Where one box cell sits on screen, the camera and cross undone."""
        across, down = self.camera()
        middle = world.CELL // 2
        x = cell[0] * world.CELL + middle - world.CROSS_X - across
        y = cell[1] * world.CELL + middle + world.CROSS_X - down
        if not -CROSS_LEFT <= x <= CLAMP_X or not SCENE_TOP <= y \
                <= SCENE_FLOOR:
            return None
        return (x & ~1, y)

    def cells(self, box):
        """The eight-pixel cells one box covers."""
        return {(x, y) for x in range(box[0], box[0] + box[2])
                for y in range(box[1], box[1] + box[3])}

    def hiders(self, word):
        """Every higher word standing in the room, with its own box."""
        return {other: self.box(other) for other in self.present()
                if other > word and self.box(other) is not None}

    def spread(self, word, box):
        """The cells of a box no higher word standing in the room covers."""
        free = self.cells(box)
        for one in self.hiders(word).values():
            free -= self.cells(one)
        return tuple(sorted(free))

    def buried(self, word):
        """The higher words in the room that leave a box no free cell."""
        box = self.box(word)
        if box is None or self.spread(word, box):
            return ()
        return tuple(one for one, other in sorted(self.hiders(word).items())
                     if self.cells(other) & self.cells(box))

    def surveyed(self, hover):
        """The free cell nearest the box's middle, where the camera has it."""
        box = self.box(hover)
        if box is None:
            return None
        middle = (box[0] + box[2] // 2, box[1] + box[3] // 2)
        for cell in sorted(self.spread(hover, box),
                           key=lambda one: (one[0] - middle[0]) ** 2
                           + (one[1] - middle[1]) ** 2):
            at = self.onscreen(cell)
            if at is not None:
                return at
        return None

    def aimed(self, word):
        """The cartridge's own centre, counted as the lookup a click made."""
        at = self.surveyed(word)
        if at is not None:
            self.noted(word, self._state.room(), BY_CENTRE, "")
        return at

    def confirm_spot(self, hover, at, lag=LINE_LAG):
        """Put the crosshair where the cartridge says and read what answers."""
        self.move(*at)
        self._run.step(lag)
        return self._state.hover() == hover

    def bring(self, word):
        """Walk Shaggy until the camera has that box on the screen."""
        if self.box(word) is None or self.surveyed(word) is not None:
            return self.surveyed(word)
        self.approach(word)
        return self.surveyed(word)

    def looks(self):
        """Every lookup: how it was found and why the centre did not hold."""
        return tuple(self._looks)

    def tally(self):
        """How many lookups the cartridge centre answered, and how many not."""
        out = {}
        for one in self._looks:
            out[one["how"]] = out.get(one["how"], 0) + 1
        return out

    def missed(self, known):
        """Why the cartridge centre did not answer with the word wanted."""
        if known is None:
            return "off screen"
        read = self._state.hover()
        return "nothing there" if not read else "reads %d" % read

    def noted(self, hover, room, how, why):
        self._looks.append({"word": hover, "room": room, "how": how,
                            "why": why, "at": self.frame()})

    def locate(self, hover, rows=SWEEP_ROWS):
        """Read the free cell off the cartridge; raster when that fails."""
        room = self._state.room()
        under = self.buried(hover)
        if under:
            self.noted(hover, room, BY_MISS, "under %s" % (under,))
            return None
        known = self.surveyed(hover) or self.bring(hover)
        if known is not None and self.confirm_spot(hover, known):
            self.noted(hover, room, BY_CENTRE, "")
            return known
        why = self.missed(known)
        self.raise_cursor()
        for y in ([known[1]] if known else []) + list(rows):
            found = self.sweep_row(y)
            if hover in found:
                xs = [x for x, _ in found[hover]]
                self.noted(hover, room, BY_SWEEP, why)
                return (sum(xs) // len(xs), y)
        self.noted(hover, room, BY_MISS, why)
        return None

    def reach(self, hover, rows=SWEEP_ROWS, span=REACH_SPAN,
              walks=REACH_WALKS):
        """Find a hotspot, walking Shaggy on when the room scrolled it off."""
        at = self.locate(hover, rows)
        if at:
            return at
        home = self._state.room()
        for way in ("right", "left"):
            back = "left" if way == "right" else "right"
            for _ in range(walks):
                here = self._state.shaggy()[0]
                self.place(here + (span if way == "right" else -span), way)
                if self._state.room() != home:
                    self.retreat(home, back)
                    break
                if self._state.shaggy()[0] == here:
                    break
                at = self.locate(hover, rows[:1])
                if at:
                    return at
        return None

    def retreat(self, room, edge, limit=ROOM_LIMIT):
        """Press back through an edge just crossed till the room returns."""
        self.lower_cursor()
        for _ in range(limit // SLIDE_COST):
            if self._floor.press(edge, self._state.room()) == room:
                break
        return self._state.room() == room

    def survey(self, box=SCENE_BOX, step=SURVEY_STEP):
        """Raster a room; answer every hotspot's name, box and centre."""
        self.raise_cursor()
        left, top, right, bottom = box
        found = {}
        for y in range(top, bottom + 1, step):
            self.move(left, y)
            self._run.hold(1, ["right"])
            was = -1
            while True:
                self._run.step(1)
                at = self._state.cursor()
                if at[0] == was or at[0] > right:
                    break
                was = at[0]
                hover = self._state.hover()
                if hover:
                    found.setdefault(hover, []).append(at)
            self._run.release(1)
            self._run.step(1)
        return {hover: self._named(hover, points)
                for hover, points in found.items()}

    def _named(self, hover, points):
        """Rest on a hotspot's middle sample and read its name off the line."""
        xs = [x for x, _ in points]
        ys = [y for _, y in points]
        mean = (sum(xs) / len(xs), sum(ys) / len(ys))
        order = sorted(points, key=lambda at: (at[0] - mean[0]) ** 2
                       + (at[1] - mean[1]) ** 2)
        for centre in order[:NAME_TRIES]:
            self.move(*centre)
            self._run.step(SETTLE_FRAMES)
            if self._state.hover() == hover and self._state.line():
                return {"name": self._state.line(), "centre": centre,
                        "count": len(points),
                        "box": (min(xs), min(ys), max(xs), max(ys))}
        return {"name": "", "centre": order[0], "count": len(points),
                "box": (min(xs), min(ys), max(xs), max(ys))}

    def on_word(self, hover):
        """Answer whether the hovered word already names that hotspot."""
        return self._state.hover() == hover

    def on(self, wanted, hover, lag=LINE_LAG):
        """Wait out the line's measured lag behind the hovered word."""
        if hover is not None and self._state.hover() != hover:
            return False
        for _ in range(lag):
            if self._state.line() == wanted:
                return True
            self._run.step(1)
        return self._state.line() == wanted

    def aim(self, x, y, wanted, hover=None, radius=AIM_RADIUS, step=AIM_STEP):
        """Move to a centre and nudge until the line and the word agree."""
        self.move(x, y)
        self._run.step(SETTLE_FRAMES)
        if self.on(wanted, hover):
            return self._state.cursor()
        if hover is not None and self._state.hover() == hover:
            return None
        for ring in range(step, radius + 1, step):
            for at in ((x + ring, y), (x - ring, y), (x, y + ring),
                       (x, y - ring), (x + ring, y + ring),
                       (x - ring, y - ring)):
                self.move(*at)
                self._run.step(SETTLE_FRAMES)
                if self.on(wanted, hover):
                    return self._state.cursor()
                if hover is not None and self._state.hover() == hover:
                    return None
        return None

    def slide(self, x, edge, limit, near):
        """Press left or right until Shaggy's own word says he is there."""
        was = self._state.room()
        way = edge or ("right" if x > self._state.shaggy()[0] else "left")
        self.lower_cursor()
        for _ in range(limit // SLIDE_COST):
            here = self._state.shaggy()[0]
            if abs(here - x) <= near:
                break
            if self._floor.press(way, was) != was:
                break
            now = self._state.shaggy()[0]
            if now == here or (now > x) != (here > x):
                break
        return abs(self._state.shaggy()[0] - x) <= near

    def place(self, x, edge=None, limit=WALK_LIMIT, near=PLACE_NEAR):
        """Walk Shaggy along the room until his own word says he is there."""
        start, was = self.frame(), self._state.room()
        self.slide(x, edge, limit, near)
        if self._state.room() == was:
            self.raise_cursor()
        return {"done": self._state.room() == was,
                "x": self._state.shaggy()[0], "room": self._state.room(),
                "frames": self.frame() - start}

    def resting(self, was):
        """Answer Shaggy's word now, and whether it is the one passed in."""
        now = self._state.shaggy()
        return now, now == was

    def click_and_read(self, wanted, limit=ACTION_LIMIT):
        """Click, wait out what it starts, and answer every phase's cost."""
        start = self.frame()
        self._run.hold(1, [B])
        self._run.step(CLICK_FRAMES)
        self._run.release(1)
        taken = None
        for _ in range(limit // PROBE_FRAMES):
            self._run.step(PROBE_FRAMES)
            if self._state.line() != wanted or not self.playing():
                taken = self.frame() - start
                break
        if taken is None:
            return {"done": False, "taken": None,
                    "scene": {"frames": 0, "cutscene": False, "button": None},
                    "caption": {"frames": 0, "caption": False},
                    "frames": self.frame() - start}
        scene = self.ride()
        drew = self.mash()
        return {"done": True, "taken": taken, "scene": scene, "caption": drew,
                "frames": self.frame() - start}

    def ride(self, onset=SCENE_ONSET, limit=CUTSCENE_LIMIT):
        """Sit out the scene an action starts; the pad is ignored in it."""
        start = self.frame()
        began = None
        for _ in range(onset // PROBE_FRAMES):
            self._run.step(PROBE_FRAMES)
            if not self.playing():
                began = self.frame() - start
                break
        if began is None:
            return {"frames": self.frame() - start, "cutscene": False,
                    "button": None}
        got = self.skip()
        return {"frames": self.frame() - start, "cutscene": True,
                "button": got["button"]}

    def mash(self, wait=CAPTION_WAIT, limit=CAPTION_LIMIT):
        """Wait for the caption's green and clear it one press edge a frame."""
        start = self.frame()
        drew = False
        for _ in range(wait // PROBE_FRAMES):
            self._run.step(PROBE_FRAMES)
            if self.caption() >= CAPTION_LEAST:
                drew = True
                break
        if not drew:
            return {"frames": self.frame() - start, "caption": False}
        for _ in range(limit):
            if self.caption() < CAPTION_LEAST:
                break
            self._run.hold(1, [B])
            self._run.step(MASH_DOWN)
            self._run.release(1)
            self._run.step(MASH_UP)
        self._run.release(1)
        return {"frames": self.frame() - start, "caption": True}

    def spot_of(self, word):
        """Where to click an object: in the room, or in the panel."""
        at = self.aimed(word)
        if at is not None:
            return at
        if self.held(word):
            return PANEL_CELL
        return self.locate(word)

    def pair(self, word, verb):
        """The first object the block for that verb wants, zero for any."""
        for one, target in self._script.verbs(word):
            if one == verb:
                return target
        return None

    def helper(self, word):
        """The named object nearest this one, to open a pair with."""
        box = self.box(word)
        near = []
        mine = self._cart.name(word)
        for other, name in self.present().items():
            if other == word or not name or name == mine:
                continue
            if not self.surveyed(other):
                continue
            far = abs(self.box(other)[0] - box[0]) if box else other
            near.append((far, other))
        return sorted(near)[0][1] if near else None

    def panel_face(self, wanted, turns=PANEL_TURNS):
        """Both rows of the strip, turning it only when items answer here."""
        held = self.carried()
        for _ in range(turns):
            seen = False
            for way in PANEL_WALK:
                if self.names(wanted):
                    return self._state.cursor()
                seen = seen or self._state.hover() in held
                self.nudge(way)
            if self.names(wanted):
                return self._state.cursor()
            if not seen:
                return None
            self.turn_panel()
        return None

    def panel_spot(self, wanted, turns=PANEL_TURNS, swaps=PANEL_SWAPS):
        """Walk the inventory for that item, whichever face the bar shows."""
        self.enter_bar()
        for _ in range(swaps):
            at = self.panel_face(wanted, turns)
            if at is not None:
                return at
            self.tap(C)
        return None

    def names(self, wanted, lag=LINE_LAG):
        """Wait out the line's lag and answer whether it ends with that."""
        for _ in range(lag):
            if self._state.line().endswith(wanted):
                return True
            self._run.step(1)
        return self._state.line().endswith(wanted)

    def turn_panel(self):
        """Click the strip's arrow, where the walk across the row ends."""
        self.tap(B)

    def held(self, word):
        """Whether that object is in the panel rather than in the room."""
        return self.field(word, world.OBJECT_ROOM) == world.CARRIED

    def prepare(self, word, verb, wants=None):
        """Click the first object of a pair; answer the line it leaves."""
        live = self._state.verb()
        open_pair = self._state.pending() if live in JOIN else 0
        named = ([one for one in self.wordsof(wants)
                  if self.field(one, world.OBJECT_ROOM) in
                  (world.CARRIED, self._state.room())] if wants else [])
        first = (open_pair or (named[0] if named else 0)
                 or self.pair(word, verb) or self.helper(word))
        if not first:
            return None
        name = self._cart.name(first)
        if open_pair:
            return "%s %s %s " % (state.VERB_NAME[live], name, JOIN[live])
        self.pick_verb(verb)
        if self.field(first, world.OBJECT_ROOM) == world.CARRIED:
            if self.panel_spot(name) is None:
                return None
        else:
            at = self.aimed(first) or self.locate(first)
            if at is None:
                return None
            if self.aim(at[0], at[1],
                        "%s %s" % (state.VERB_NAME[verb], name),
                        first) is None:
                return None
        self.tap(B)
        self._run.step(PAIR_SETTLE)
        return "%s %s %s " % (state.VERB_NAME[verb], name, JOIN[verb])

    def act(self, verb, x, y, wanted, hover=None, tries=CLICK_TRIES,
            wants=None):
        """Pick the verb if it differs, aim, click, and clear what follows."""
        start = self.frame()
        spent = {"verb": 0, "move": 0, "click": 0, "scene": 0, "caption": 0}
        at = None
        mark = self.frame()
        if verb in JOIN and hover is not None:
            line = self.prepare(hover, verb, wants)
            if line is None:
                return dict(spent, done=False, why="no first object to pair",
                            frames=self.frame() - start)
            wanted = line + self._cart.name(hover)
        for _ in range(VERB_TRIES):
            if verb and self._state.verb() != verb:
                bar = self.frame()
                got = self.pick_verb(verb)
                spent["verb"] += self.frame() - bar
                if got != verb:
                    return dict(spent, done=False,
                                why="the bar refused the verb",
                                frames=self.frame() - start)
            at = (self.panel_spot(self._cart.name(hover))
                  if hover is not None and self.held(hover)
                  else self.aim(x, y, wanted, hover))
            if at is not None:
                break
            if hover is not None and self.surveyed(hover) is None:
                spot = self.locate(hover)
                if spot:
                    x, y = spot
        spent["move"] = self.frame() - mark - spent["verb"]
        if at is None:
            return dict(spent, done=False, hover=self._state.hover(),
                        why="the line never read %r" % wanted,
                        frames=self.frame() - start)
        mark = self.frame()
        hit = None
        for _ in range(tries):
            hit = self.click_and_read(wanted)
            if hit["done"]:
                break
        spent["scene"] = hit["scene"]["frames"]
        spent["caption"] = hit["caption"]["frames"]
        spent["click"] = (self.frame() - mark - spent["scene"]
                          - spent["caption"])
        if not hit["done"]:
            return dict(spent, done=False, why="the click was never taken",
                        frames=self.frame() - start)
        return dict(spent, done=True, at=at, taken=hit["taken"],
                    cutscene=hit["scene"]["cutscene"],
                    button=hit["scene"]["button"],
                    caption=hit["caption"]["caption"],
                    room=self._state.room(), after=self._state.verb(),
                    frames=self.frame() - start)

    def approach(self, word):
        """Stand Shaggy under a box, as deep into the floor as he walks."""
        box = self.box(word)
        if box is None:
            return None
        self.place((box[0] + box[2] // 2) * world.CELL)
        return self._state.shaggy()

    def exits(self):
        """Every room change the scripts hold, by the rooms it joins."""
        out = {}
        for word, room in self.placed().items():
            if room <= world.CARRIED:
                continue
            for verb, goes in self._script.goes(word):
                if goes != room:
                    out.setdefault((room, goes), (word, verb))
        return out

    def handles(self, word, verb):
        """Whether the object's script answers that verb at all."""
        return any(one == verb for one, _ in self._script.verbs(word))

    def destination(self, word, verb=world.WALK_ON):
        """The room that object's own script sends a walker into."""
        for one, goes in self._script.goes(word):
            if one == verb:
                return goes
        return None

    def landed(self, was, want):
        """Whether the room turned into the one the crossing names."""
        here = self._state.room()
        return here == want if want else here != was

    def walk_onto(self, word):
        """Cross by walking: the floor's own path to the doorway."""
        start = self.frame()
        got = self._floor.cross(word, self.destination(word))
        if got["done"]:
            self.wait_for_play()
            for _ in range(RAISE_TRIES):
                if self.raise_cursor():
                    break
        return dict(got, frames=self.frame() - start)

    def walks(self):
        """What the floor's searches have cost, and the paths they found."""
        return self._floor.tally()

    def cross(self, word, verb):
        """Take one derived exit: walk onto its box, or act on it."""
        start, was = self.frame(), self._state.room()
        if verb == world.WALK_ON:
            self.walk_onto(word)
        else:
            name = "%s %s" % (state.VERB_NAME[verb], self._cart.name(word))
            for _ in range(CROSS_TRIES):
                if self._state.room() != was:
                    break
                at = self.aimed(word) or self.locate(word)
                if at is None or self.pick_verb(verb) != verb:
                    break
                if self.aim(at[0], at[1], name, word) is None:
                    break
                self.tap(B)
                self.arrive(was)
        self.wait_for_play()
        return {"done": self.landed(was, self.destination(word, verb)),
                "room": self._state.room(),
                "word": word, "verb": verb, "frames": self.frame() - start}

    def arrive(self, was, limit=CROSS_LIMIT):
        """Wait out the scene an acted room change plays before it turns."""
        for _ in range(limit // PROBE_FRAMES):
            if self._state.room() != was:
                return True
            self._run.step(PROBE_FRAMES)
        return False

    def steady(self, ticks=STEADY_TICKS):
        """Answer whether the bar has been up for that many samples running."""
        for _ in range(ticks):
            if not self.playing():
                return False
            self._run.step(PROBE_FRAMES)
        return self.playing()

    def skip(self, buttons=SKIP_BUTTONS, probe=SKIP_PROBE,
             limit=CUTSCENE_LIMIT):
        """Mash a cutscene away; answer the frames and the button that did."""
        start = self.frame()
        for at, button in enumerate(buttons):
            budget = limit if at == len(buttons) - 1 else probe
            spent = 0
            while spent < budget:
                if self.playing() and self.steady():
                    return {"done": True, "button": button,
                            "frames": self.frame() - start}
                self._run.hold(1, [button])
                self._run.step(SKIP_DOWN)
                self._run.release(1)
                self._run.step(SKIP_UP)
                spent += SKIP_DOWN + SKIP_UP
        return {"done": self.playing(), "button": None,
                "frames": self.frame() - start}

    def wait_for_play(self, limit=CUTSCENE_LIMIT):
        """Run until 0x630 says the pad is read again, pressing nothing."""
        start = self.frame()
        self._run.run_until(LIVE_AGAIN, limit)
        return self.frame() - start

    def inventory(self, rows=PANEL_ROW, columns=BAR_COLUMN):
        """Swap the bar to the panel and read every cell off the status line."""
        held = []
        self.tap(C)
        for row in rows:
            for column in columns:
                self.move(column, row)
                said = self._state.line()
                if said:
                    held.append(said)
        self.tap(C)
        return held

    def panel_cell(self, wanted, rows=PANEL_ROW, columns=BAR_COLUMN):
        """Answer the panel cell whose status line reads that, or None."""
        for row in rows:
            for column in columns:
                self.move(column, row)
                if self._state.line().endswith(wanted):
                    return column, row
        return None
