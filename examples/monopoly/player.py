"""The line: the model plans, the emulator judges, the rival goes broke."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import board
import dice
import engine
import model
import search
import state

TAPE = "examples/monopoly/tapes/title-to-first-roll.yaml"
CHECKPOINT = "after-tape-fast"
SEAT = 0
RIVAL = 1
ANSWERS = 60
TURN_FRAMES = 9000
PAIRS = 40
IDLES = 14
OUR_KEEP = 3
LIQUID_WEIGHT = 20
OWN_WEIGHT = 4
WON_BONUS = 10 ** 7
LOST_SCORE = -10 ** 9
WIDE_SPAN = 72
ROLL_WAIT = 300
STUCK = 3
STUCK_OUT = 6
STILL_PAIRS = 2
# An auction fills the panel; the words at 0x8860 outlive it (README).
AUCTION_PANEL = 7000
FACES = range(1, 7)
LOSER_PLATE = "perceptual_hash 497072db0b360dcd within 6"
WINNER_PLATE = "perceptual_hash 47b938c18c1e71dc within 6"
PLATES = (("loser", LOSER_PLATE), ("winner", WINNER_PLATE))
PLATE_STEP = 900
PLATE_PRESSES = 24


class Player:
    """Our seat: a beam over the model, its candidates tried in the core."""

    def __init__(self, run, table):
        self.run = run
        self.table = table
        self.hunt = search.Search(table)
        self.cands = []
        self.start = None
        self.spent = 0
        self.half = {}
        self.rows = []
        self.gaps = []
        self.trials = 0
        self.trial_frames = 0
        self.standing = None
        self.marks = {}
        self.keeping = False

    def begin(self):
        """No restore_or_play in the scenario api, so it is spelt out."""
        run = self.run
        try:
            run.restore(CHECKPOINT)
        except RuntimeError:
            print("tape", run.play(TAPE), flush=True)
            run.checkpoint(CHECKPOINT)
        run.release(engine.PORT)
        engine.settle(run)

    def read(self):
        return model.Model(self.table).read(self.run)

    def prompt(self):
        """Name the screen the game is waiting on from the state alone."""
        run = self.run
        if state.current(run) != SEAT:
            return "opponent"
        if engine.in_menu(run):
            return "menu"
        if engine.roll_prompt(run):
            return "jail" if engine.jailed(run) else "roll"
        if engine.deed_prompt(run):
            return "deed"
        if engine.auction_prompt(run) and engine.panel(run) >= AUCTION_PANEL:
            return "auction"
        return "notice"

    def our_idle(self, want, double=False):
        """Hold back until our press draws that total, double or not."""
        register = dice.register_of(self.run)
        total = sum(want) if want else 0
        spare = None
        for offset in range(WIDE_SPAN):
            roll = dice.roll_at(register, offset)
            if (roll[0] == roll[1]) != double:
                continue
            if total and sum(roll) == total:
                return offset
            if spare is None:
                spare = offset
        return 0 if spare is None else spare

    def roll(self, want, double=False):
        """Press at the frame that draws it, and read the faces it drew."""
        run = self.run
        before = state.dice(run)
        offset = self.our_idle(want, double)
        if offset:
            run.step(offset)
        engine.press(run, engine.CONFIRM, after=0)
        seen = None
        for _ in range(ROLL_WAIT):
            now = state.dice(run)
            if now != before and all(face in FACES for face in now):
                seen = now
                break
            run.step(1)
        engine.settle(run)
        return seen

    def walk(self, choice):
        """Our half: the wanted rolls by offset, other presses staggered."""
        run = self.run
        rolls = list(choice["rolls"])
        made = []
        start = run.frames()
        seen, stuck = None, 0
        if choice["build"][0]:
            choice["built"] = engine.build(run, choice["build"][0],
                                           choice["build"][1])
        for index in range(ANSWERS):
            engine.settle(run)
            self.plate_seen()
            if (engine.decided(run) or state.current(run) != SEAT
                    or run.frames() - start > TURN_FRAMES):
                break
            here = (self.prompt(), state.player(run, SEAT)["cash"],
                    state.player(run, SEAT)["square"])
            stuck = stuck + 1 if here == seen else 0
            seen = here
            if stuck >= STUCK_OUT:
                break
            name = here[0]
            if name in ("roll", "jail") and stuck < STUCK:
                drew = self.roll(rolls.pop(0) if rolls else None,
                                 name == "jail")
                if drew:
                    made.append(drew)
                continue
            if index:
                run.step(choice["idle"])
            if name == "menu":
                engine.close_menu(run)
            elif name == "auction":
                engine.auction(run, 0)
            elif name == "deed" and not self.affords(choice["buy"]):
                engine.decline(run)
            elif stuck >= STUCK:
                engine.answer(run, engine.BID_OUT)
            else:
                engine.acknowledge(run)
        return made

    def affords(self, buy):
        """A deed we cannot pay for is a decline, whatever the plan says."""
        run = self.run
        square = state.player(run, SEAT)["square"]
        return buy and (state.player(run, SEAT)["cash"]
                        >= self.table.price(square))

    def rival_turn(self, idle):
        """Answer the rival's screens; its roll was chosen back in our half."""
        run = self.run
        start = run.frames()
        for _ in range(ANSWERS):
            engine.settle(run)
            self.plate_seen()
            if engine.decided(run) or state.current(run) == SEAT:
                break
            if idle:
                run.step(idle)
            engine.acknowledge(run)
            if run.frames() - start > TURN_FRAMES:
                break
        return run.frames() - start

    def apply(self, run, index):
        """One candidate played out of the turn's checkpoint, pad released."""
        choice = self.cands[index]
        run.release(engine.PORT)
        start = run.frames()
        choice["made"] = self.walk(choice)
        self.half = {"cash": [state.player(run, s)["cash"]
                              for s in (SEAT, RIVAL)],
                     "squares": [state.player(run, s)["square"]
                                 for s in (SEAT, RIVAL)]}
        self.rival_turn(choice["idle"])
        self.spent = run.frames() - start
        self.trials += 1
        self.trial_frames += self.spent
        return self.spent

    def score(self, run):
        """What the rival can still raise, against our own and the frames."""
        if engine.lost(run, SEAT):
            return LOST_SCORE
        after = self.read()
        value = (OWN_WEIGHT * after.liquid(SEAT)
                 - LIQUID_WEIGHT * after.liquid(RIVAL) - self.spent)
        if state.left(run) <= 1:
            value += WON_BONUS
        return value

    def halves(self, table, line):
        """The model's best first halves, without the rival's answer."""
        seen, out = set(), []
        steps = list(line["line"][:1]) if line and line["line"] else []
        for node in self.hunt.root_children(table):
            steps.append(node["line"][0])
        for step in steps:
            key = (step["ours"], step["buy"], tuple(step["build"]))
            if key in seen:
                continue
            seen.add(key)
            ahead = table.clone()
            ahead.turn(SEAT, step["rolls"], [step["buy"]]
                       * (len(step["rolls"]) + 1))
            if step["build"][0]:
                ahead.build(SEAT, step["build"][0], step["build"][1])
            out.append({"total": step["ours"], "rolls": step["rolls"],
                        "buy": step["buy"], "build": step["build"],
                        "said": {"cash": list(ahead.cash),
                                 "squares": list(ahead.square)}})
            if len(out) >= OUR_KEEP:
                break
        return out

    def choices(self, table, line):
        out = []
        for half in self.halves(table, line):
            for idle in range(IDLES):
                item = dict(half)
                item["idle"] = idle
                out.append(item)
        return out

    def check(self, choice):
        """The model over the rolls the machine drew, against the machine."""
        ahead = self.start.clone()
        if choice["build"][0]:
            ahead.build(SEAT, choice["build"][0], choice["build"][1])
        ahead.turn(SEAT, choice["made"],
                   [choice["buy"]] * (len(choice["made"]) + 1))
        said = {"cash": list(ahead.cash), "squares": list(ahead.square)}
        agreed = (said["cash"] == self.half["cash"]
                  and said["squares"] == self.half["squares"])
        if not agreed:
            gap = {"said": said, "got": dict(self.half),
                   "made": choice["made"], "wanted": choice["rolls"],
                   "buy": choice["buy"]}
            self.gaps.append(gap)
            print("gap", gap, flush=True)
        return agreed

    def turn(self, index):
        run = self.run
        run.checkpoint("turn-%d" % index)
        start = run.frames()
        table = self.read()
        self.start = table
        line = self.hunt.run(table)
        self.cands = self.choices(table, line)
        found = tash.search(list(range(len(self.cands))), self.apply,
                            self.score)
        pick = self.cands[found.best]
        self.keeping = True
        self.apply(run, found.best)
        self.keeping = False
        after = self.read()
        row = {"turn": index, "total": pick["total"], "buy": pick["buy"],
               "build": pick["build"], "idle": pick["idle"],
               "made": pick["made"], "trials": found.trials,
               "search_frames": found.frames, "seconds": found.seconds,
               "frames": run.frames() - start,
               "cash": list(after.cash), "squares": list(after.square),
               "liquid": [after.liquid(s) for s in (SEAT, RIVAL)],
               "depth": len(line["line"]) if line else 0,
               "agreed": self.check(pick)}
        self.rows.append(row)
        run.mark("turn %d total %d idle %d" % (index, row["total"],
                                               row["idle"]), "turns")
        print("pair %2d roll %2d buy %-5s build %-12s idle %2d cash %6d/%-6d"
              " sq %2d/%-2d liquid %6d/%-6d frames %5d trials %3d %5.1fs"
              " depth %2d %s"
              % (index, row["total"], row["buy"], str(row["build"]),
                 row["idle"], row["cash"][0], row["cash"][1],
                 row["squares"][0], row["squares"][1], row["liquid"][0],
                 row["liquid"][1], row["frames"], row["trials"],
                 row["seconds"], row["depth"],
                 "ok" if row["agreed"] else "GAP"), flush=True)
        return row

    def play(self, pairs=PAIRS):
        """Pairs until the rival folds, the budget ends or nothing moves."""
        still = 0
        for index in range(pairs):
            if engine.decided(self.run):
                break
            row = self.turn(index)
            same = (row["cash"], row["squares"])
            still = still + 1 if same == self.standing else 0
            self.standing = same
            if still >= STILL_PAIRS:
                print("no move in %d pairs, stopping" % still, flush=True)
                break
        return self.rows

    def plate_seen(self):
        """Each end plate marked the first frame of the kept line it is on."""
        if not self.keeping:
            return
        for name, plate in PLATES:
            if name not in self.marks and self.run.anchor(plate):
                self.marks[name] = self.run.frames()
                self.run.mark("%s plate" % name, "plates")
                self.run.look()

    def plates(self):
        """The plates the game ends on; never press past one already up."""
        run = self.run
        self.keeping = True
        for name, plate in PLATES:
            for _ in range(PLATE_PRESSES):
                self.plate_seen()
                if name in self.marks or run.anchor(WINNER_PLATE):
                    break
                try:
                    run.run_until(plate, PLATE_STEP)
                except RuntimeError:
                    engine.press(run, engine.CONFIRM, after=0)
        self.plate_seen()
        return self.marks


def main(run):
    player = Player(run, board.Board(run))
    player.begin()
    rows = player.play()
    won = state.left(run) <= 1
    marks = player.plates() if won else {}
    print("trials %d, trial frames %d, nodes %d, expansions %d"
          % (player.trials, player.trial_frames, player.hunt.nodes,
             player.hunt.expanded), flush=True)
    print("plates", marks, flush=True)
    run.expect("the rival was bankrupted", won,
               "players_left %d after %d turn pairs"
               % (state.left(run), len(rows)), raises=False)
    run.expect("the winner's plate was shown", "winner" in marks,
               "loser plate at frame %s, winner plate at frame %s"
               % (marks.get("loser"), marks.get("winner")), raises=False)
    run.judge("the line was searched, not placed by hand", True,
              "%d pairs, %d trials, %d frames"
              % (len(rows), player.trials, run.frames()))
    print(run.report())


main(tash.run)
