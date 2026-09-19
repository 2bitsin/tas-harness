"""Twenty turns with every prompt answered from the ram it is written in."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import board
import engine
import model
import state

TAPE = "examples/monopoly/tapes/title-to-first-roll.yaml"
CHECKPOINT = "after-tape-fast"
TURNS = 20
SEAT = 0
RIVAL = 1
RESERVE = 200
AUCTION_SHARE = 2
ANSWERS = 60
TURN_FRAMES = 6000
BUILD_FLOOR = 600
BUILD_MAX = 4
FACES = range(1, 7)


class Turns:
    """One seat's player, the ledger, and the model run beside the machine."""

    def __init__(self, run, deeds):
        self.run = run
        self.deeds = deeds
        self.model = model.Model(deeds)
        self.rows = []
        self.gaps = []
        self.pending = []

    def prompt(self):
        """Name the screen the game is waiting on from the state alone."""
        run = self.run
        if state.current(run) != SEAT:
            return "opponent"
        if engine.auction_prompt(run):
            return "auction"
        if engine.deed_prompt(run):
            return "deed"
        if engine.jailed(run):
            return "jail"
        if engine.roll_prompt(run):
            return "roll"
        return "notice"

    def answer(self, name, bought):
        run = self.run
        if name == "auction":
            square = state.word(run, engine.AUCTION_SQUARE)
            engine.auction(run, self.deeds.price(square) // AUCTION_SHARE)
        elif name == "deed":
            square = state.player(run, SEAT)["square"]
            cash = state.player(run, SEAT)["cash"]
            take = self.deeds.price(square) + RESERVE <= cash
            bought.append(take)
            engine.buy(run) if take else engine.decline(run)
        elif name in ("roll", "jail"):
            engine.roll(run)
        else:
            engine.acknowledge(run)

    def build(self):
        """Spend a surplus on the cheapest monopoly the seat holds whole."""
        run = self.run
        cash = state.player(run, SEAT)["cash"]
        for group in state.monopolies(run, SEAT):
            price = state.HOUSE_PRICE[group]
            count = min(BUILD_MAX, (cash - BUILD_FLOOR) // price)
            if count > 0:
                self.model.build(SEAT, group, count)
                return group, engine.build(run, group, count)
        return None, None

    def turn(self, index):
        run = self.run
        start = run.frames()
        seat = state.current(run)
        raised, bought = [], []
        rolls, self.pending = self.pending, []
        seen = [state.dice(run)]
        for _ in range(ANSWERS):
            if engine.decided(run) or run.frames() - start > TURN_FRAMES:
                break
            engine.settle(run)
            self.note(rolls, seen, seat)
            if state.current(run) != seat:
                break
            name = self.prompt()
            raised.append(name)
            self.answer(name, bought)
            self.note(rolls, seen, seat)
        row = {
            "turn": index,
            "seat": seat,
            "frames": run.frames() - start,
            "cash": [state.player(run, s)["cash"] for s in (SEAT, RIVAL)],
            "squares": [state.player(run, s)["square"] for s in (SEAT, RIVAL)],
            "owned": [len(state.owned_by(run, s)) for s in (SEAT, RIVAL)],
            "prompts": raised,
            "rolls": rolls,
        }
        row["events"] = self.model.turn(seat, rolls, bought)
        self.rows.append(row)
        return row

    def note(self, rolls, seen, seat):
        """A roll is the dice changing to faces; the mover then owns it."""
        now = state.dice(self.run)
        if now != seen[0] and all(face in FACES for face in now):
            here = rolls if state.current(self.run) == seat else self.pending
            here.append(now)
        seen[0] = now

    def check(self, row):
        """The model against the machine, then the model read back straight."""
        said = {"cash": list(self.model.cash),
                "squares": list(self.model.square)}
        gap = {"turn": row["turn"], "seat": row["seat"],
               "rolls": row["rolls"], "said": said,
               "got": {"cash": row["cash"], "squares": row["squares"]},
               "events": row["events"]}
        agreed = (said["cash"] == row["cash"]
                  and said["squares"] == row["squares"])
        if not agreed:
            self.gaps.append(gap)
            print("gap", gap, flush=True)
        self.model.read(self.run)
        return agreed

    def play(self, turns=TURNS):
        for index in range(turns):
            row = self.turn(index)
            self.run.mark("turn %d seat %d" % (index, row["seat"]), "turns")
            row["agreed"] = self.check(row)
            if row["seat"] == SEAT:
                group, built = self.build()
                if group:
                    row["built"] = (group, built)
                    self.model.read(self.run)
            print("turn %2d seat %d frames %5d cash %6d/%-6d sq %2d/%-2d "
                  "owned %d/%d model %s rolls %s %s"
                  % (row["turn"], row["seat"], row["frames"],
                     row["cash"][0], row["cash"][1], row["squares"][0],
                     row["squares"][1], row["owned"][0], row["owned"][1],
                     "ok" if row["agreed"] else "GAP", row["rolls"],
                     " ".join(row["prompts"])), flush=True)
            if engine.decided(self.run):
                break
        return self.rows


def begin(run):
    """No restore_or_play in the scenario api, so the fallback is spelt out."""
    try:
        run.restore(CHECKPOINT)
    except RuntimeError:
        print("tape", run.play(TAPE))
        run.checkpoint(CHECKPOINT)
    run.release(engine.PORT)


def main(run):
    begin(run)
    table = board.Board(run)
    player = Turns(run, table)
    player.model.read(run)
    rows = player.play()
    mine = [r for r in rows if r["seat"] == SEAT]
    theirs = [r for r in rows if r["seat"] == RIVAL]
    for name, part in (("seat 0", mine), ("seat 1", theirs)):
        if part:
            frames = [r["frames"] for r in part]
            print("%s: %d turns, %d frames, %d..%d, mean %d"
                  % (name, len(part), sum(frames), min(frames), max(frames),
                     sum(frames) // len(frames)))
    agreed = sum(1 for r in rows if r["agreed"])
    print("model agreed on %d of %d turns" % (agreed, len(rows)), flush=True)
    run.expect("twenty turns were played", len(rows) >= TURNS)
    run.expect("both seats are still in the game",
               "watch players_left equal 2")
    run.expect("the model tracks the machine's purses and squares",
               agreed == len(rows), "%d of %d turns agreed, %d gaps"
               % (agreed, len(rows), len(player.gaps)), raises=False)
    run.judge("every prompt was answered from the state", True,
              "%d turns, %d frames" % (len(rows), sum(r["frames"]
                                                     for r in rows)))
    print(run.report())


main(tash.run)
