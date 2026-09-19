"""The Sardaukar as state: every record where it stands and the tile it
steps onto next, the waves counted off their first move toward my yard,
and what stood beside each building of mine that fell."""

import state

# A record walks a tile at a time: +126 and +128 hold the tile it is
# stepping onto, y then x, centred as the position words are, and both
# read 0 on the frames between two steps.
STEP_Y = 126
STEP_X = 128
LAST_ORDER = 0xc188

WORM = 25
# Only these make units; a record that appears beside a rocket turret was
# not made by it.
MAKER_KINDS = {2, 3, 4, 5, 7, 10, 11, 12}
MAKER_REACH = 6
WAVE_GAP = 3000
KILLERS_KEPT = 3


def steps(here, there):
    return abs(here[0] - there[0]) + abs(here[1] - there[1])


def stepping(raw):
    """The tile a record is stepping onto, or None between two steps."""
    down = state.record_word(raw, STEP_Y)
    across = state.record_word(raw, STEP_X)
    if not down and not across:
        return None
    return across >> state.TILE_SHIFT, down >> state.TILE_SHIFT


def heading(row):
    step = row["step"]
    if step is None:
        return None
    return step[0] - row["tile"][0], step[1] - row["tile"][1]


def sardaukar(table):
    return {slot: rec for slot, rec in table.items()
            if rec["owner"] == state.SARDAUKAR}


class Army:
    """The enemy of one yard, polled: the units, the waves and the kills."""

    def __init__(self, yard):
        self.yard = yard
        self.rows = {}
        self.worms = {}
        self.kills = []
        self.standing = {}
        self.stood = {}
        self.polls = 0

    def born(self, slot, rec, shot, frame):
        made = self.maker(shot, rec["tile"]) if self.polls else None
        return {"slot": slot, "kind": rec["name"], "from": rec["tile"],
                "seen": frame, "maker": made,
                "reach": steps(rec["tile"], self.yard), "moved": None,
                "at": None, "tile": rec["tile"], "step": None,
                "trail": [(frame, rec["tile"])],
                "order": rec.get("order"), "health": rec["health"],
                "gone": None}

    def maker(self, shot, tile):
        """The Sardaukar building nearest a record's first tile, if near."""
        near = sorted((steps(rec["tile"], tile), slot, rec["name"])
                      for slot, rec in sardaukar(shot.buildings).items()
                      if rec["kind"] in MAKER_KINDS)
        if not near or near[0][0] > MAKER_REACH:
            return None
        return {"slot": near[0][1], "kind": near[0][2], "reach": near[0][0]}

    def read(self, shot, frame):
        """Every Sardaukar record: new ones opened, the rest brought up."""
        theirs = sardaukar(shot.units)
        for slot, rec in theirs.items():
            row = self.rows.get(slot)
            if row is None or row["gone"] is not None:
                row = self.born(slot, rec, shot, frame)
                self.rows[slot] = row
            reach = steps(rec["tile"], self.yard)
            if row["moved"] is None and reach < row["reach"]:
                row["moved"], row["at"] = frame, rec["tile"]
            row["reach"] = min(row["reach"], reach)
            if rec["tile"] != row["tile"]:
                row["trail"].append((frame, rec["tile"]))
            row["tile"] = rec["tile"]
            row["step"] = stepping(rec["raw"])
            row["order"] = rec.get("order")
            row["health"] = rec["health"]
        for slot, row in self.rows.items():
            if slot not in theirs and row["gone"] is None:
                row["gone"] = frame
        return theirs

    def hazard(self, shot, frame):
        """The sandworm is nobody's unit: a kind, a tile and a last frame."""
        for slot, rec in shot.units.items():
            if rec["kind"] != WORM:
                continue
            row = self.worms.setdefault(
                slot, {"slot": slot, "owner": rec["owner"], "surfaced": 0,
                       "from": rec["tile"], "seen": frame, "track": []})
            if not row["track"] or row["track"][-1][1] != rec["tile"]:
                row["track"].append((frame, rec["tile"]))
            row["tile"] = rec["tile"]
            row["last"] = frame
            row["surfaced"] += 1
        return self.worms

    def watch_kills(self, shot, frame, house, theirs):
        """What of mine went, and the enemy records nearest it before it."""
        mine = {slot: (rec["name"], rec["tile"])
                for slot, rec in shot.mine(shot.buildings, house).items()}
        for slot, (name, tile) in self.standing.items():
            if slot in mine:
                continue
            near = sorted((steps(at, tile), was, self.rows[was]["kind"])
                          for was, at in self.stood.items())
            self.kills.append({"frame": frame, "tile": tile, "what": name,
                               "killers": near[:KILLERS_KEPT]})
        self.standing = mine
        self.stood = {slot: rec["tile"] for slot, rec in theirs.items()}

    def poll(self, shot, frame, house):
        """One frame of the enemy, from the tables the snapshot already has."""
        theirs = self.read(shot, frame)
        self.hazard(shot, frame)
        self.watch_kills(shot, frame, house, theirs)
        self.polls += 1
        return theirs

    def moved(self):
        left = [row for row in self.rows.values()
                if row["moved"] is not None]
        return sorted(left, key=lambda row: row["moved"])

    def waves(self, gap=WAVE_GAP):
        """Group the first moves: when a wave leaves, its size, its makers."""
        out = []
        for row in self.moved():
            if not out or row["moved"] - out[-1]["last"] > gap:
                out.append({"frame": row["moved"], "last": row["moved"],
                            "slots": [], "kinds": {}, "from": {}})
            wave = out[-1]
            wave["last"] = row["moved"]
            wave["slots"].append(row["slot"])
            wave["kinds"][row["kind"]] = wave["kinds"].get(row["kind"], 0) + 1
            made = row["maker"]
            name = f"{made['kind']} {made['slot']}" if made else "the opening"
            wave["from"][name] = wave["from"].get(name, 0) + 1
            wave["at"] = row["at"]
        for wave in out:
            wave["size"] = len(wave["slots"])
        return out

    def period(self, gap=WAVE_GAP):
        frames = [wave["frame"] for wave in self.waves(gap)]
        return [b - a for a, b in zip(frames, frames[1:])]

    def approach(self):
        """The tiles the waves that have already left came over."""
        return [row["at"] for row in self.moved() if row["at"]]
