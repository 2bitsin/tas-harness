"""Bret Hart through the ladder on the combo meter: fill it, then finish."""

import os
import pathlib
import sys
import time

try:
    HERE = pathlib.Path(__file__).resolve().parent
except NameError:
    HERE = pathlib.Path("examples/wwf").resolve()
sys.path.insert(0, str(HERE))

import combos
import ring
import supers
import tash

WHOLE = 164
RECORDS = ("system", 0xB312, 14 * 6)
STRIDE = 14
OPPONENT_RECORDS = range(2, 6)
NAMES = {0: "Bret Hart", 1: "Razor Ramon", 2: "Undertaker", 3: "Yokozuna",
         4: "Shawn Michaels", 5: "Bam Bam Bigelow", 8: "Lex Luger"}

TAKEN_WEIGHT = 1.2
BAND_WEIGHT = 0.1
# A meter point is a sixteenth of a 52 damage combo, so it is worth about
# three points of damage; two keeps the player from whiffing for meter.
METER_WEIGHT = 2.0
CHARGE_WEIGHT = 12.0
STEP_FRAMES = 8
DASH_FRAMES = 16
BACK_FRAMES = 24
WALK_FRAMES = 120
GUARD_FRAMES = 24
WAIT_FRAMES = 12
SETTLE_FRAMES = 20
AWARD_FRAMES = 200
BELL_TIMEOUT = 5000
FALL_DECISIONS = 200
REWINDS_PER_FALL = 5
STALL_DECISIONS = 4
MATCH_LIMIT = 12
IDLE_TAIL = 12
CALIBRATE_TRIES = 6
CALIBRATE_GAP = 20
COMBO_BUTTON = "P"
COMBO_FOLLOW = supers.FOLLOWS[0]
RISE = ("P", "K", "SP", "SK")
GRAB_ROWS = ("Head grab", "Head Slam", "Slam", "Face Slam",
             "Uppercut in the hold", "post-grab string",
             "grab string")

ROWS = {row[0]: row for row in combos.COMBOS}
FROM_RANGE = ("DDT",)
CLOSING = ("walk in", "step in")
DASHING = ("dash in",) + FROM_RANGE
DEFENCE = ("guard", "wait")
STEPS = ("walk in", "step in", "dash in", "step out", "back off", "guard",
         "wait")
CANDIDATES = tuple(ROWS) + STEPS


def records(run):
    raw = run.memory(*RECORDS)
    return [raw[i * STRIDE] | (raw[i * STRIDE + 1] << 8) for i in range(6)]


def opponents(run):
    health = records(run)
    return [health[i] for i in OPPONENT_RECORDS]


def band_penalty(run):
    away = abs(combos.gap(run))
    return max(0, away - combos.REACH) + max(0, combos.TOUCH - away)


