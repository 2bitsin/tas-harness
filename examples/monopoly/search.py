"""A beam over turn pairs of the model: the fewest frames to the fold."""


import model

SEAT = 0
RIVAL = 1
SQUARES = 40
MIN_TOTAL = 3
MAX_TOTAL = 35
DOUBLE_SIX = (6, 6)
DOUBLE_ONE = (1, 1)

# The frames the cost table in the README measures, rounded to a turn: our
# turn, the rival's, one more roll inside a turn, and a house through
# OPTIONS, BUILDINGS.
OUR_TURN = 790
RIVAL_TURN = 585
EXTRA_ROLL = 300
BUILD_BASE = 396
BUILD_HOUSE = 66
PLATE = 775

OUR_WIDTH = 14
RIVAL_WIDTH = 11
BEAM = 40
DEPTH = 14
TAX_HIT = 200
RESERVE = 60
GROUP_STEP = 60
COMPLETE_BONUS = 400
PRICE_SHARE = 4


def pair(total):
    """A pair of dice that is not a double and adds to that total."""
    for first in range(1, 7):
        second = total - first
        if 1 <= second <= 6 and second != first:
            return (first, second)
    return None


def sequence(total):
    """The shortest run of rolls that walks the token that many squares."""
    if MIN_TOTAL <= total <= 11:
        return [pair(total)]
    for lead in (DOUBLE_SIX, DOUBLE_ONE):
        rest = total - sum(lead)
        if MIN_TOTAL <= rest <= 11:
            return [lead, pair(rest)]
    for first in (DOUBLE_SIX, DOUBLE_ONE):
        for second in (DOUBLE_SIX, DOUBLE_ONE):
            rest = total - sum(first) - sum(second)
            if MIN_TOTAL <= rest <= 11:
                return [first, second, pair(rest)]
    return None


