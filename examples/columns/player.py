"""Play Columns to 10,000 at EASY: search every placement, keep the best."""

import sys

sys.path.insert(0, "examples/columns")

import tash
import well

run = tash.run
REGION = "system"
RAM = (REGION, 0, well.RAM_BYTES)
BLOCK = (REGION, well.WELL_BASE, well.ROWS * well.ROW_BYTES)
GOAL = 10_000
# README.md, "What an input costs" and "What a drop costs": a tap registers
# on the next frame, a held down lands from the spawn in 26 frames.
TAP_FRAMES = 1
GAP_FRAMES = 1
DROP_FRAMES = 2 * (well.ROWS - 1) + 2
SETTLE_FRAMES = 30
SETTLE_TIMEOUT = 400
SPAWN_TIMEOUT = 900
ROTATIONS = 3
DEEP_HEIGHT = 10
DEAD = -1_000_000.0
CANDIDATES = [(lane, turns) for lane in range(well.COLUMNS)
              for turns in range(ROTATIONS)]

dead = False


def state():
    return well.decode(run.memory(*RAM))


def heights(rows):
    return [next((well.ROWS - row for row in range(well.ROWS)
                  if rows[row][lane] != well.EMPTY), 0)
            for lane in range(well.COLUMNS)]


def tap(button):
    run.hold(1, button)
    run.step(TAP_FRAMES)
    run.release(1)
    run.step(GAP_FRAMES)


def spawned():
    for _ in range(SPAWN_TIMEOUT):
        now = state()
        if now.falling is not None:
            return True
        if any(cell > 6 for row in now.rows for cell in row):
            return False
        run.step(1)
    return False


def place(run, choice):
    global dead
    lane, turns = choice
    if not spawned():
        dead = True
        return
    for _ in range(well.COLUMNS):
        falling = state().falling
        if falling is None or falling.x == lane:
            break
        tap("left" if falling.x > lane else "right")
    for _ in range(turns):
        tap("a")
    before = state().rows
    run.hold(1, "down")
    landed = False
    for _ in range(DROP_FRAMES):
        run.step(1)
        if state().rows != before:
            landed = True
            break
    run.release(1)
    if not landed:
        dead = True
        return
    try:
        run.memory_stable(*BLOCK, SETTLE_FRAMES, SETTLE_TIMEOUT)
    except RuntimeError:
        dead = True


def score(run):
    global dead
    if dead:
        dead = False
        return DEAD
    rows = state().rows
    if any(cell > 6 for row in rows for cell in row):
        return DEAD
    high = heights(rows)
    bumps = sum(abs(a - b) for a, b in zip(high, high[1:]))
    return run.watch("score") - 0.1 * max(high) - 0.01 * bumps


def play():
    global dead
    decision = 0
    while run.watch("score") < GOAL:
        if not spawned():
            break
        tall = max(heights(state().rows))
        depth = 2 if tall >= DEEP_HEIGHT else 1
        found = tash.search(CANDIDATES, place, score, depth=depth)
        if found.best is None or found.score <= DEAD:
            print(f"decision {decision}: every placement ends the game")
            break
        run.checkpoint(f"decision-{decision}")
        dead = False
        place(run, found.best)
        run.mark(f"decision {decision} {found.best}", group="decision")
        print(f"decision {decision} {found.best} depth {depth}"
              f" score {run.watch('score')} level {run.watch('level')}"
              f" tall {tall} trials {found.trials} {found.seconds:.1f}s")
        if dead:
            print(f"decision {decision}: the placement did not settle")
            break
        decision += 1
    run.step(60)
    final = run.watch("score")
    run.judge("reached 10,000 points at EASY without a game over",
              final >= GOAL and not dead,
              f"score {final}, jewels {run.watch('jewels')},"
              f" level {run.watch('level')}, {decision} decisions,"
              f" frame {run.frames()}")
    print(f"final score {final} frame {run.frames()}")


play()
