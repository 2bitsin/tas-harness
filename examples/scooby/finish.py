"""The computed plan played on the emulator, one checkpoint an action."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import cartridge
import engine
import graph
import hotel
import plan
import route
import state
import tash
import world

ACT_NAME = "plan-%02d"
SEED_NAME = "lobby"
PAIRED = {state.USE: " with ", state.GIVE: " to "}
ROOM_SCRIPT = -1


class Finish:
    """Plays what Plan.order() computed, measuring and checkpointing it."""

    def __init__(self, run):
        self._run = run
        self._plan = plan.Plan(run)
        self._cart = cartridge.Cartridge(run)
        self._hotel = hotel.Hotel(run, ())
        self._state = state.State(run)
        self._done = []

    def rooms(self):
        """Room word to the name world.ROOMS gives it."""
        return {one["word"]: name for name, one in world.ROOMS.items()
                if one["word"] is not world.UNKNOWN}

    def line(self, verb, word, target):
        """The guide-shaped line for one action, which names its pair."""
        name = self._cart.name(word) or str(word)
        if not verb or verb == ROOM_SCRIPT:
            return "Stand in room %s" % word
        said = "%s %s" % (state.VERB_NAME[verb], name)
        if target:
            said += "%s%s" % (PAIRED.get(verb, " with "),
                              self._cart.name(target) or str(target))
        return said

    def crossings(self, number, facts, room, names):
        """One step a crossing, over the room list Plan.route() gives."""
        here = facts[(graph.HERE,)]
        walk = self._plan.route(here, room, facts) if room else None
        walk, out = walk or (), []
        for one, two in zip(walk, walk[1:]):
            cross = self._plan.crosser(one, two, facts)
            word = cross["word"] if cross else None
            out.append(dict(route.step(
                number, names.get(two, str(two)), route.WALK,
                "Goto %s" % names.get(two, two),
                verb=cross["verb"] if cross else None,
                obj=self._cart.name(word) if word else None, hover=word)))
            facts = self._plan.walked(one, two, facts)
        return out

    def action(self, number, one, room, names):
        """The plan's own action, as the guide-shaped step Hotel.one runs."""
        word, verb, target = one["word"], one["verb"], one["target"]
        click = target or word
        return dict(route.step(
            number, names.get(room, str(room)),
            route.WALK if verb == ROOM_SCRIPT else route.ACT,
            self.line(verb, word, target),
            verb=None if verb == ROOM_SCRIPT else verb,
            obj=self._cart.name(click), hover=click))

    def steps(self):
        """Every rung: the crossings it walks, then the action it plays."""
        names, out = self.rooms(), []
        acts, facts = self._plan.acted(), dict(self._plan.state())
        for number, (key, frames) in enumerate(self._plan.order(), 1):
            one = acts[key]
            room = self._plan.where(one, facts)
            legs = self.crossings(number, facts, room, names)
            legs.append(self.action(number, one, room, names))
            after = self._plan.played(one, self._plan.stood(one, facts))
            out.append({"n": number, "predicted": frames,
                        "room": legs[-1]["room"],
                        "source": legs[-1]["source"], "steps": tuple(legs),
                        "wants": {key: value for key, value in after.items()
                                  if facts.get(key) != value}})
            facts = after
        return tuple(out)

    def drift(self, leg):
        """Which facts the action was to write and the ram does not hold."""
        live = self._plan.afresh()
        return {key: (value, live.get(key))
                for key, value in leg["wants"].items()
                if live.get(key) != value}

    def line_frames(self):
        """How long the tape's line is now, which is what a replay runs."""
        return self._run.line()

    def rung(self, leg):
        """One rung run step by step, its frames summed over all of them."""
        spent, why, ok = 0, "", True
        for step in leg["steps"]:
            got = self._hotel.one(step)
            spent += got["frames"]
            ok = got["ok"]
            if not ok:
                why = "%s: %s" % (step["source"], got.get("why") or "no")
                break
        apart = self.drift(leg)
        return {"n": leg["n"], "room": leg["room"], "source": leg["source"],
                "spent": spent, "predicted": leg["predicted"], "ok": ok,
                "why": why or ("" if not apart else "drift %s" % apart),
                "walks": len(leg["steps"]) - 1, "apart": len(apart),
                "at": self.frame(), "line": 0}

    def frame(self):
        return self._run.observe()["frame"]

    def seed(self, name=SEED_NAME):
        """Power on and play the tape, so the line runs from frame zero."""
        self._hotel.start()
        self._run.checkpoint(name)
        return self.frame()

    def resume(self, first):
        """Restore what the rung before this one left, and raise the cursor."""
        if first <= 1:
            return 0
        self._run.restore(ACT_NAME % (first - 1))
        self._hotel.ready()
        return first - 1

    def play(self, legs, first=1, last=None):
        """Run the plan's rungs in order, stopping at the first refusal."""
        for leg in legs:
            if leg["n"] < first or (last and leg["n"] > last):
                continue
            record = self.rung(leg)
            self._done.append(record)
            print(self.row(record), flush=True)
            if not record["ok"]:
                break
            self._run.checkpoint(ACT_NAME % leg["n"])
            record["line"] = self.line_frames() or 0
        return self._done

    def row(self, record):
        """One line of the measured against predicted table."""
        return ("%3d %-9s %-30s %5d %7d %7d %8d %5d %s"
                % (record["n"], record["room"], record["source"][:30],
                   record["walks"], record["spent"], record["predicted"],
                   record["line"], record["apart"],
                   record["why"] or "ok"))

    def table(self):
        """The measured frames table, header and all."""
        head = ("%3s %-9s %-30s %5s %7s %7s %8s %5s %s"
                % ("n", "room", "action", "hops", "frames", "plan", "line",
                   "off", "took"))
        return "\n".join([head] + [self.row(one) for one in self._done])

    def total(self):
        """Frames measured against frames predicted, over what ran."""
        return {"measured": sum(one["spent"] for one in self._done),
                "predicted": sum(one["predicted"] for one in self._done),
                "line": max([one["line"] for one in self._done] or [0]),
                "actions": len(self._done)}

    def won(self):
        """Whether the ending room word stands and the screen holds it."""
        return self._state.room() == graph.ENDING_ROOM

    def looked(self):
        """The clicks the cartridge centre answered, and those it did not."""
        return dict(self._hotel.tally(),
                    fell=tuple(one for one in self._hotel.looks()
                               if one["how"] != engine.BY_CENTRE))

    def crossed(self):
        """The trials and frames the floor's crossing searches spent."""
        return self._hotel.walks()

    def screen(self):
        """The hash of where the run stopped, and the words that say so."""
        return self._hotel.last_screen()


def main(run, first=1, last=None):
    """Play the computed plan from the seed, for `tash run --scenario`."""
    play = Finish(run)
    play.seed()
    steps = play.steps()
    play.resume(first)
    play.play(steps, first=first, last=last)
    print(play.table(), flush=True)
    print(play.total(), flush=True)
    print(play.screen(), flush=True)
    print(play.crossed(), flush=True)
    seen = play.looked()
    print({key: one for key, one in seen.items() if key != "fell"},
          flush=True)
    for one in seen["fell"]:
        print(one, flush=True)
    run.mark("plan last screen", group="plan")
    return play


if getattr(tash, "run", None) is not None and __name__ != "finish":
    main(tash.run)