class Player:
    """One decision is a search; every frame of it is read and answered."""

    def __init__(self, run):
        self.run = run
        self.place = ring.Ring(run, 0)
        self.current = None
        self.banned = set()
        self.engaged = set()
        self.busy = False
        self.last = (0, 0, 0, 1, 0)
        self.tried_frames = 0
        self.step_idle = 0
        self.idle = 0
        self.spent = 0
        self.hurt = True
        self.matches_won = 0
        self.falls = 0
        self.decisions = 0
        self.combos = 0
        self.reversals = 0
        self.scrambles = 0
        self.rises = 0
        self.kept = (0, 0, 0)
        self.searched = 0
        self.widths = []
        self.started = time.time()

    def fired(self):
        return (self.combos, self.reversals, self.rises)

    def opp_health(self):
        return sum(opponents(self.run))

    def calibrate(self, match):
        """Find the player's object again, this match's slot."""
        found = None
        for _ in range(CALIBRATE_TRIES):
            found = ring.find(self.run)
            if found is not None:
                break
            self.run.step(CALIBRATE_GAP)
        if found is not None:
            self.place = ring.Ring(self.run, found)
        self.run.judge(f"match {match}: the player's object is found",
                       found is not None,
                       f"object {found and hex(found)}, action"
                       f" {hex(self.place.action())}, record"
                       f" {self.place.record()}, meter {self.place.meter()}")

    def engage(self):
        for i, health in zip(OPPONENT_RECORDS, opponents(self.run)):
            if health < WHOLE:
                self.engaged.add(i)

    def fall_state(self):
        if self.run.watch("p1_health") == 0:
            return "lost"
        self.engage()
        health = records(self.run)
        if all(health[i] == 0 for i in self.engaged | {2}):
            return "won"
        return None

    def idle_frame(self, mine, theirs):
        return (self.place.action() == combos.STANDING
                and self.run.watch("p1_health") == mine
                and self.opp_health() == theirs)

    def react(self):
        """Answer this frame's state with the input the ROM listens for."""
        if self.busy:
            return
        self.busy = True
        try:
            state = self.place.state()
            if state == ring.HELD:
                self.reversals += bool(supers.reverse(self.place))
            elif state == ring.HOLDING and self.place.loaded():
                if supers.initiate(self.place, COMBO_BUTTON) >= 0:
                    supers.follow(self.place, COMBO_FOLLOW)
                    self.combos += 1
            elif self.place.down():
                self.scramble()
        finally:
            self.busy = False

    def scramble(self):
        """Mash from the floor; a full meter buys the rise and is spent."""
        before = self.place.meter()
        self.place.tap([combos.BUTTONS[name] for name in RISE])
        self.scrambles += 1
        self.rises += before >= ring.FULL > self.place.meter()

    def advance(self, frames):
        run = self.run
        for _ in range(frames):
            was = (run.watch("p1_health"), self.opp_health())
            run.step(1)
            self.step_idle += self.idle_frame(*was)
            self.react()

    def hold_for(self, buttons, frames):
        self.run.hold(1, buttons)
        self.advance(frames)
        self.run.release(1, buttons)

    def walk_in(self, budget):
        run = self.run
        for walked in range(budget):
            away = abs(combos.gap(run))
            if combos.TOUCH <= away <= combos.REACH:
                if walked < WAIT_FRAMES:
                    self.advance(WAIT_FRAMES - walked)
                return
            step = (combos.forward(run) if away > combos.REACH
                    else combos.pad(run, "B")[0])
            self.hold_for([step], 1)

    def perform(self, row):
        """Play one row of the table, reading every frame of it."""
        run = self.run
        for symbol, hold, gap_frames in row[1]:
            pressed = combos.pad(run, symbol)
            run.hold(1, pressed)
            self.advance(hold)
            run.release(1, pressed)
            self.advance(gap_frames)
        run.release(1)
        quiet = 0
        for _ in range(combos.TAIL_FRAMES):
            was = (run.watch("p1_health"), self.opp_health())
            before = self.step_idle
            self.advance(1)
            quiet = quiet + 1 if self.step_idle > before else 0
            if quiet >= IDLE_TAIL:
                break

    def apply(self, run, name):
        self.current = name
        self.step_idle = 0
        started = run.frames()
        before = (self.opp_health(), run.watch("p1_health"),
                  self.place.meter())
        if name in ROWS:
            self.perform(ROWS[name])
        elif name == "walk in":
            self.walk_in(WALK_FRAMES)
        elif name == "step in":
            self.hold_for([combos.forward(run)], STEP_FRAMES)
            self.advance(SETTLE_FRAMES)
        elif name == "dash in":
            self.hold_for([combos.forward(run)], 2)
            self.advance(2)
            self.hold_for([combos.forward(run)], DASH_FRAMES)
            self.advance(SETTLE_FRAMES)
        elif name == "step out":
            self.hold_for(combos.pad(run, "B"), STEP_FRAMES)
            self.advance(SETTLE_FRAMES)
        elif name == "back off":
            self.hold_for(combos.pad(run, "B"), BACK_FRAMES)
            self.advance(SETTLE_FRAMES)
        elif name == "guard":
            self.hold_for([combos.BUTTONS["BLK"]], GUARD_FRAMES)
            self.advance(SETTLE_FRAMES // 2)
        else:
            self.advance(WAIT_FRAMES)
        dealt = max(0, before[0] - self.opp_health())
        taken = before[1] - run.watch("p1_health")
        gained = max(0, self.place.meter() - before[2])
        self.last = (dealt, taken, gained, run.frames() - started,
                     self.step_idle)

    def score(self, run):
        dealt, taken, gained, frames, _ = self.last
        weight = (CHARGE_WEIGHT if self.place.meter() < ring.FULL
                  else METER_WEIGHT)
        return ((dealt + weight * gained - TAKEN_WEIGHT * taken
                 - BAND_WEIGHT * band_penalty(run)) / max(1, frames))

    def candidates(self, run):
        away = abs(combos.gap(run))
        dropped = set(self.banned)
        dropped.update(CLOSING if away > combos.REACH else DASHING)
        if not self.hurt:
            dropped.update(DEFENCE)
        pool = GRAB_ROWS + STEPS if self.place.loaded() else CANDIDATES
        return [name for name in pool if name not in dropped]

    def choose(self, depth):
        found = tash.search(self.candidates, self.apply, self.score,
                            depth=depth)
        self.tried_frames += found.frames
        self.searched += 1
        self.widths.append(found.trials)
        return found.best, found

    def play_fall(self, match, fall):
        run = self.run
        print(f"match {match} fall {fall}: bell at frame {run.frames()},"
              f" opponent {NAMES.get(run.watch('opponent'), '?')}"
              f" ({run.watch('opponent')}), records {records(run)},"
              f" meter {self.place.meter()}, gap {combos.gap(run)}",
              flush=True)
        line, rewinds, stalled, depth = [], 0, 0, 1
        while len(line) < FALL_DECISIONS:
            index = len(line)
            name = f"m{match}f{fall}d{index}"
            run.checkpoint(name)
            if stalled and stalled % STALL_DECISIONS == 0:
                depth = 2
            choice, found = self.choose(depth)
            depth = 1
            self.banned = set()
            if choice is None:
                break
            was = self.fired()
            self.apply(run, choice)
            kept = tuple(now - before
                         for now, before in zip(self.fired(), was))
            self.kept = tuple(a + b for a, b in zip(self.kept, kept))
            dealt, taken, gained, frames, idle = self.last
            self.decisions += 1
            self.idle += idle
            self.spent += frames
            self.hurt = taken > 0
            run.mark(f"{self.decisions} {choice}", group="decision")
            line.append((choice, name, frames, idle, kept))
            stalled = 0 if dealt else stalled + 1
            print(f"  d{index} {choice}: dealt {dealt} taken {taken}"
                  f" meter {self.place.meter()} (+{gained})"
                  f" p1 {run.watch('p1_health')} opp {opponents(run)}"
                  f" in {frames} frames, {idle} idle; combos {self.combos}"
                  f" reversals {self.reversals} rises {self.rises}"
                  f" kept {self.kept}"
                  f" ({found.trials} trials, {found.seconds:.2f} s)",
                  flush=True)
            state = self.fall_state()
            if state == "won":
                return "won", len(line)
            if state == "lost":
                if rewinds >= REWINDS_PER_FALL:
                    return "lost", len(line)
                back = min(len(line), 2 ** (rewinds + 1))
                target = len(line) - back
                rewinds += 1
                banned_choice, target_name = line[target][:2]
                for _, _, frames, idle, kept in line[target:]:
                    self.spent -= frames
                    self.idle -= idle
                    self.kept = tuple(a - b
                                      for a, b in zip(self.kept, kept))
                run.restore(target_name)
                del line[target:]
                depth, stalled = 2, 0
                print(f"  fall lost: rewind {back} decisions to d{target},"
                      f" {banned_choice} set aside", flush=True)
                self.banned = {banned_choice}
        return "lost", len(line)

    def wait_for_bell(self, match):
        """Run on to the next bell; answer bell, fighting, off or timeout."""
        run = self.run
        run.release(1)
        mine = run.watch("p1_health")
        theirs = opponents(run)

        def bell():
            if run.watch("match") == 0:
                return True
            standing = opponents(run)
            if run.watch("p1_health") < mine:
                return True
            if any(now < was for now, was in zip(standing, theirs)):
                return True
            return (run.watch("p1_health") == WHOLE
                    and all(h in (0, WHOLE) for h in standing)
                    and any(h == WHOLE for h in standing)
                    and run.watch("timer_tens") == 9
                    and run.watch("timer_ones") == 8)
        try:
            waited = run.run_until(bell, BELL_TIMEOUT)
        except Exception as error:
            print(f"no bell within {BELL_TIMEOUT} frames: {error}",
                  flush=True)
            return "timeout"
        if run.watch("match") == 0:
            return "off"
        standing = opponents(run)
        if (run.watch("p1_health") < mine
                or any(now < was for now, was in zip(standing, theirs))):
            print(f"the fall goes on after {waited} frames", flush=True)
            return "fighting"
        print(f"bell after {waited} frames, match {run.watch('match')}",
              flush=True)
        return "bell"

    def idle_share(self):
        return 100.0 * self.idle / max(1, self.spent)

    def width(self):
        return sum(self.widths) / max(1, len(self.widths))

    def progress(self, match, fall, result, decisions):
        print(f"match {match} fall {fall} {result} in {decisions} decisions;"
              f" won {self.matches_won}, frame {self.run.frames()}, tried"
              f" {self.tried_frames}, idle {self.idle}/{self.spent}"
              f" ({self.idle_share():.1f}%), combos {self.combos},"
              f" reversals {self.reversals}, rises {self.rises},"
              f" kept {self.kept},"
              f" searches {self.searched} averaging {self.width():.1f}"
              f" trials; {time.time() - self.started:.0f} s", flush=True)


def main():
    run = tash.run
    player = Player(run)
    fall_limit = int(os.environ.get("WWF_FALLS", "0"))
    wall_limit = float(os.environ.get("WWF_WALL_MINUTES", "150")) * 60

    run.mark("tape")
    played = run.play(combos.HARDEST_TAPE)
    watches = run.observe()["watches"]
    run.expect("the tape reached the first bell at VERY HARD",
               played["segments"] == 5 and watches["skill"] == 10,
               f"{played['segments']} segments, {played['frames']} frames,"
               f" skill {watches['skill']}")
    run.expect("match one of the ladder", watches["match"] == 1)
    run.expect("both wrestlers are whole",
               watches["p1_health"] == watches["cpu_health"] == WHOLE)

    match = 1
    outcome = "unfinished"
    while match and player.matches_won < MATCH_LIMIT:
        run.mark(f"match {match} bell")
        player.calibrate(match)
        player.engaged = set()
        mine = theirs = fall = 0
        after = "bell"
        while mine < 2 and theirs < 2 and outcome == "unfinished":
            fall += 1
            result, decisions = player.play_fall(match, fall)
            if result == "won" and mine == 1:
                run.step(AWARD_FRAMES)
            after = player.wait_for_bell(match)
            if after == "fighting":
                fall -= 1
                continue
            player.falls += 1
            run.mark(f"match {match} fall {fall} {result}", group="fall")
            if result == "won":
                mine += 1
            else:
                theirs += 1
            player.progress(match, fall, result, decisions)
            if mine == 2:
                player.matches_won += 1
                run.mark(f"match {match} won")
            if fall_limit and player.falls >= fall_limit:
                outcome = "fall limit"
            elif time.time() - player.started > wall_limit:
                outcome = "wall limit"
            elif after != "bell":
                break
            elif run.watch("match") != match:
                break
        if outcome != "unfinished":
            break
        if theirs == 2:
            outcome = "lost"
            break
        if after != "bell":
            outcome = "belt" if mine == 2 else f"off the ladder ({after})"
            break
        match = run.watch("match")

    run.look()
    wall = time.time() - player.started
    text = (f"{outcome}: {player.matches_won} matches won, {player.falls}"
            f" falls, {player.decisions} decisions, last match {match};"
            f" {player.combos} combos, {player.reversals} reversals,"
            f" {player.rises} rises, kept {player.kept};"
            f" frame {run.frames()}, tried"
            f" {player.tried_frames}, idle {player.idle}/{player.spent}"
            f" ({player.idle_share():.1f}%), {player.searched} searches"
            f" averaging {player.width():.1f} trials, {wall:.0f} s")
    run.judge("the Intercontinental belt on the combo meter",
              outcome == "belt", text)
    print("done:", text, flush=True)


main()
