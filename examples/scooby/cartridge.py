"""The rom's own name and box tables, read through the cartridge region."""

import world

REGION = "cartridge"
LONG = 4
NAME_LIMIT = 64


class Cartridge:
    """Every name and box the rom holds, read once and kept."""

    def __init__(self, run):
        self._run = run
        self._names = None
        self._rooms = None
        self._boxes = None

    def long(self, address):
        raw = self._run.memory(REGION, address, LONG)
        return int.from_bytes(raw, "big")

    def text(self, offset, limit=NAME_LIMIT):
        """One null-terminated string, at an offset into the string block."""
        if not 0 < offset < world.STRINGS_END - world.STRINGS:
            return ""
        line = self._run.string(REGION, world.STRINGS + offset, limit)
        end = line.find("\0")
        return (line if end < 0 else line[:end]).strip()

    def names(self):
        """Every object's name, by the word the hovered object reads."""
        if self._names is None:
            self._names = {}
            for word in range(world.OBJECT_FIRST, world.OBJECT_LAST + 1):
                name = self.text(self.long(world.OBJECT_TABLE
                                           + word * world.OBJECT_ENTRY))
                if name:
                    self._names[word] = name
        return self._names

    def name(self, word):
        return self.names().get(word, "")

    def rooms(self):
        """Every room's banner, by the word at 0x6ac."""
        if self._rooms is None:
            self._rooms = {}
            for word in range(world.ROOM_FIRST, world.ROOM_LAST + 1):
                name = self.text(self.long(world.ROOM_TABLE
                                           + word * world.ROOM_ENTRY))
                if name:
                    self._rooms[word] = name
        return self._rooms

    def boxes(self):
        """Every hotspot box, four words of eight-pixel cells."""
        if self._boxes is None:
            found = []
            for index in range(world.BOX_COUNT):
                at = world.BOX_BASE + self.long(world.BOX_INDEX + index * LONG)
                raw = self._run.memory(REGION, at, world.BOX_WORDS * 2)
                found.append(tuple(
                    int.from_bytes(raw[n * 2:n * 2 + 2], "big")
                    for n in range(world.BOX_WORDS)))
            self._boxes = tuple(found)
        return self._boxes

    def box(self, index):
        """The box a record's one-based box number names."""
        boxes = self.boxes()
        return boxes[index - 1] if 0 < index <= len(boxes) else None
