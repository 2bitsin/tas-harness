"""Bret Hart through the ladder: a search per decision over the move table."""

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
import tash

FULL = 164
RECORDS = ("system", 0xB312, 14 * 6)
STRIDE = 14
OPPONENT_RECORDS = range(2, 6)
NAMES = {0: "Bret Hart", 1: "Razor Ramon", 2: "Undertaker", 3: "Yokozuna",
         4: "Shawn Michaels", 5: "Bam Bam Bigelow", 8: "Lex Luger"}

TAKEN_WEIGHT = 1.2
BAND_WEIGHT = 0.1
STEP_FRAMES = 8
DASH_FRAMES = 16
BACK_FRAMES = 24
WALK_FRAMES = 120
GUARD_FRAMES = 24
WAIT_FRAMES = 12
SETTLE_FRAMES = 20
AWARD_FRAMES = 200
# Between a fall and the next bell the game plays the pin, the award screen
# and, between matches, the walk-in; the tape's whole title-to-bell leg is
# 674 frames, so this is well past any of them.
BELL_TIMEOUT = 5000
FALL_DECISIONS = 200
REWINDS_PER_FALL = 5
STALL_DECISIONS = 4
MATCH_LIMIT = 12

ROWS = {row[0]: row for row in combos.COMBOS}
# Every row is playable from point blank but the DDT, which needs the room to
# run; the holds and their follow-ups are rows of their own, so no decision
# has to reach a context another decision left behind.
FROM_RANGE = ("DDT",)
BARES = ("Punch", "Kick", "Super Punch", "Super Kick")
CLOSING = ("walk in", "step in")
DASHING = ("dash in",) + FROM_RANGE
DEFENCE = ("guard", "wait")
STEPS = ("walk in", "step in", "dash in", "step out", "back off", "guard",
         "wait")
CANDIDATES = tuple(ROWS) + STEPS
BARE_IDS = {ROWS[name][5] for name in BARES}


# The wrestler's animation word is not at a fixed address across matches: the
# object slots are handed out in the order the match loads them.  The player
# finds it again at every bell, in the block the hunt found it in, by the two
# values it must take.
ACTION_BLOCK = (0x8000, 0x2000)
PUNCH_FRAMES = 14
CALIBRATE_TRIES = 6
CALIBRATE_GAP = 20
# The table's tail is 80 frames because a move has to be measured to its end;
# a move that has been standing still this long is over, and the rest of the
# tail is stalling.
IDLE_TAIL = 12


def find_action(run):
    """The address the player's animation word is at in this match."""
    base, size = ACTION_BLOCK
    run.checkpoint("calibrate")
    standing = run.memory("system", base, size)
    run.hold(1, [combos.BUTTONS["P"]])
    run.step(2)
    run.release(1)
    seen = []
    for _ in range(PUNCH_FRAMES):
        run.step(1)
        seen.append(run.memory("system", base, size))
    run.restore("calibrate")
    punch = ROWS["Punch"][5]
    for i in range(0, size - 1, 2):
        if standing[i] | (standing[i + 1] << 8) != combos.STANDING:
            continue
        if any(block[i] | (block[i + 1] << 8) == punch for block in seen):
            return base + i
    return None


def records(run):
    """The health of each of the six wrestler records."""
    raw = run.memory(*RECORDS)
    return [raw[i * STRIDE] | (raw[i * STRIDE + 1] << 8) for i in range(6)]


def opponents(run):
    """The health of every opponent record, the unused ones stuck at 164."""
    health = records(run)
    return [health[i] for i in OPPONENT_RECORDS]


def band_penalty(run):
    away = abs(combos.gap(run))
    return max(0, away - combos.REACH) + max(0, combos.TOUCH - away)


def idle_frame(run, action, mine, theirs):
    """Whether the frame just run was one where nothing happened."""
    return (action() == combos.STANDING
            and run.watch("p1_health") == mine
            and sum(opponents(run)) == theirs)


class Tracker(combos.Landed):
    """combos.Landed over every opponent record, by the match's own word."""

    def __init__(self, run, action):
        super().__init__(run)
        self.health = sum(opponents(run))
        self.action_of = action
        self.runs = [[action(), 0, 0]]
        self.idle = 0
        self.quiet = 0

    def step(self, frames):
        for _ in range(frames):
            was = (self.run.watch("p1_health"), self.health)
            self.run.step(1)
            self.frame += 1
            quiet = idle_frame(self.run, self.action_of, *was)
            self.idle += quiet
            self.quiet = self.quiet + 1 if quiet else 0
            action = self.action_of()
            if self.runs[-1][0] == action:
                self.runs[-1][1] += 1
            else:
                self.runs.append([action, 1, self.frame])
            now = sum(opponents(self.run))
            if now < self.health:
                if self.at and self.frame - self.at[-1] <= 1:
                    self.at[-1] = self.frame
                else:
                    self.at.append(self.frame)
                self.damage += self.health - now
            self.health = now


