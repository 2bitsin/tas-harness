"""Blake's Hotel played from the tape, one named checkpoint a step."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import engine
import route
import state
import tash
import world

TAPE = "examples/scooby/tapes/title-to-control.yaml"
STEP_NAME = "hotel-%02d"
KINDS = ("verb", "move", "click", "scene", "caption",
         "walk", "find")
FIRST_STEP = 1
LAST_VERIFIED = 16
HOP_LIMIT = 6
SUMMON_DEPTH = 2
JOINED = {state.USE: " with ", state.GIVE: " to "}
HOP_COST = 400


class Hotel:
    """Plays route.STEPS, measuring and checkpointing every one of them."""

    def __init__(self, run, steps=route.STEPS):
        self._run = run
        self._state = state.State(run)
        self._cursor = engine.Cursor(run)
        self._steps = steps
        self._done = []

    def frame(self):
        return self._run.observe()["frame"]

    def start(self, tape=TAPE):
        """Reach the lobby from power on by playing the tape, every time."""
        self._run.reset()
        self._run.play(tape)
        self._run.release(1)
        self._run.step(1)
        return self.frame()

    def resume(self, upto):
        """Restore the newest step checkpoint at or below that number."""
        for number in range(upto, 0, -1):
            try:
                self._run.restore(STEP_NAME % number)
            except Exception:
                continue
            self._run.release(1)
            self._cursor.raise_cursor()
            return number
        self.start()
        return 0

    def ready(self):
        """Put the crosshair back up after a restore, so the pad aims."""
        self._run.release(1)
        self._cursor.raise_cursor()
        return self.frame()

    def centre(self, step, word=None):
        """The word and the centre the cartridge gives this step's object."""
        for candidate in ([word] if word else self.words(step)):
            if self._cursor.held(candidate):
                return engine.PANEL_CELL, candidate
            at = self._cursor.aimed(candidate)
            if at is not None:
                return at, candidate
            at = self._cursor.locate(candidate)
            if at is not None:
                return at, candidate
        if step["centre"]:
            return step["centre"], step["hover"]
        return None, None

    def words(self, step):
        """Which words this step clicks: the named ones, or the one given."""
        named = self._cursor.like(step["object"] or "")
        if step["every"]:
            return named
        if step["hover"] and step["hover"] in named:
            return [step["hover"]]
        return ([step["hover"]] if step["hover"] else []) + named

    def edges(self):
        """The room graph as Dijkstra wants it, where a crossing is timed."""
        known = {}
        for one in world.EDGES:
            here = world.ROOMS.get(one["room"], {}).get("word")
            there = world.ROOMS.get(one["goes"], {}).get("word")
            if here and there and one["frames"] is not world.UNKNOWN:
                known[(here, there)] = one
        return known

    def way_to(self, room):
        """The holds tash.plan.graph picks from here to that room word."""
        known = self.edges()
        lines = [(here, there, one["frames"])
                 for (here, there), one in known.items()]
        if not lines or self._state.room() == room:
            return []
        walk = tash.plan.graph(lines, self._state.room(), room)
        nodes = walk["nodes"]
        return [known[(nodes[n], nodes[n + 1])]["hold"]
                for n in range(len(nodes) - 1)]

    def goes(self, step):
        """Where a walk step lands: the room the next step works in."""
        for one in self._steps:
            if one["n"] > step["n"]:
                return one["room"]
        return step["room"]

    def holds(self):
        """The held direction each measured edge crosses on, by room word."""
        out = {}
        for one in world.EDGES:
            here = world.ROOMS.get(one["room"], {}).get("word")
            there = world.ROOMS.get(one["goes"], {}).get("word")
            if here and there:
                out[(here, there)] = one
        return out

    def travel(self, room, hops=HOP_LIMIT):
        """Walk to a room word over the graph the scripts derive."""
        spent, holds = 0, self.holds()
        for _ in range(hops):
            here = self._state.room()
            if here == room:
                return {"done": True, "frames": spent, "room": here}
            exits = self._cursor.exits()
            lines = [(one, two, self.cost(holds, one, two))
                     for one, two in exits]
            plan = tash.plan.graph(lines, here, room)
            if not plan["nodes"][1:]:
                return {"done": False, "frames": spent, "room": here,
                        "why": "no derived way to room %d" % room}
            hop = (plan["nodes"][0], plan["nodes"][1])
            word, verb = exits[hop]
            mark = self.frame()
            went = self._cursor.cross(word, verb)
            spent += self.frame() - mark
            if self._state.room() == here:
                return {"done": False, "frames": spent, "room": here,
                        "why": "obj %d verb %d did not leave" % (word, verb),
                        "went": went}
        return {"done": self._state.room() == room, "frames": spent,
                "room": self._state.room()}

    def hop(self, step, room):
        """Take the one crossing a step names, rather than route again."""
        was = self._state.room()
        went = self._cursor.cross(step["hover"], step["verb"])
        here = self._state.room()
        done = here == room if room else here != was
        return {"done": done, "room": here,
                "frames": went.get("frames", 0),
                "why": "" if done else "obj %d verb %d left %d into %d, not %s"
                       % (step["hover"], step["verb"], was, here, room)}

    def cost(self, holds, here, there):
        """What a hop costs: the measured frames, or the standing guess."""
        one = holds.get((here, there))
        if one and one["frames"] is not world.UNKNOWN:
            return one["frames"]
        return HOP_COST

    def fetch(self, step, record):
        """Stand in the room the step names, or one the array says holds it."""
        want = world.ROOMS.get(step["room"], {}).get("word")
        mark = self.frame()
        if want and self._state.room() != want:
            self.travel(want)
        rooms = self._cursor.whereis(step["object"] or "")
        if rooms and self._state.room() not in rooms \
                and world.CARRIED not in rooms:
            for room in rooms:
                if self.travel(room)["done"]:
                    break
        record["walk"] += self.frame() - mark
        return True

    def summon(self, name, record, depth=SUMMON_DEPTH):
        """Run the derived action that makes an object of that name appear."""
        cursor = self._cursor
        if cursor.like(name) or name in cursor.carried().values():
            return True
        here = self._state.room()
        for word in cursor.wordsof(name):
            for source, verb, room in cursor.providers(word):
                if room != here or not verb or verb == world.WALK_ON:
                    continue
                if not cursor.here(source) and depth > 0:
                    self.summon(cursor.name(source), record, depth - 1)
                if not cursor.here(source):
                    continue
                at = cursor.surveyed(source)
                if at is None:
                    continue
                mark = self.frame()
                cursor.act(verb, at[0], at[1],
                           "%s %s" % (state.VERB_NAME[verb],
                                      cursor.name(source)), source)
                record["find"] += self.frame() - mark
                if cursor.like(name):
                    return True
        return bool(cursor.like(name))

    def one(self, step):
        """Run one route step and answer what it cost and whether it took."""
        begin = self.frame()
        record = {"n": step["n"], "source": step["source"],
                  "room": step["room"], "distance": 0, "ok": True}
        for kind in KINDS:
            record.setdefault(kind, 0)
        if step["anchor"] is not None:
            mark = self.frame()
            self._cursor.place(step["anchor"], "right")
            record["find"] = self.frame() - mark
        if step["kind"] == route.WALK:
            mark = self.frame()
            room = world.ROOMS.get(self.goes(step), {})
            if step["hover"]:
                went = self.hop(step, room.get("word"))
            elif room.get("word"):
                went = self.travel(room["word"])
            else:
                went = {"done": False, "why": "no word for that room"}
            record["walk"] = self.frame() - mark
            record.update(ok=went["done"], word=self._state.room(),
                          why=went.get("why", ""),
                          frames=self.frame() - begin)
            return record
        self.fetch(step, record)
        if step["object"] and not self.words(step):
            self.summon(step["object"], record)
        for word in (self.words(step) if step["every"] else [None]):
            self.click(step, word, record)
            if not record["ok"]:
                break
        record["frames"] = self.frame() - begin
        return record

    def click(self, step, word, record):
        """Aim one of the step's objects and click it, adding what it cost."""
        at, hover = self.centre(step, word)
        if at is None:
            record.update(ok=False, word=self._state.room(),
                          why="no hotspot reads %r" % step["object"])
            return record
        if hover and not self._cursor.on_word(hover):
            mark = self.frame()
            at = self._cursor.reach(hover) or at
            record["find"] += self.frame() - mark
        was = self._state.cursor()
        record["distance"] += abs(at[0] - was[0]) + abs(at[1] - was[1])
        verb = step["verb"]
        if hover and verb:
            verb = self._cursor.instead(hover, verb)
        wanted = self._cursor.name(hover) if hover else step["object"]
        if verb:
            wanted = "%s %s" % (state.VERB_NAME[verb], wanted)
        acted = self._cursor.act(verb, at[0], at[1], wanted, hover,
                                 wants=self.wants(step, verb))
        for kind in ("verb", "move", "click", "scene", "caption"):
            record[kind] += acted.get(kind) or 0
        record.update(ok=acted["done"], why=acted.get("why", ""),
                      word=self._state.room(),
                      cutscene=acted.get("cutscene"),
                      button=acted.get("button"))
        if record["ok"] and self._cursor.fact(step["yields"]) is False:
            record.update(ok=False,
                          why="the click took but %r did not follow"
                              % step["yields"])
        return record

    def wants(self, step, verb):
        """The first object of a pair, as the guide's own line names it."""
        if verb not in JOINED:
            return None
        words = step["source"].split(JOINED[verb])
        return words[0].split(" ", 1)[-1].strip() if len(words) > 1 else None

    def play(self, first=1, last=None):
        """Run the route between two steps, stopping at a refusal."""
        for step in self._steps:
            if step["n"] < first or (last and step["n"] > last):
                continue
            record = self.one(step)
            self._done.append(record)
            print(self.row(record), flush=True)
            if not record["ok"]:
                break
            self._run.checkpoint(STEP_NAME % step["n"])
        return self._done

    def row(self, record):
        """One line of the per-step cost table."""
        return ("%3d %-9s %-30s %6d %5d %5d %5d %5d %5d %5d %5d %s"
                % (record["n"], record["room"], record["source"][:30],
                   record["frames"], record["find"], record["verb"],
                   record["move"], record["click"], record["scene"],
                   record["caption"], record["distance"],
                   "ok" if record["ok"] else record.get("why", "no")))

    def table(self):
        """The per-step cost table, header and all."""
        head = ("%3s %-9s %-30s %6s %5s %5s %5s %5s %5s %5s %5s %s"
                % ("n", "room", "step", "frames", "find", "verb", "move",
                   "click", "scene", "capt", "dist", "took"))
        return "\n".join([head] + [self.row(one) for one in self._done])

    def kinds(self):
        """Where the frames went, by kind."""
        total = sum(one["frames"] for one in self._done)
        lines = ["%-10s %8s %6s" % ("kind", "frames", "share")]
        for kind in KINDS:
            spent = sum(one.get(kind, 0) for one in self._done)
            if spent:
                lines.append("%-10s %8d %5.1f%%"
                             % (kind, spent, 100.0 * spent / max(total, 1)))
        lines.append("%-10s %8d" % ("total", total))
        return "\n".join(lines)

    def cutscenes(self):
        """Every cutscene the route hit, with its frames and its button."""
        lines = ["%3s %-34s %7s %s" % ("n", "step", "frames", "button")]
        for one in self._done:
            if one.get("cutscene"):
                lines.append("%3d %-34s %7d %s"
                             % (one["n"], one["source"][:34], one["scene"],
                                one.get("button") or "none"))
        return "\n".join(lines)

    def looks(self):
        return self._cursor.looks()

    def tally(self):
        return self._cursor.tally()

    def walks(self):
        """What the floor searches cost, and the paths they found."""
        return self._cursor.walks()

    def last_screen(self):
        """The hash of where the route stopped, and the words that say so."""
        seen = self._run.observe()
        return {"frame": seen["frame"], "exact": seen["exact"],
                "difference": seen["difference"],
                "perceptual": seen["perceptual"],
                "room": self._state.room(), "verb": self._state.verb(),
                "line": self._state.line()}


def main(run, first=FIRST_STEP, last=LAST_VERIFIED):
    """Play the route from power on, for `tash run --scenario`."""
    play = Hotel(run)
    play.start()
    play.play(first=first, last=last)
    print(play.table(), flush=True)
    print(play.kinds(), flush=True)
    print(play.cutscenes(), flush=True)
    print(play.last_screen(), flush=True)
    run.mark("hotel step 1 last screen", group="hotel")
    return play


import tash

if getattr(tash, "run", None) is not None and __name__ != "hotel":
    main(tash.run)
