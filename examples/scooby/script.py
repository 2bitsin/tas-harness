"""The cartridge's script VM, decoded: what each object's script does."""

import world

# Instruction lengths read off each handler's a5 adjust, dispatch table
# at rom 0x2354; the block ops carry their body inside their length, and
# opcode 33 carries animation data, which is not code and is not walked.
FIXED = {0: 2, 5: 6, 6: 6, 7: 4, 8: 0xc, 9: 6, 10: 6, 11: 6, 12: 4,
         13: 4, 14: 0xa, 15: 0xc, 16: 8, 17: 8, 18: 6, 20: 6, 21: 4,
         22: 4, 23: 4, 24: 8, 25: 4, 26: 0xa, 27: 0xc, 28: 4, 29: 4,
         30: 6, 31: 4, 32: 4}
BLOCK = {1: (8, 6), 2: (0xe, 0xa), 3: (0x10, 2), 4: (0x10, 2),
         19: (4, 2)}
SPAWN, SPAWN_HEAD, SPAWN_LENGTH = 33, 8, 2
ENTRY_SCRIPT, FRAME_SCRIPT, ROOM_HEADER, LONG = 0xc, 0x10, 4, 4
ON_VERB, VERB_AT, TARGET_AT = 1, 2, 4
GO = (17, 24)
PUT = 5
WALK_ON = 11
WORD = 2
DEPTH_LIMIT = 12


class Script:
    def __init__(self, run):
        self._run = run
        self._code = None
        self._table = None
        self._gives = None
        self._byroom = None

    def _bytes(self):
        if self._code is None:
            span = world.SCRIPT_END - world.SCRIPT_BASE
            self._code = bytes(self._run.memory(
                world.CART, world.SCRIPT_BASE, span))
        return self._code

    def _word(self, at):
        raw = self._bytes()
        return int.from_bytes(raw[at:at + WORD], "big")

    def _offsets(self):
        if self._table is None:
            count = world.OBJECT_LAST - world.OBJECT_FIRST + 1
            span = (count + world.OBJECT_FIRST) * world.OBJECT_ENTRY
            raw = bytes(self._run.memory(
                world.CART, world.OBJECT_TABLE, span))
            self._table = {}
            for word in range(world.OBJECT_FIRST, world.OBJECT_LAST):
                at = word * world.OBJECT_ENTRY + world.OBJECT_SCRIPT
                self._table[word] = int.from_bytes(raw[at:at + 4], "big")
        return self._table

    def rooms(self):
        """Every room script: its room, whether per frame, start, length."""
        if self._byroom is None:
            found = []
            for room in range(world.ROOM_FIRST, world.ROOM_LAST + 1):
                at = world.ROOM_TABLE + room * world.ROOM_ENTRY
                raw = bytes(self._run.memory(world.CART, at,
                                             world.ROOM_ENTRY))
                for which in (ENTRY_SCRIPT, FRAME_SCRIPT):
                    start = int.from_bytes(raw[which:which + LONG], "big")
                    if not start or start >= len(self._bytes()):
                        continue
                    found.append((room, which == FRAME_SCRIPT,
                                  start + ROOM_HEADER, self._word(start)))
            self._byroom = tuple(found)
        return self._byroom

    def span(self, word):
        """Script start and length, both relative to the script base."""
        start = self._offsets().get(word)
        if start is None or start >= world.SCRIPT_END - world.SCRIPT_BASE:
            return None
        return (start + world.SCRIPT_HEADER, self._word(start))

    def _size(self, at):
        op = self._word(at)
        if op in BLOCK:
            head, where = BLOCK[op]
            return self._word(at + where), head
        if op == SPAWN:
            return (SPAWN_HEAD + self._word(at + SPAWN_LENGTH), None)
        return FIXED.get(op), None

    def walk(self, word):
        """Every instruction of one script, as (depth, at, op, length)."""
        found = self.span(word)
        if found is None:
            return ()
        return self.reach(*found)

    def reach(self, start, length):
        """Every instruction of one span, as (depth, at, op, length)."""
        out = []
        self._walk(start, start + length, out, 0)
        return tuple(out)

    def _walk(self, at, stop, out, depth):
        while at < stop:
            total, head = self._size(at)
            if not total or at + total > stop or depth > DEPTH_LIMIT:
                return False
            out.append((depth, at, self._word(at), total))
            if head is not None and total > head:
                self._walk(at + head, at + total, out, depth + 1)
            at += total
        return True

    def verbs(self, word):
        """Verbs the object answers, each with the second object it wants."""
        return tuple((self._word(at + VERB_AT), self._word(at + TARGET_AT))
                     for depth, at, op, total in self.walk(word)
                     if op == ON_VERB)

    def goes(self, word):
        """Room changes, each with the verb whose block holds it."""
        out, seen = [], {}
        for depth, at, op, total in self.walk(word):
            if op == ON_VERB:
                seen[depth] = self._word(at + VERB_AT)
            if op in GO:
                above = [d for d in seen if d < depth]
                verb = seen.get(max(above)) if above else None
                out.append((verb, self._word(at + WORD)))
        return tuple(out)

    def givers(self):
        """Which object and verb puts each object into a room."""
        if self._gives is None:
            found = {}
            for word in self._offsets():
                for one, verb, room in self._moves(word):
                    found.setdefault(one, []).append((word, verb, room))
            self._gives = {one: tuple(rows) for one, rows in found.items()}
        return self._gives

    def _moves(self, word):
        """Every put this script makes, with the verb whose block holds it."""
        out, seen = [], {}
        for depth, at, op, total in self.walk(word):
            if op == ON_VERB:
                seen[depth] = self._word(at + VERB_AT)
            if op == PUT:
                above = [one for one in seen if one < depth]
                verb = seen.get(max(above)) if above else None
                out.append((self._word(at + WORD), verb,
                            self._word(at + TARGET_AT)))
        return tuple(out)

    def puts(self, word):
        """Objects this script moves, as (object, room)."""
        return tuple((self._word(at + WORD), self._word(at + TARGET_AT))
                     for depth, at, op, total in self.walk(word)
                     if op == PUT)
