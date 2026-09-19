"""The dependency graph, derived from the cartridge's scripts alone."""

# A block's condition is judged once, where the vm reaches the if, so an
# assignment inside it cannot unmake the gate its siblings stand on.

import script
import world

ON_VERB, SAY, IF, GUARD, PUT, ASSIGN = 1, 2, 3, 4, 5, 8
GO = (17, 24)
# Two more one-field writes, each proved by the gate that reads it: opcode
# 11 sets field 0 (`11 29 3` in the Wheels' script is what the Mine Car's
# ride wants) and opcode 10 sets field 4 (`10 28 43` on the third Wheel is
# what makes the Wheels usable).
POSE, POSE_FIELD = 11, 0
RENAME, RENAME_FIELD = 10, 4
EFFECTS = (PUT, ASSIGN, POSE, RENAME) + GO

VERB_AT, TARGET_AT = 2, 4
POSE_OBJECT, POSE_VALUE = 2, 4
COND_LEFT, COND_RIGHT, COND_KIND = 6, 0xa, 0xe
PUT_OBJECT, PUT_ROOM = 2, 4
GUARD_ELSE = 4
ASSIGN_DEST, ASSIGN_SOURCE, ASSIGN_KIND = 2, 6, 0xa
GO_ROOM = 2

QUIET = 0x7fff
WORD = 2

# The condition and assignment encodings, read off the evaluator at rom
# 0x2796 and the assignment handler at 0x2c56: a condition's left side is
# selected by bits 0 and 1 of its kind word, its right side by bits 2 and 3.
LEFT_BIT, RIGHT_BIT, SOURCE_BIT = 0, 2, 2
EQUAL, ABOVE, BELOW = 0x10, 0x20, 0x40
DEST_FLAG, PLAIN = 1, 0x10
LESS, EITHER = 0x20, 0x40

# Condition side (field 6, word 1) is the room word itself, not a record.
ROOM_FIELD, ROOM_SELF = 6, 1

ROOM_VERB, BLOCK_HEAD, BLOCK_END = -1, 8, 6
SAY, SAY_BODY, SAY_TEXT, TALK_VERB = 2, 0xe, 2, 8
SHAGGY = 3
ENDING_ROOM = 10
WALK_ON = 11
PAIR_VERBS = (5, 6)

FLAG, FIELD, HERE, VALUE = "flag", "field", "here", "value"