class Search:
    """Turn pairs expanded over the model, the cheapest line kept."""

    def __init__(self, table, beam=BEAM, depth=DEPTH):
        self.table = table
        self.beam = beam
        self.depth = depth
        self.nodes = 0
        self.expanded = 0

    def totals(self):
        return [total for total in range(MIN_TOTAL, MAX_TOTAL + 1)
                if sequence(total)]

    def hit(self, board, seat):
        """The biggest bill one landing can put on that seat right now."""
        other = 1 - seat
        best = TAX_HIT
        for square in board.owned_by(other):
            if not board.mortgaged[square]:
                best = max(best, board.rent(square, 7))
        return best

    def value(self, board):
        """What is left to take off the rival before the game folds it."""
        return board.liquid(RIVAL)

    def estimate(self, board):
        left = self.value(board)
        if left <= 0:
            return 0
        turns = -(-left // max(self.hit(board, RIVAL), 1))
        return turns * (OUR_TURN + RIVAL_TURN)

    def deed_gain(self, board, square):
        """A deed is worth the monopoly it brings, not the price it costs."""
        group = self.table.group(square)
        squares = [at for at in range(SQUARES)
                   if self.table.group(at) == group]
        mine = board.count_of(SEAT, squares) + 1
        gain = GROUP_STEP * mine
        if squares and mine == len(squares):
            gain += COMPLETE_BONUS
        return gain - self.table.price(square) // PRICE_SHARE

    def our_options(self, board):
        """Rank the squares we could reach by what landing there is worth."""
        out = []
        for total in self.totals():
            rolls = sequence(total)
            where = (board.square[SEAT] + total) % SQUARES
            gain = 0
            if board.owner[where] == model.BANK and self.table.buyable(where):
                gain = self.deed_gain(board, where)
            if board.owner[where] == RIVAL:
                gain = -board.rent(where, total)
            if where == model.JAIL_GATE:
                gain = -TAX_HIT
            out.append((gain - EXTRA_ROLL * (len(rolls) - 1) // 100,
                        total, rolls))
        out.sort(key=lambda row: -row[0])
        return [(total, rolls) for _, total, rolls in out[:OUR_WIDTH]]

    def rival_options(self, board):
        """Where the rival can be sent, by what it costs the rival."""
        out = []
        for total in range(MIN_TOTAL, 12):
            where = (board.square[RIVAL] + total) % SQUARES
            cost = 0
            if board.owner[where] == SEAT and not board.mortgaged[where]:
                cost = board.rent(where, total)
            elif (board.owner[where] == model.BANK
                  and self.table.buyable(where)):
                cost = self.table.price(where) // 2
            elif where == model.INCOME_TAX:
                cost = min(model.INCOME_FLAT,
                           board.worth(RIVAL) // model.INCOME_SHARE)
            elif where in model.TAXES:
                cost = model.TAXES[where]
            out.append((cost, total, [pair(total)]))
        out.sort(key=lambda row: -row[0])
        return [(total, rolls) for _, total, rolls in out[:RIVAL_WIDTH]]

    def build_options(self, board):
        """Nothing, one round of houses, or as many as the purse bears."""
        out = [(None, 0)]
        for group in ("brown", "light-blue", "pink", "orange", "red",
                      "yellow", "green", "dark-blue"):
            squares = self.table.squares_of(group)
            if not squares or any(board.owner[at] != SEAT for at in squares):
                continue
            room = sum(model.HOTEL - board.houses[at] for at in squares)
            price = self.table.house_price(squares[0])
            afford = max(0, (board.cash[SEAT] - RESERVE) // price)
            most = min(room, afford)
            for count in {len(squares), most}:
                if 0 < count <= most:
                    out.append((group, count))
        return out

    def child(self, node, ours, built, rival, spent):
        """One turn pair applied to a board our own turn already moved."""
        board = ours["board"].clone()
        frames = node["frames"] + spent
        group, count = built
        if group and not board.build(SEAT, group, count):
            return None
        if group:
            frames += BUILD_BASE + BUILD_HOUSE * count
        if not board.out[RIVAL]:
            board.turn(RIVAL, rival[1])
            frames += RIVAL_TURN
        self.nodes += 1
        step = {"ours": ours["total"], "rolls": ours["rolls"],
                "buy": ours["buy"], "build": built, "theirs": rival[0],
                "theirs_rolls": rival[1], "cash": list(board.cash),
                "square": list(board.square), "liquid": board.liquid(RIVAL)}
        return {"board": board, "frames": frames,
                "line": node["line"] + [step]}

    def our_turns(self, board):
        """Every first half of a turn pair: where we land and what we take."""
        out = []
        for total, rolls in self.our_options(board):
            where = (board.square[SEAT] + total) % SQUARES
            takes = ((True, False)
                     if board.owner[where] == model.BANK
                     and self.table.buyable(where) else (True,))
            for buy in takes:
                ahead = board.clone()
                ahead.turn(SEAT, rolls, [buy] * (len(rolls) + 1))
                if ahead.out[SEAT]:
                    continue
                out.append({"total": total, "rolls": rolls, "buy": buy,
                            "board": ahead,
                            "spent": OUR_TURN
                            + EXTRA_ROLL * (len(rolls) - 1)})
        return out

    def expand(self, node):
        children = []
        for ours in self.our_turns(node["board"]):
            for built in self.build_options(ours["board"]):
                for rival in self.rival_options(ours["board"]):
                    made = self.child(node, ours, built, rival,
                                      ours["spent"])
                    if made:
                        children.append(made)
        self.expanded += 1
        return children

    def root_children(self, board):
        node = {"board": board.clone(), "frames": 0, "line": []}
        children = self.expand(node)
        children.sort(key=self.rank)
        return children

    def rank(self, node):
        return node["frames"] + self.estimate(node["board"])

    def run(self, board):
        """Expand turn pairs by the beam until the rival is out of money."""
        level = [{"board": board.clone(), "frames": 0, "line": []}]
        best = None
        for _ in range(self.depth):
            found = []
            for node in level:
                for child in self.expand(node):
                    if child["board"].out[RIVAL]:
                        if best is None or child["frames"] < best["frames"]:
                            best = child
                        continue
                    found.append(child)
            if best:
                return best
            if not found:
                break
            found.sort(key=self.rank)
            level = found[:self.beam]
        return best or (level[0] if level else None)

    def report(self):
        return {"nodes": self.nodes, "expanded": self.expanded}
