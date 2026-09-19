"""Prove or refute every move on Bret Hart's published list, by action id."""

import sys

sys.path.insert(0, "examples/wwf")

import combos
import tash

SPACINGS = tuple(range(2, 13))
# The grab holds for about fifty frames; a follow-up is swept over the frames
# it can go in on rather than over a spacing.
LEADS = (8, 14, 20, 26, 32, 38, 44)
GRAB = (("F", 2, 4), ("F+SP", 2, 0))
FOLLOW_GAP = 4
DASH_TRIGGER = 110
DASH_TIMEOUT = 200
RUN_TRIGGER = 120
RUN_TIMEOUT = 300
KNOCKDOWN_FRAMES = 30
FEET_TIMEOUT = 120
KNOCKDOWN_DAMAGE = 20
# The grounded rows sweep the frames waited over the body, not a spacing.
DOWN_WAIT = 10
# Long enough to cover the longest row, so that every state a context passes
# through on its own is known before a row is played from it.
IDLE_FRAMES = 260
# A move holds its id for twelve frames and up; the six-frame runs between two
# ids are the steps from one to the next, not a move.
REGISTER_FRAMES = 10
HELD_SK_FRAMES = 60
BARES = ("P", "K", "SP", "SK")
# README.md, "The action id": the two walks, and the grab the follow-ups of a
# hold all start from.
WALKS = (0x2BE8, 0x212C)
GRAB_ID = 0x649E
KNOCKDOWNS = (
    lambda s: (("B", 2, s), ("B", 2, s), ("SK", 2, 0)),
    lambda s: (("D", 2, s), ("DF", 2, s), ("F+SP", 2, 0)),
    lambda s: (("D", 2, s), ("D+P", 2, 0)),
)


def dash_in(run):
    """Tap forward, hold it, and run at the opponent until he is in range."""
    step = combos.forward(run)
    run.hold(1, [step])
    run.step(2)
    run.release(1)
    run.step(2)
    run.hold(1, [step])
    for _ in range(DASH_TIMEOUT):
        run.step(1)
        if abs(combos.gap(run)) <= DASH_TRIGGER:
            return


def run_in(run):
    """Hold RUN and close, leaving RUN held for the move that follows."""
    run.hold(1, [combos.BUTTONS["RUN"], combos.forward(run)])
    for _ in range(RUN_TIMEOUT):
        run.step(1)
        if abs(combos.gap(run)) <= RUN_TRIGGER:
            return


def to_the_feet(run):
    """Walk up to the fallen opponent until the sprites touch."""
    for _ in range(FEET_TIMEOUT):
        if abs(combos.gap(run)) <= combos.TOUCH:
            return True
        run.hold(1, [combos.forward(run)])
        run.step(1)
        run.release(1)
    return False


def knock_down(run):
    """Land a knockdown from point blank and stand over the fallen body."""
    for build in KNOCKDOWNS:
        for spacing in SPACINGS:
            run.restore("close")
            landed = combos.perform(
                run, ("knockdown", build(spacing), 0, 0, 0, 0))
            if landed.damage >= KNOCKDOWN_DAMAGE:
                run.step(KNOCKDOWN_FRAMES)
                return to_the_feet(run), landed.damage
    return False, 0


def bare_vocabulary(run):
    """Every id the four bare buttons pass through at point blank."""
    legend = {}
    for symbol in BARES:
        run.restore("close")
        landed = combos.perform(run, (symbol, ((symbol, 2, 0),), 0, 0, 0, 0))
        legend[symbol] = landed.seen()
        print(f"TABLE | {symbol} | the button alone |"
              f" {hex(landed.action())} | -- | {landed.first()} |"
              f" {landed.damage} | {landed.hits()} | {landed.meter} |",
              flush=True)
    return legend


class Result:
    """The best step of one published row, and what it produced."""

    def __init__(self, name, published):
        self.name = name
        self.published = published
        self.best = None
        self.most = None
        self.meter = 0
        self.tried = []

    def offer(self, step, landed, neutral):
        action = landed.action(neutral, REGISTER_FRAMES)
        self.tried.append(f"{step}:{action and hex(action)}")
        self.meter = max(self.meter, landed.meter)
        rank = (landed.hits(), landed.damage)
        if self.most is None or rank > self.most[3]:
            self.most = (step, action, landed, rank)
        if action is None:
            return
        if self.best is None or rank > self.best[3]:
            self.best = (step, action, landed, rank)

    def line(self):
        step, action, landed, _ = self.best or self.most
        name = hex(action) if self.best else "did not register"
        return (f"| {self.name} | {self.published} | {name} | {step} |"
                f" {landed.first()} | {landed.damage} | {landed.hits()} |"
                f" {self.meter} |")


