"""Columns from power on: play the tape in, then drop columns and watch."""

import pathlib

import tash

HERE = pathlib.Path(__file__).resolve().parent

# A landing moves 1% to 6% of the screen; a clear, or a screen that has
# changed altogether, moves more. Measured over the bursts below.
BUSY_CHANGE = 0.07

TAP_FRAMES = 3
DROP_FRAMES = 100
SETTLE_FRAMES = 40

# Where each column is steered before it is dropped, left to right.
PLAN = [("left", 3), ("left", 2), ("left", 1),
        ("right", 1), ("right", 2), ("right", 3)]

run = tash.run

run.mark("tape")
played = run.play(HERE / "tapes" / "title-to-game.yaml")
run.checkpoint("playfield")
started = run.observe()["watches"]

run.expect("the tape reached the playfield", played["segments"] == 5,
           f"{played['segments']} segments, {played['frames']} frames")
run.expect("a game is running at level 0", started["level"] == 0)
run.expect("the first column landed", started["score"] > 0,
           f"score {started['score']}")

for burst, (side, taps) in enumerate(PLAN):
    for _ in range(taps):
        run.hold(1, [side])
        run.step(TAP_FRAMES)
        run.release(1)
        run.step(TAP_FRAMES)
    run.hold(1, ["down"])
    run.step(DROP_FRAMES)
    run.release(1)
    run.step(SETTLE_FRAMES)

    seen = run.observe()
    print(f"burst {burst} {side} {taps}: change {seen['change']:.4f},"
          f" score {seen['watches']['score']}")
    if seen["change"] > BUSY_CHANGE:
        run.mark(f"burst-{burst}")
        shot = run.look() if run.bundle() else None
        print(f"  busy: {shot}")

played_out = seen["watches"]
run.judge("the score rose across the play",
          played_out["score"] > started["score"],
          f"{started['score']} to {played_out['score']}")
print(f"score {played_out['score']} level {played_out['level']}"
      f" jewels {played_out['jewels']}")
