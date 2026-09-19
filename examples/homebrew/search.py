"""What a tash.search run does: try a handful of input strings, keep the one
that carries the demo furthest, and mark every trial in one group."""

import tash

GROUP = "trial"
SETTLE_FRAMES = 30
TAP_FRAMES = 20

BUTTONS = {"a": "a", "b": "b", "y": "y",
           "s": "start", "l": "left", "r": "right"}

# A candidate is a string of taps, so a longer string is a longer run; the
# demo ignores the pad, and what the watch measures is how far the run got.
CANDIDATES = ("a", "rl", "sab", "abyr", "rlrlab")

run = tash.run


def press(driven, candidate):
    """Tap each button of the candidate string in turn."""
    driven.mark(f"trial {candidate}", group=GROUP)
    for letter in candidate:
        driven.hold(1, BUTTONS[letter])
        driven.step(TAP_FRAMES // 2)
        driven.release(1)
        driven.step(TAP_FRAMES - TAP_FRAMES // 2)


def moved(driven):
    """How far the demo's own frame counter got."""
    return driven.watch("beat")


run.step(SETTLE_FRAMES)
started = tash.exact_hash()

found = tash.search(CANDIDATES, press, moved)
print(f"best {found.best!r} scored {found.score} in {found.trials} trials,"
      f" {found.frames} frames, {found.seconds:.2f} s")

run.mark("searched")
run.expect("the-longest-string-carried-the-run-furthest",
           found.best == CANDIDATES[-1],
           f"{found.best!r} scored {found.score}")
run.expect("every-candidate-was-tried", found.trials == len(CANDIDATES),
           f"{found.trials} trials over {len(CANDIDATES)} candidates")
run.expect("the-search-put-the-run-back-where-it-started",
           tash.exact_hash() == started,
           f"exact={tash.exact_hash():016x} started={started:016x}")

if run.bundle():
    page = run.report()
    collapsed = f"x{len(CANDIDATES)}, frames"
    run.expect("the-report-collapsed-the-group-into-one-row",
               collapsed in open(page, encoding="utf-8").read(),
               f"{collapsed!r} in {page}")