def rows():
    """Every published row: name, notation, place, inputs."""
    held = combos.HELD_FRAMES
    listed = [
        ("Rolling Uppercut", "D, DF, F + SP", "close",
         lambda s: (("D", 2, s), ("DF", 2, s), ("F+SP", 2, 0))),
        ("Eye Rake", "D, DF, F + P", "close",
         lambda s: (("D", 2, s), ("DF", 2, s), ("F+P", 2, 0))),
        ("Eye Rake, P held", "hold P about 3 s", "close",
         lambda s: (("P", held, 0),)),
        ("Quick Uppercut", "D, D + P", "close",
         lambda s: (("D", 2, s), ("D+P", 2, 0))),
        ("Super Flying Kick", "B, B + SK", "close",
         lambda s: (("B", 2, s), ("B", 2, s), ("SK", 2, 0))),
        ("Arm Drag", "B, B + P, in close", "close",
         lambda s: (("B", 2, s), ("B", 2, s), ("P", 2, 0))),
        ("Backbreaker", "D, D + SK", "close",
         lambda s: (("D", 2, s), ("D+SK", 2, 0))),
        ("DDT", "RUN held, SP", "runup",
         lambda s: (("RUN+SP", 2, 0),)),
        ("Head grab", "F, F + SP", "close",
         lambda s: (("F", 2, s), ("F+SP", 2, 0))),
        ("Head grab, dashed", "F, F + SP off a dash", "dash",
         lambda s: (("F+SP", 2, 0),)),
        ("Sharpshooter, SP", "SP at the feet", "grounded",
         lambda s: (("-", 0, s * DOWN_WAIT), ("SP", 2, 0))),
        ("Sharpshooter, SK held", "hold SK at the feet", "grounded",
         lambda s: (("-", 0, s * DOWN_WAIT), ("SK", HELD_SK_FRAMES, 0))),
        ("grab string", "F+P tapped six times", "close",
         lambda s: (("F+P", 2, s),) * 6),
        ("punch string", "P, P, K, SK", "close",
         lambda s: (("P", 2, s), ("P", 2, s), ("K", 2, s), ("SK", 2, 0))),
    ]
    during = [
        ("Head Slam, during the hold", "P",
         lambda g: ((("P", 2, 0),))),
        ("Head Slam, during the hold, F+P", "F + P",
         lambda g: ((("F+P", 2, 0),))),
        ("Bulldog, during the hold", "D + SK",
         lambda g: ((("D+SK", 2, 0),))),
        ("Uppercut, during the hold", "D + SP",
         lambda g: ((("D+SP", 2, 0),))),
        ("Face Slam, during the hold", "D, DF, F + P tapped four times",
         lambda g: (("D", 2, g), ("DF", 2, g), ("F+P", 2, g), ("P", 2, g),
                    ("P", 2, g), ("P", 2, 0))),
        ("post-grab string", "F held, P tapped five times, SK",
         lambda g: (("F+P", 2, g),) * 5 + (("SK", 2, 0),)),
        ("post-grab 16-hit string", "F, F + SK, SP, P, K, SK",
         lambda g: (("F", 2, g), ("F+SK", 2, g), ("SP", 2, g), ("P", 2, g),
                    ("K", 2, g), ("SK", 2, 0))),
        ("combo string", "P and K tapped eight times",
         lambda g: (("P", 2, g), ("K", 2, g)) * 8),
    ]
    for name, published, follow in during:
        listed.append((name, f"the grab, then {published}", "hold",
                       lambda lead, follow=follow: GRAB[:1] + (
                           ("F+SP", 2, lead),) + follow(FOLLOW_GAP)))
    return listed


def main():
    run = tash.run
    run.mark("tape")
    played = run.play(combos.HARDEST_TAPE)
    run.expect("the tape reached the first bell at VERY HARD",
               played["segments"] == 5 and run.watch("skill") == 10,
               f"{played['frames']} frames, skill {run.watch('skill')}")
    run.checkpoint("bell")
    run.expect("point blank at VERY HARD", combos.approach(run),
               f"gap {combos.gap(run)}")
    run.checkpoint("close")
    legend = bare_vocabulary(run)
    print("bare at point blank:",
          {s: sorted(hex(v) for v in ids) for s, ids in legend.items()},
          flush=True)
    grounded, dealt = knock_down(run)
    run.checkpoint("grounded")
    run.judge("the opponent is down and the player is at his feet", grounded,
              f"{dealt} damage, gap {combos.gap(run)}, cpu action"
              f" {hex(run.watch('cpu_action'))}")

    places = {
        "close": lambda run: run.restore("close"),
        "hold": lambda run: run.restore("close"),
        "grounded": lambda run: run.restore("grounded"),
        "dash": lambda run: (run.restore("bell"), dash_in(run)),
        "runup": lambda run: (run.restore("bell"), run_in(run)),
    }
    bares = set(WALKS).union(*legend.values())
    quiet = {}
    for place, go in places.items():
        go(run)
        quiet[place] = bares | combos.idles(run, place, IDLE_FRAMES)
        print(f"{place} on its own:",
              sorted(hex(v) for v in quiet[place]), flush=True)
    results = []
    for name, published, place, build in rows():
        result = Result(name, published)
        neutral = quiet[place] | ({GRAB_ID} if place == "hold" else set())
        steps = LEADS if place == "hold" else SPACINGS
        if build(steps[0]) == build(steps[1]):
            steps = steps[:1]
        for step in steps:
            places[place](run)
            result.offer(step, combos.perform(
                run, (name, build(step), 0, 0, 0, 0)), neutral)
        results.append(result)
        print(f"{name}: {' '.join(result.tried)}", flush=True)
        if result.best:
            print("  ids", [(hex(v), n, at) for v, n, at
                            in result.best[2].runs
                            if n >= combos.MOVE_FRAMES],
                  "hurt", result.best[2].hurt, flush=True)
        print("TABLE " + result.line(), flush=True)
        run.mark(name, "published")
        run.judge(f"{name} registers an action id of its own",
                  bool(result.best), result.line())
    registered = [result for result in results if result.best]
    run.judge("the published list was played through", True,
              f"{len(registered)} of {len(results)} rows registered:"
              f" {', '.join(result.name for result in registered)}")


main()