class Trial:
    def __init__(self, name, score, dealt, taken, tracker):
        self.name = name
        self.score = score
        self.dealt = dealt
        self.taken = taken
        self.tracker = tracker

    def worthwhile(self):
        """Whether the exchange was not lost, whatever it scored a frame."""
        return self.dealt >= TAKEN_WEIGHT * self.taken

    def landed_row(self):
        """Whether this trial played the animation its row is named by."""
        row = ROWS.get(self.name)
        if row is None or self.tracker is None:
            return False
        return row[5] in self.tracker.seen()


class Player:
    def __init__(self, run):
        self.run = run
        self.landed = {}
        self.trials = []
        self.recording = True
        self.current = None
        self.banned = set()
        self.engaged = set()
        self.tried_frames = 0
        self.action_at = None
        self.step_idle = 0
        self.idle = 0
        self.spent = 0
        self.hurt = True
        self.matches_won = 0
        self.falls = 0
        self.decisions = 0
        self.started = time.time()

    def opp_health(self):
        return sum(opponents(self.run))

    def action(self):
        """The player's animation word, wherever this match keeps it."""
        if self.action_at is None:
            return self.run.watch("p1_action")
        raw = self.run.memory("system", self.action_at, 2)
        return raw[0] | (raw[1] << 8)

    def calibrate(self, match):
        """Find the animation word again, this match's slot."""
        previous = self.action_at
        self.action_at = None
        found = None
        for _ in range(CALIBRATE_TRIES):
            found = find_action(self.run)
            if found is not None:
                break
            self.run.step(CALIBRATE_GAP)
        self.action_at = found if found is not None else previous
        self.run.judge(f"match {match}: the animation word is found",
                       found is not None,
                       f"at {found and hex(found)}, standing"
                       f" {hex(self.action())}")

    def engage(self):
        """Note every extra record that has taken damage this match."""
        for i, health in zip(OPPONENT_RECORDS, opponents(self.run)):
            if health < FULL:
                self.engaged.add(i)

    def fall_state(self):
        if self.run.watch("p1_health") == 0:
            return "lost"
        self.engage()
        health = records(self.run)
        if all(health[i] == 0 for i in self.engaged | {2}):
            return "won"
        return None

    def advance(self, frames):
        """Step, counting the frames where no health moves and nobody acts."""
        run = self.run
        for _ in range(frames):
            was = (run.watch("p1_health"), self.opp_health())
            run.step(1)
            self.step_idle += idle_frame(run, self.action, *was)

    def hold_for(self, buttons, frames):
        run = self.run
        run.hold(1, buttons)
        self.advance(frames)
        run.release(1, buttons)

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

    def apply(self, run, name):
        self.current = name
        self.step_idle = 0
        started = run.frames()
        before = (self.opp_health(), run.watch("p1_health"))
        tracker = None
        if name in ROWS:
            tracker = self.perform(ROWS[name])
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
        idle = self.step_idle + (tracker.idle if tracker else 0)
        self.last = (dealt, taken, tracker, run.frames() - started, idle)

    def perform(self, row):
        """combos.perform with the tracker over every opponent record."""
        run = self.run
        tracker = Tracker(run, self.action)
        for symbol, hold, gap_frames in row[1]:
            pressed = combos.pad(run, symbol)
            run.hold(1, pressed)
            tracker.step(hold)
            run.release(1, pressed)
            tracker.step(gap_frames)
        run.release(1)
        for _ in range(combos.TAIL_FRAMES):
            tracker.step(1)
            if tracker.quiet >= IDLE_TAIL:
                break
        return tracker

    def score(self, run):
        dealt, taken, tracker, frames, _ = self.last
        value = ((dealt - TAKEN_WEIGHT * taken
                  - BAND_WEIGHT * band_penalty(run)) / max(1, frames))
        if self.recording:
            self.trials.append(Trial(self.current, value, dealt, taken,
                                     tracker))
        return value

    def candidates(self, run):
        away = abs(combos.gap(run))
        dropped = set(self.banned)
        dropped.update(CLOSING if away > combos.REACH else DASHING)
        if not self.hurt:
            dropped.update(DEFENCE)
        return [name for name in CANDIDATES if name not in dropped]

    def choose(self, depth):
        """Search from here and answer the candidate to play."""
        self.trials = []
        self.recording = depth == 1
        found = tash.search(self.candidates, self.apply, self.score,
                            depth=depth)
        self.tried_frames += found.frames
        choice = found.best
        if self.recording and self.trials:
            unlanded = [trial for trial in self.trials
                        if trial.name in ROWS and trial.name not in self.landed
                        and trial.landed_row() and trial.worthwhile()]
            if unlanded:
                choice = max(unlanded, key=lambda trial: trial.score).name
        return choice, found

    def top(self, count=3):
        ranked = sorted(self.trials, key=lambda trial: -trial.score)
        return ", ".join(f"{trial.name} {trial.score:.1f}"
                         for trial in ranked[:count])

    def play_fall(self, match, fall):
        """Decide until one side's health is 0; rewind a lost fall."""
        run = self.run
        print(f"match {match} fall {fall}: bell at frame {run.frames()},"
              f" opponent {NAMES.get(run.watch('opponent'), '?')}"
              f" ({run.watch('opponent')}), records {records(run)},"
              f" engaged {sorted(self.engaged)}, gap {combos.gap(run)}",
              flush=True)
        line = []
        rewinds = 0
        stalled = 0
        depth = 1
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
            self.apply(run, choice)
            dealt, taken, tracker, frames, idle = self.last
            self.decisions += 1
            self.idle += idle
            self.spent += frames
            self.hurt = taken > 0
            run.mark(f"{self.decisions} {choice}", group="decision")
            line.append((choice, name, frames, idle))
            stalled = 0 if dealt else stalled + 1
            print(f"  d{index} {choice}: dealt {dealt} taken {taken}"
                  f" gap {combos.gap(run)} p1 {run.watch('p1_health')}"
                  f" opp {opponents(run)} in {frames} frames, {idle} idle"
                  f" ids {tracker and sorted(hex(v) for v in tracker.seen())}"
                  f" ({found.trials} trials,"
                  f" {found.seconds:.2f} s) top: {self.top()}", flush=True)
            played = tracker.seen() if tracker else set()
            for name, row in ROWS.items():
                if name in self.landed or row[5] not in played:
                    continue
                if name != choice and row[5] in BARE_IDS:
                    continue
                self.landed[name] = (match, fall, index)
                run.judge(f"{name} played", True,
                          f"match {match} fall {fall} decision {index}"
                          f" ({choice}): action {hex(row[5])},"
                          f" {dealt} damage in {tracker.hits()} hit(s)"
                          f" at frames {tracker.at}, {taken} taken,"
                          f" {frames} frames, {idle} idle;"
                          f" {len(self.landed)} of {len(ROWS)} rows")
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
                for _, _, frames, idle in line[target:]:
                    self.spent -= frames
                    self.idle -= idle
                run.restore(target_name)
                for row in list(self.landed):
                    if self.landed[row] >= (match, fall, target):
                        del self.landed[row]
                del line[target:]
                depth = 2
                stalled = 0
                print(f"  fall lost: rewind {back} decisions to d{target},"
                      f" {banned_choice} set aside, depth 2 next",
                      flush=True)
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
            return (run.watch("p1_health") == FULL
                    and all(h in (0, FULL) for h in standing)
                    and any(h == FULL for h in standing)
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
            print(f"the fall goes on after {waited} frames: p1"
                  f" {run.watch('p1_health')}, opponents {standing}",
                  flush=True)
            return "fighting"
        print(f"bell after {waited} frames, match watch {run.watch('match')}",
              flush=True)
        return "bell"

    def idle_share(self):
        return 100.0 * self.idle / max(1, self.spent)

    def progress(self, match, fall, result, decisions):
        print(f"match {match} fall {fall} {result} in {decisions} decisions;"
              f" matches won {self.matches_won}, frame {self.run.frames()},"
              f" tried {self.tried_frames}, idle {self.idle}"
              f"/{self.spent} ({self.idle_share():.1f}%),"
              f" landed {len(self.landed)}"
              f"/{len(ROWS)}: {', '.join(sorted(self.landed))};"
              f" {time.time() - self.started:.0f} s", flush=True)


def main():
    run = tash.run
    player = Player(run)
    fall_limit = int(os.environ.get("WWF_FALLS", "0"))
    wall_limit = float(os.environ.get("WWF_WALL_MINUTES", "75")) * 60

    run.mark("tape")
    played = run.play(combos.HARDEST_TAPE)
    watches = run.observe()["watches"]
    run.expect("the tape reached the first bell at VERY HARD",
               played["segments"] == 5 and watches["skill"] == 10,
               f"{played['segments']} segments, {played['frames']} frames,"
               f" skill {watches['skill']}")
    run.expect("match one of the ladder", watches["match"] == 1)
    run.expect("both wrestlers are whole",
               watches["p1_health"] == watches["cpu_health"] == FULL)

    match = 1
    outcome = "unfinished"
    while match and player.matches_won < MATCH_LIMIT:
        run.mark(f"match {match} bell")
        player.calibrate(match)
        player.engaged = set()
        mine = theirs = 0
        fall = 0
        after = "bell"
        while mine < 2 and theirs < 2 and outcome == "unfinished":
            fall += 1
            result, decisions = player.play_fall(match, fall)
            if result == "won" and mine == 1:
                run.step(AWARD_FRAMES)
                run.look()
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
                if mine < 2:
                    print(f"match watch moved to {run.watch('match')} with"
                          f" the match at {mine}-{theirs}", flush=True)
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
    unlanded = [name for name in ROWS if name not in player.landed]
    wall = time.time() - player.started
    text = (f"{outcome}: {player.matches_won} matches won, {player.falls}"
            f" falls, {player.decisions} decisions, last match {match};"
            f" landed {len(player.landed)}/{len(ROWS)}"
            f" (unlanded: {', '.join(unlanded) or 'none'});"
            f" frame {run.frames()}, tried {player.tried_frames},"
            f" idle {player.idle}/{player.spent}"
            f" ({player.idle_share():.1f}%), {wall:.0f} s")
    run.judge("the Intercontinental belt, every row of the table landed",
              outcome == "belt" and not unlanded, text)
    print("done:", text, flush=True)


main()
