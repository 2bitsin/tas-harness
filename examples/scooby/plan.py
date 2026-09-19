"""The completion path, computed over the graph with measured costs."""

import budget
import cartridge
import graph
import state
import tash
import world

LOOK, SHUT, WALK_ON = 10, 4, 11
NOWHERE, INVENTORY = 0, 1
ROOM_FIELD = 6
FLAG_BASE, FLAG_BYTES = 0x2a00, 0x20
OBJECT_BITS = 8
WORD_BYTES = 2

# Measured on the emulator in step 1d: the median room crossing and the
# median on-screen action, used where world.EDGES has no number.
HOP_FRAMES, SCREEN_FRAMES = 366, 700

# The two prunes: Look never changes a fact the ending reads, and Shut
# only undoes an Open, so neither can shorten a tour.
QUIET_VERBS = (LOOK, SHUT, WALK_ON)

ROOM_WORDS = 0x20
EVERYWHERE = frozenset(range(ROOM_WORDS))
SOMEWHERE = frozenset(range(INVENTORY, ROOM_WORDS))

# The bounds every search runs under: expansions of the regression, and
# wall seconds, whichever runs out first.
EXPANSIONS = 20000
SECONDS = 240.0
CHAIN_DEPTH = 20
ROUNDS = 24


class Plan:
    """The relevant actions, ordered by the cheapest tour that wins."""

    def __init__(self, run):
        self._run = run
        self._graph = graph.Graph(run)
        self._cart = cartridge.Cartridge(run)
        self._boxes = None
        self._over = None
        self._state = None
        self._far = {}
        self._keep = None
        self._words = None
        self._price = None
        self._spine = None
        self._crossings = None
        self._relevant = None
        self._failed = set()

    def raw(self, at, count):
        """A block of work ram with the core's byte swap undone."""
        got = self._run.memory("system", at, count)
        return bytes(got[one ^ 1] for one in range(len(got)))

    def state(self):
        """Every fact the planner tracks, read out of the live ram."""
        if self._state is None:
            here = self.raw(state.ROOM, WORD_BYTES)
            span = world.OBJECT_BYTES * (world.OBJECT_LAST + 1)
            live = self.raw(world.OBJECTS, span)
            flags = self.raw(FLAG_BASE, FLAG_BYTES)
            found = {(graph.HERE,): int.from_bytes(here, "big")}
            for word, field in self.fields():
                at = ((word - world.OBJECT_FIRST) * world.OBJECT_BYTES
                      + field)
                if 0 <= at <= len(live) - WORD_BYTES:
                    found[(graph.FIELD, word, field)] = int.from_bytes(
                        live[at:at + WORD_BYTES], "big")
            for byte in range(FLAG_BYTES):
                for bit in range(OBJECT_BITS):
                    found[(graph.FLAG, byte, bit)] = flags[byte] >> bit & 1
            self._state = found
        return self._state

    def afresh(self):
        """Read the facts out of ram again, the run having moved on."""
        self._state = None
        return self.state()

    def fields(self):
        """Every object record word a script reads, the room field aside."""
        found = {(word, ROOM_FIELD)
                 for word in range(world.OBJECT_FIRST, world.OBJECT_LAST)}
        for one in self._graph.facts():
            if one[0] == graph.FIELD:
                found.add((one[1], one[2]))
        return sorted(found)

    def boxes(self):
        """Each object's hotspot box in cells, by the record's box number."""
        if self._boxes is None:
            span = world.OBJECT_BYTES * (world.OBJECT_LAST + 1)
            live, found = self.raw(world.OBJECTS, span), {}
            for word in range(world.OBJECT_FIRST, world.OBJECT_LAST + 1):
                at = ((word - world.OBJECT_FIRST) * world.OBJECT_BYTES
                      + world.OBJECT_BOX)
                if at > len(live) - WORD_BYTES:
                    continue
                box = self._cart.box(
                    int.from_bytes(live[at:at + WORD_BYTES], "big"))
                if box:
                    found[word] = box
            self._boxes = found
        return self._boxes

    def cells(self, box):
        """The eight-pixel cells one box covers."""
        return {(x, y) for x in range(box[0], box[0] + box[2])
                for y in range(box[1], box[1] + box[3])}

    def over(self, word):
        """Every higher word whose box overlaps this one's."""
        if self._over is None:
            boxes, found = self.boxes(), {}
            marked = {one: self.cells(box) for one, box in boxes.items()}
            for one in sorted(boxes):
                found[one] = tuple(two for two in sorted(boxes)
                                   if two > one and marked[one] & marked[two])
            self._over = found
        return self._over.get(word, ())

    def hiders(self, word, facts, room):
        """Every higher word whose box overlaps this one's, standing here."""
        return tuple(one for one in self.over(word)
                     if facts.get((graph.FIELD, one, ROOM_FIELD),
                                  NOWHERE) == room)

    def free(self, word, facts, room):
        """The cells of a word's box no higher word standing here covers."""
        box = self.boxes().get(word)
        if box is None:
            return set()
        cells = self.cells(box)
        for one in self.hiders(word, facts, room):
            cells -= self.cells(self.boxes()[one])
        return cells

    def buried(self, word, facts, room):
        """The higher words standing in the room that leave it no free cell."""
        if self.boxes().get(word) is None or self.free(word, facts, room):
            return ()
        return self.hiders(word, facts, room)

    def crossing(self, here, there):
        """What one room crossing cost, measured, or the median."""
        if self._words is None:
            self._words = {name: one["word"]
                           for name, one in world.ROOMS.items()}
        words = self._words
        for one in world.CROSSINGS:
            if (one["here"] == here and one["there"] == there
                    and isinstance(one["frames"], int)):
                return one["frames"]
        for edge in world.EDGES:
            if (words.get(edge["room"]) == here
                    and words.get(edge["goes"]) == there
                    and isinstance(edge["frames"], int)):
                return edge["frames"]
        return HOP_FRAMES

    def crossings(self):
        """Every action a branch of which writes the room word."""
        if self._crossings is None:
            found = []
            for one in self._graph.actions():
                for branch in one["branches"]:
                    fact, how, source = branch["effect"]
                    if (fact == (graph.HERE,) and source[0] == graph.VALUE
                            and source[1]):
                        found.append((one, branch, source[1]))
            self._crossings = tuple(found)
        return self._crossings

    def standing(self, one, facts):
        """The room an object has to be stood in to cross by its script."""
        here = facts.get(
            (graph.FIELD, one["word"], ROOM_FIELD), NOWHERE)
        return here if here > INVENTORY else None

    def hops(self, facts):
        """Every room change open in one state, as (from, to, frames)."""
        found = []
        for one, branch, there in self.crossings():
            here = self.standing(one, facts)
            if here is None or there == here:
                continue
            if not self._graph.open_gates(branch, facts):
                continue
            found.append((here, there, self.crossing(here, there)
                          + (0 if one["verb"] == WALK_ON
                             else self.price(one))))
        for one in world.WALKS:
            byte, bit = one["flag"]
            if facts.get((graph.FLAG, byte, bit), 0) == one["want"]:
                found.append((one["here"], one["there"],
                              self.crossing(one["here"], one["there"])))
        return tuple(dict.fromkeys(found))

    def far(self, facts):
        """Room to room travel, Dijkstra over the doorways that are open."""
        hops = self.hops(facts)
        if hops not in self._far:
            rooms = sorted({one for hop in hops for one in hop[:2]})
            self._far[hops] = tash.plan.graph_distances(hops, rooms)
        return self._far[hops]

    def key(self, action):
        """An action's identity: two blocks of one verb are not one action."""
        return (action["word"], action["verb"], action["target"],
                action["at"])

    def aside(self, action):
        """Whether an action is a room script or a dialogue answer."""
        return bool(action.get("room") or action.get("choice"))

    def parts(self, action):
        """The objects an action needs at hand; a room script needs none."""
        if action.get("room"):
            return ()
        if action.get("choice"):
            return (action["word"],) if action["word"] else ()
        return tuple(word for word in (action["word"], action["target"])
                     if word)

    def stands(self, action):
        """The rooms an action's own gates ask Shaggy to be standing in."""
        found = []
        for branch in action["branches"]:
            for (left, how, right), want in branch["gates"]:
                if (left == (graph.HERE,) and right[0] == graph.VALUE
                        and (how == "==") == want):
                    found.append(right[1])
        return tuple(dict.fromkeys(found))

    def where(self, action, facts):
        """The room an action is done in, or None when it cannot be."""
        if action.get("room"):
            return action["room"]
        stood = self.stands(action)
        if stood:
            return stood[0] if len(stood) == 1 else None
        rooms = []
        for word in self.parts(action):
            room = facts.get((graph.FIELD, word, ROOM_FIELD), NOWHERE)
            if room == INVENTORY:
                continue
            if room == NOWHERE:
                return None
            rooms.append(room)
        if not rooms:
            return facts[(graph.HERE,)]
        if len(set(rooms)) > 1:
            return None
        return rooms[0]

    def travel(self, action, facts):
        """The frames it costs to stand where an action can be done."""
        room = self.where(action, facts)
        if room is None:
            return None
        here = facts[(graph.HERE,)]
        if room == here:
            return 0
        got = self.far(facts).get(here, {}).get(room)
        return None if got is None or got >= tash.plan.UNREACHED else got

    def price(self, action):
        """What one action costs on screen, measured where step 1d has it."""
        if action.get("room"):
            return 0
        if self._price is None:
            self._price = {(one["spot"], one["verb"]): one["frames"]
                           for one in world.ACTIONS
                           if isinstance(one["frames"], int)}
        return self._price.get((action["word"], action["verb"]),
                               SCREEN_FRAMES)

    def makers(self, fact, want=None, avoid=()):
        """Every action whose effect can set that fact to a wanted value."""
        found = []
        for one in self._graph.actions():
            if one["verb"] in QUIET_VERBS or self.key(one) in avoid:
                continue
            for branch in one["branches"]:
                got, how, source = branch["effect"]
                if got != fact:
                    continue
                if (want is None or how != "=" or source[0] != graph.VALUE
                        or source[1] in want):
                    found.append(one)
                    break
        return tuple(found)

    def ways(self, room, facts):
        """What every shut way into a room asks for, and the ways behind."""
        here, found, seen = facts[(graph.HERE,)], [], set()
        far, queue = self.far(facts).get(here, {}), [room]
        while queue:
            want = queue.pop(0)
            if want in seen:
                continue
            seen.add(want)
            for one, branch, there in self.crossings():
                if there != want:
                    continue
                word = (graph.FIELD, one["word"], ROOM_FIELD)
                stood = facts.get(word, NOWHERE)
                if stood == NOWHERE:
                    found.append((word, SOMEWHERE))
                elif stood <= INVENTORY:
                    continue
                elif self.near(stood, here, far):
                    for gate in branch["gates"]:
                        if self._graph.holds(gate[0], facts) != gate[1]:
                            found += self.owes(gate)
                else:
                    queue.append(stood)
            for one in world.WALKS:
                if one["there"] != want:
                    continue
                flag = (graph.FLAG, one["flag"][0], one["flag"][1])
                if facts.get(flag, 0) != one["want"]:
                    found.append((flag, frozenset((one["want"],))))
                elif not self.near(one["here"], here, far):
                    queue.append(one["here"])
        return tuple(dict.fromkeys(found))

    def near(self, room, here, far):
        """Whether a walk from here reaches that room in this state."""
        return room == here or far.get(
            room, tash.plan.UNREACHED) < tash.plan.UNREACHED

    def owes(self, gate):
        """What one gate asks of each side that is not a bare number."""
        (left, how, right), want = gate
        found = []
        for side, other in ((left, right), (right, left)):
            if side[0] in (graph.VALUE, graph.HERE):
                continue
            owed = None
            if other[0] == graph.VALUE:
                if (how == "==") == want:
                    owed = frozenset((other[1],))
                elif side[0] == graph.FLAG and other[1] in (0, 1):
                    owed = frozenset((1 - other[1],))
                elif side[0] == graph.FIELD and side[2] == ROOM_FIELD:
                    owed = EVERYWHERE - frozenset((other[1],))
            found.append((side, owed))
        return found

    def aiming(self, action, aim):
        """The branches that could write one wanted fact, or all of them."""
        if aim is None:
            return action["branches"]
        fact, owed = aim
        found = [branch for branch in action["branches"]
                 if branch["effect"][0] == fact
                 and (owed is None or branch["effect"][1] != "="
                      or branch["effect"][2][0] != graph.VALUE
                      or branch["effect"][2][1] in owed)]
        return found or action["branches"]

    def made(self, before, after, aim):
        """Whether a played action left the fact it was chosen for."""
        if aim is None:
            return True
        fact, owed = aim
        got = after.get(fact, 0)
        return got != before.get(fact, 0) if owed is None else got in owed

    def wanting(self, action, facts, aim=None):
        """The unmet gates of whichever branch stands nearest to firing."""
        best = None
        for branch in self.aiming(action, aim):
            unmet = tuple(gate for gate in branch["gates"]
                          if self._graph.holds(gate[0], facts) != gate[1])
            if not unmet:
                return ()
            if best is None or len(unmet) < len(best):
                best = unmet
        return best or ()

    def reads(self, action):
        """Every fact an action could ever want, whatever the state."""
        found = []
        for gate in self._graph.needs(action):
            found += self.owes(gate)
        for word in self.parts(action):
            found.append(((graph.FIELD, word, ROOM_FIELD), None))
            found += [((graph.FIELD, one, ROOM_FIELD), None)
                      for one in self.over(word)]
        if action["target"] and not self.aside(action):
            found.append(((graph.FIELD, action["word"], ROOM_FIELD),
                          frozenset((INVENTORY,))))
        return tuple(dict.fromkeys(found))

    def needed(self, action, facts=None, aim=None):
        """The facts an action reads or needs present, and what they owe."""
        facts = self.state() if facts is None else facts
        found = []
        for gate in self.wanting(action, facts, aim):
            found += self.owes(gate)
        room = self.where(action, facts)
        for word in self.parts(action):
            at = facts.get((graph.FIELD, word, ROOM_FIELD), NOWHERE)
            if at == NOWHERE:
                found.append(((graph.FIELD, word, ROOM_FIELD), SOMEWHERE))
            elif room is not None and at not in (INVENTORY, room):
                found.append(((graph.FIELD, word, ROOM_FIELD),
                              frozenset((INVENTORY,))))
            elif room is not None:
                found += [((graph.FIELD, one, ROOM_FIELD),
                           EVERYWHERE - frozenset((room,)))
                          for one in self.buried(word, facts, room)]
        if room is not None and self.travel(action, facts) is None:
            found += self.ways(room, facts)
        if action["target"] and not self.aside(action):
            held = (graph.FIELD, action["word"], ROOM_FIELD)
            if facts.get(held, NOWHERE) != INVENTORY:
                found.append((held, frozenset((INVENTORY,))))
        return tuple(dict.fromkeys(found))

    def relevant(self):
        """The backward closure over what an action can ever want."""
        if self._relevant is None:
            found, queue = {}, [self._graph.goal()]
            queue += [one for one, branch, there in self.crossings()]
            for one in world.WALKS:
                queue += list(self.makers((graph.FLAG,) + one["flag"],
                                          frozenset((one["want"],))))
            while queue:
                one = queue.pop()
                if self.key(one) in found:
                    continue
                found[self.key(one)] = one
                for fact, want in self.reads(one):
                    queue += list(self.makers(fact, want))
            self._relevant = tuple(found.values())
        return self._relevant

    def won(self, facts):
        return facts.get((graph.HERE,)) == graph.ENDING_ROOM

    def athand(self, action, facts, room):
        """Whether every object an action clicks is in reach and not buried."""
        for word in self.parts(action):
            at = facts.get((graph.FIELD, word, ROOM_FIELD), NOWHERE)
            if at not in (INVENTORY, room):
                return False
            if at == room and self.buried(word, facts, room):
                return False
        return True

    def stood(self, action, facts):
        """The state walking to the room an action is done in leaves."""
        room = self.where(action, facts)
        if room is None or not self.athand(action, facts, room):
            return None
        if self.travel(action, facts) is None:
            return None
        walk = self.route(facts[(graph.HERE,)], room, facts)
        if walk is None:
            return None
        after = dict(facts)
        for here, there in zip(walk, walk[1:]):
            after = self.walked(here, there, after)
        after[(graph.HERE,)] = room
        return after

    def arrived(self, room, facts):
        """The entry script a room runs the moment Shaggy stands in it."""
        after = dict(facts)
        for one in self._graph.arrivals().get(room, ()):
            after = self._graph.run(one, after)
        return after

    def played(self, action, facts):
        """One action run, with the entry script of any room it lands in."""
        after = self._graph.run(action, facts)
        here = after.get((graph.HERE,))
        if here != facts.get((graph.HERE,)):
            after = self.arrived(here, after)
        return after

    def step(self, action, facts):
        """One action played symbolically, where the gates want it played."""
        before = self.stood(action, facts)
        if before is None:
            return None
        return (self.travel(action, facts) + self.price(action),
                self.played(action, before))

    def route(self, here, room, facts):
        """The rooms a walk passes through, the one stood in first."""
        if here == room:
            return (here,)
        try:
            got = tash.plan.graph(self.hops(facts), here, room)
        except Exception:
            return None
        return tuple(got["nodes"])

    def crosser(self, here, there, facts):
        """The action whose open branch crosses between two rooms."""
        for one, branch, goes in self.crossings():
            if goes != there or self.standing(one, facts) != here:
                continue
            if self._graph.open_gates(branch, facts):
                return one
        return None

    def walked(self, here, there, facts):
        """One crossing played where it stands, so its script writes too."""
        after, one = dict(facts), self.crosser(here, there, facts)
        if one is not None:
            after[(graph.HERE,)] = here
            after = self._graph.run(one, after)
        after[(graph.HERE,)] = there
        return self.arrived(there, after)

    def weigh(self, action):
        """How hard an action is: its unmet gates first, then its frames."""
        return (len(self.needed(action)), self.price(action))

    def reaches(self, pairs, one, two):
        """Whether the precedence already runs from one action to another."""
        seen, queue = {one}, [one]
        while queue:
            at = queue.pop()
            if at == two:
                return True
            for first, then in pairs:
                if first == at and then not in seen:
                    seen.add(then)
                    queue.append(then)
        return False

    def heaped(self, fact, makers):
        """A fact piled up bit by bit: one maker will not make it."""
        return any(branch["effect"][0] == fact and branch["effect"][1] != "="
                   for one in makers for branch in one["branches"])

    def spine(self):
        """The required actions and their precedence, chained back."""
        if self._spine is None:
            goal = self._graph.goal()
            found, pairs, queue = {self.key(goal): goal}, [], [goal]
            while queue:
                one = queue.pop()
                for fact, want in self.needed(one):
                    makers = self.makers(fact, want,
                                         (self.key(one), self.key(goal)))
                    if not makers:
                        continue
                    had = [two for two in makers
                           if self.key(two) in found]
                    if self.heaped(fact, makers):
                        had = list(makers)
                    elif not had:
                        had = [min(makers, key=self.weigh)]
                    for two in had:
                        if self.key(two) not in found:
                            found[self.key(two)] = two
                            queue.append(two)
                        if not self.reaches(pairs, self.key(one),
                                            self.key(two)):
                            pairs.append((self.key(two), self.key(one)))
            self._spine = (tuple(found.values()),
                           tuple(dict.fromkeys(pairs)))
        return self._spine

    def chain(self):
        """The actions the ending needs, the goal first."""
        return self.spine()[0]

    def before(self):
        """The precedence pairs those actions impose on each other."""
        return self.spine()[1]

    def sequence(self):
        """The spine's actions in an order its precedence allows."""
        pairs, left = self.before(), [self.key(one) for one in self.chain()]
        out = []
        while left:
            free = [key for key in left
                    if not any(two in left
                               for two, then in pairs if then == key)]
            out += free
            left = [key for key in left if key not in free]
        return tuple(out)

    def lands(self, one, facts):
        """An action played where it stands, when it changes anything."""
        before = self.stood(one, facts)
        if before is None:
            return None
        after = self.played(one, before)
        if after == before and not self.won(after):
            return None
        return (self.travel(one, facts) + self.price(one), after)

    def handy(self, action, facts, aim=None):
        """How near an action is here: unmet needs first, then its frames."""
        cost = self.travel(action, facts)
        return (len(self.needed(action, facts, aim)),
                self.price(action) + (SCREEN_FRAMES if cost is None
                                      else cost))

    def bring(self, want, acts, facts, watch, depth, path=(), aim=None):
        """Everything one action needs, made true here, then the action."""
        stamp = (want, aim, depth, frozenset(facts.items()))
        if stamp in self._failed:
            return None
        done, cost, first = (), 0, facts
        for _ in range(ROUNDS):
            got = self.lands(acts[want], facts)
            if got is not None and self.made(first, got[1], aim):
                return (done + (want,), cost + got[0], got[1])
            if depth <= 0 or watch.spent():
                return None
            step = self.someway(want, acts, facts, watch, depth, path, aim)
            if step is None:
                return self.blame(stamp, watch)
            done, cost, facts = done + step[0], cost + step[1], step[2]
        return self.blame(stamp, watch)

    def blame(self, stamp, watch):
        """A chain that failed once fails again from the same state."""
        if not watch.spent():
            self._failed.add(stamp)
        return None

    def someway(self, want, acts, facts, watch, depth, path, aim=None):
        """One unmet need of an action, met by the handiest maker there is."""
        for fact, owed in self.needed(acts[want], facts, aim):
            makers = [one for one in self.makers(fact, owed, path + (want,))
                      if self.key(one) in acts and self.key(one) not in path]
            for maker in sorted(makers, key=lambda one: self.handy(
                    one, facts, (fact, owed))):
                watch.tick()
                for goal in ((fact, owed), None):
                    step = self.bring(self.key(maker), acts, facts, watch,
                                      depth - 1, path + (want,), goal)
                    if step is not None:
                        return step
        return None

    def search(self, limit=EXPANSIONS, seconds=SECONDS):
        """Goal regression from the ending, the spine's order as the hint."""
        acts = {self.key(one): one for one in self.relevant()}
        watch, self._failed = budget.Budget(limit, seconds), set()
        facts, done, cost = dict(self.state()), (), 0
        for want in self.sequence():
            if want not in acts or watch.spent():
                continue
            got = self.bring(want, acts, facts, watch, CHAIN_DEPTH)
            if got is None:
                continue
            done, cost, facts = done + got[0], cost + got[1], got[2]
        return (done, cost) if self.won(facts) else (None, None)

    def acted(self):
        """Every action the graph holds, by the key the plan names it with."""
        return {self.key(one): one for one in self._graph.actions()}

    def order(self):
        """The computed completion path, as keys with predicted frames."""
        found, _ = self.search()
        if found is None:
            return ()
        acts = {self.key(one): one for one in self._graph.actions()}
        facts, out = dict(self.state()), []
        for key in found:
            spent, facts = self.step(acts[key], facts)
            out.append((key, spent))
        return tuple(out)