class Graph:
    """Facts, actions and gates, read out of every object's script."""

    def __init__(self, run):
        self._script = script.Script(run)
        self._actions = None
        self._arrivals = None

    def word(self, at):
        return self._script._word(at)

    def side(self, kind, shift, at, itself=False):
        """One side of a condition or an assignment, as a fact or a value."""
        one, two = self.word(at), self.word(at + WORD)
        if kind >> shift & 1:
            return (FLAG, one, two)
        if kind >> (shift + 1) & 1:
            if itself and (one, two) == (ROOM_FIELD, ROOM_SELF):
                return (HERE,)
            return (FIELD, two, one)
        return (VALUE, one)

    def condition(self, at):
        """One opcode 3 or 4 condition, as left, how and right."""
        kind = self.word(at + COND_KIND)
        how = ("==" if kind & EQUAL else ">" if kind & ABOVE
               else "<" if kind & BELOW else "!=")
        return (self.side(kind, LEFT_BIT, at + COND_LEFT, True), how,
                self.side(kind, RIGHT_BIT, at + COND_RIGHT))

    def effect(self, at, op):
        """What one instruction makes true: a fact, how it changes, a side."""
        if op == PUT:
            return ((FIELD, self.word(at + PUT_OBJECT) & QUIET, ROOM_FIELD),
                    "=", (VALUE, self.word(at + PUT_ROOM)))
        if op in GO:
            return ((HERE,), "=", (VALUE, self.word(at + GO_ROOM)))
        if op in (POSE, RENAME):
            field = POSE_FIELD if op == POSE else RENAME_FIELD
            return ((FIELD, self.word(at + POSE_OBJECT) & QUIET, field),
                    "=", (VALUE, self.word(at + POSE_VALUE)))
        kind = self.word(at + ASSIGN_KIND)
        source = self.side(kind, SOURCE_BIT, at + ASSIGN_SOURCE)
        if kind & DEST_FLAG:
            dest = (FLAG, self.word(at + ASSIGN_DEST),
                    self.word(at + ASSIGN_DEST + WORD))
            return (dest, "=" if kind & PLAIN else "^", source)
        dest = (FIELD, self.word(at + ASSIGN_DEST + WORD),
                self.word(at + ASSIGN_DEST))
        how = ("=" if kind & PLAIN else "-" if kind & LESS
               else "|" if kind & EITHER else "+")
        return (dest, how, source)

    def follows(self, flat, index, stop=None):
        """Every effect a matched verb block reaches, with its gates."""
        own, elses, out, skip = [], [], [], None
        for order, (depth, at, op, total) in enumerate(flat):
            del own[depth:]
            elses = [one for one in elses if one[2] > at]
            if skip is not None and depth > skip:
                continue
            skip = None
            if order > index and op == ON_VERB:
                skip = depth
                continue
            after = order > index and (stop is None or at < stop)
            if after and op in EFFECTS and not self.transient(at, op):
                out.append({
                    "gates": tuple([g for g in own if g]
                                   + [g for g, first, end in elses
                                      if first <= at]),
                    "effect": self.effect(at, op), "op": op})
            if op in (IF, GUARD):
                own.append((self.condition(at), True))
                other = op == GUARD and self.word(at + GUARD_ELSE)
                if other:
                    elses.append(((self.condition(at), False),
                                  at + total, at + total + other))
            else:
                own.append(None)
        return tuple(out)

    def transient(self, at, op):
        """A pose on Shaggy, which his own walk overwrites the next frame."""
        return (op == POSE
                and self.word(at + POSE_OBJECT) & QUIET == SHAGGY)

    def actions(self):
        """Every verb block of every script, with its gates and effects."""
        if self._actions is None:
            found = []
            for word in sorted(self._script._offsets()):
                flat = self._script.walk(word)
                for order, (depth, at, op, total) in enumerate(flat):
                    if op != ON_VERB:
                        continue
                    stop = at + self.word(at + BLOCK_END)
                    found.append({
                        "word": word, "verb": self.word(at + VERB_AT),
                        "target": self.word(at + TARGET_AT), "at": at,
                        "branches": self.follows(flat, order, stop)})
            self._actions = (tuple(found) + self.choices()
                             + self.chores())
        return self._actions

    def choices(self):
        """Every dialogue answer: opcode 2 runs when it is picked."""
        found = []
        for word in sorted(self._script._offsets()):
            flat = self._script.walk(word)
            for order, (depth, at, op, total) in enumerate(flat):
                if op != SAY or total <= SAY_BODY:
                    continue
                found.append({
                    "word": word, "verb": TALK_VERB, "choice": True,
                    "target": self.word(at + SAY_TEXT + WORD), "at": at,
                    "branches": self.follows(flat, order, at + total)})
        return tuple(found)

    def chore(self, room, start, length):
        """One room script as an action, gated on standing in its room."""
        gate = (((HERE,), "==", (VALUE, room)), True)
        return {"word": 0, "verb": ROOM_VERB, "target": room, "room": room,
                "at": start, "branches": tuple(
                    dict(one, gates=one["gates"] + (gate,))
                    for one in self.follows(
                        self._script.reach(start, length), -1))}

    def chores(self):
        """Every per-frame room script, as an action a plan can order."""
        return tuple(self.chore(room, start, length)
                     for room, frame, start, length in self._script.rooms()
                     if frame and self._script.reach(start, length))

    def arrivals(self):
        """Each room's entry script: run by arriving, never chosen."""
        if self._arrivals is None:
            found = {}
            for room, frame, start, length in self._script.rooms():
                if frame or not self._script.reach(start, length):
                    continue
                found.setdefault(room, []).append(
                    self.chore(room, start, length))
            self._arrivals = {room: tuple(ones)
                              for room, ones in found.items()}
        return self._arrivals

    def doorways(self):
        """Every action whose verb is the walk-onto a doorway answers."""
        return tuple(one for one in self.actions()
                     if one["verb"] == WALK_ON and self.rooms(one))

    def rooms(self, action):
        """The rooms an action's branches send Shaggy to."""
        out = []
        for branch in action["branches"]:
            fact, _, source = branch["effect"]
            if fact == (HERE,) and source[0] == VALUE:
                out.append(source[1])
            if fact == (FIELD, SHAGGY, ROOM_FIELD) and source[0] == VALUE:
                out.append(source[1])
        return tuple(dict.fromkeys(out))

    def facts(self):
        """Every fact any script reads or writes, gates and effects alike."""
        found = set()
        for one in self.actions():
            for branch in one["branches"]:
                found.add(branch["effect"][0])
                for (left, _, right), _ in branch["gates"]:
                    found.update(side for side in (left, right)
                                 if side[0] != VALUE)
        return found

    def gates(self):
        """Every distinct condition the scripts gate an effect behind."""
        found = set()
        for one in self.actions():
            for branch in one["branches"]:
                found.update(gate for gate, _ in branch["gates"])
        return found

    def goal(self, room=ENDING_ROOM):
        """The action that ends the scenario, and the room word it reaches."""
        for one in self.actions():
            if room in self.rooms(one):
                return one
        return None

    def read(self, side, state):
        """What one side of a condition is worth in a planner's state."""
        if side[0] == VALUE:
            return side[1]
        return state.get(side, 0)

    def holds(self, condition, state):
        """Whether one gate holds in that state."""
        left, how, right = condition
        one, two = self.read(left, state), self.read(right, state)
        if how == "==":
            return one == two
        if how == "!=":
            return one != two
        if how == ">":
            return one > two
        return one < two

    def open_gates(self, branch, state):
        """Whether every gate on a branch stands the way it has to."""
        return all(self.holds(gate, state) == want
                   for gate, want in branch["gates"])

    def apply(self, branch, state):
        """The state one branch's effect leaves behind."""
        fact, how, source = branch["effect"]
        value = self.read(source, state)
        was = state.get(fact, 0)
        after = dict(state)
        after[fact] = (value if how == "=" else was ^ value if how == "^"
                       else was - value if how == "-"
                       else was | value if how == "|" else was + value)
        return after

    def run(self, action, state):
        """Play the branches; the ending screen is where a run stops."""
        after, judged = dict(state), {}
        for branch in action["branches"]:
            if all(judged.setdefault(gate, self.holds(gate, after)) == want
                   for gate, want in branch["gates"]):
                after = self.apply(branch, after)
                if after.get((HERE,)) == ENDING_ROOM:
                    break
        return after

    def yields(self, action, state):
        """Every fact one action changes from that state, as fact to value."""
        after = self.run(action, state)
        return {fact: value for fact, value in after.items()
                if state.get(fact, 0) != value}

    def needs(self, action):
        """Every gate every branch of an action carries, deduplicated."""
        found = []
        for branch in action["branches"]:
            for gate in branch["gates"]:
                if gate not in found:
                    found.append(gate)
        return tuple(found)
