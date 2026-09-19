"""Thirty columns land where they are asked, at level 0 and at level 5."""

import dataclasses
import os
import pathlib
import subprocess

import well

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PROFILE = HERE / "profile.yaml"
TAPE = HERE / "tapes" / "title-to-game.yaml"
LEVEL_VARIABLE = "TASH_COLUMNS_LEVEL"
LEVELS = (0, 5)
PLACEMENTS = 30
ROTATIONS = 3
RUN_SECONDS = 600

REGION = "system"
BLOCK_BYTES = well.ROWS * well.ROW_BYTES
SETTLED_IDS = frozenset(range(7))

# README.md, "What an input costs": a button held one frame registers on the
# next, and one frame off is enough for the next tap to register too.
TAP_FRAMES = 1
GAP_FRAMES = 1
# A held down falls eight pixels a frame: 24 frames from the spawn to the
# floor, and two more before the well takes the column.
DROP_FRAMES = 2 * (well.ROWS - 1) + 2
# A clear sits still for sixteen frames in the middle, and the longest chain
# measured settles 237 frames after the landing.
SETTLE_FRAMES = 30
SETTLE_TIMEOUT = 400
SPAWN_TIMEOUT = 900

# The start the README's hunt scripts, and the down presses that pick the
# difficulty on SELECT LEVEL: none is EASY at level 0, one is MEDIUM at 5.
TITLE_FRAMES = 320
MENU_FRAMES = 180
SELECT_FRAMES = 240
PRESS_FRAMES = 10
DOWN_FRAMES = 2
DOWNS = {0: 0, 5: 1}


@dataclasses.dataclass(frozen=True)
class Placed:
    """One placement: the well before the drop, at the landing, settled."""

    before: well.Well
    landed: well.Well
    settled: well.Well
    frames: int


def _decoded(run):
    """The well as it stands."""
    return well.decode(run.memory(REGION, 0, well.RAM_BYTES))


def _tap(run, button):
    """One tap: held a frame, then a frame off."""
    run.hold(1, button)
    run.step(TAP_FRAMES)
    run.release(1)
    run.step(GAP_FRAMES)


def _heights(rows):
    """How high each lane is stacked, 0 for an empty lane."""
    return [next((well.ROWS - row for row in range(well.ROWS)
                  if rows[row][lane] != 0), 0)
            for lane in range(well.COLUMNS)]


def _moved(before, after):
    """The cells that differ between two wells, top row first."""
    return sorted((row, lane) for row in range(well.ROWS)
                  for lane in range(well.COLUMNS)
                  if before.rows[row][lane] != after.rows[row][lane])


def place(run, column, rotation):
    """Walk to that lane, rotate, drop, and wait for the well to settle."""
    for _ in range(well.COLUMNS):
        falling = _decoded(run).falling
        if falling is None or falling.x == column:
            break
        _tap(run, "left" if falling.x > column else "right")
    for _ in range(rotation):
        _tap(run, "a")

    before = _decoded(run)
    landed = None
    run.hold(1, "down")
    for _ in range(DROP_FRAMES):
        run.step(1)
        now = _decoded(run)
        if now.rows != before.rows:
            landed = now
            break
    run.release(1)
    if landed is None:
        raise RuntimeError(f"columns: the column over lane {column} did not"
                           f" land in {DROP_FRAMES} frames")
    frames = run.memory_stable(REGION, well.WELL_BASE, BLOCK_BYTES,
                               SETTLE_FRAMES, SETTLE_TIMEOUT)
    return Placed(before, landed, _decoded(run), frames)


def _landed_as_asked(placed, column):
    """Did the three jewels land in that lane, in the order they fell?"""
    cells = _moved(placed.before, placed.landed)
    if len(cells) != len(placed.before.falling.ids):
        return False
    if {lane for _, lane in cells} != {column}:
        return False
    rows = [row for row, _ in cells]
    if rows != list(range(rows[0], rows[0] + len(rows))):
        return False
    jewels = tuple(placed.landed.rows[row][lane] for row, lane in cells)
    return jewels == placed.before.falling.ids


def _spawned(run):
    """Waits for a column to be in flight; False when none comes."""
    for _ in range(SPAWN_TIMEOUT):
        if _decoded(run).falling is not None:
            return True
        run.step(1)
    return False


def _started(run, level):
    """Power on to the first falling column at that level."""
    if level == 0:
        run.play(str(TAPE))
        return
    run.step(TITLE_FRAMES)
    for wait in (MENU_FRAMES, SELECT_FRAMES):
        run.hold(1, "start")
        run.step(PRESS_FRAMES)
        run.release(1)
        run.step(wait)
    for _ in range(DOWNS[level]):
        run.hold(1, "down")
        run.step(DOWN_FRAMES)
        run.release(1)
        run.step(DOWN_FRAMES)
    run.hold(1, "start")
    run.step(PRESS_FRAMES)
    run.release(1)


def _drive(run, level):
    """Place thirty columns, each in the lane the run asks for."""
    _started(run, level)
    if not _spawned(run):
        raise RuntimeError(f"columns: no column falls at level {level}")
    run.expect(f"the-game-runs-at-level-{level}",
               run.watch("level") == level, f"level {run.watch('level')}")

    for turn in range(PLACEMENTS):
        if not _spawned(run):
            raise RuntimeError(f"columns: no column for placement {turn}")
        high = _heights(_decoded(run).rows)
        column = min(range(well.COLUMNS), key=lambda lane: (high[lane], lane))
        placed = place(run, column, turn % ROTATIONS)
        loose = {cell for row in placed.settled.rows for cell in row
                 } - SETTLED_IDS
        run.expect(f"placement-{turn:02d}-landed-in-lane-{column}",
                   _landed_as_asked(placed, column),
                   f"asked lane {column} over height {high[column]},"
                   f" {placed.before.falling} ->"
                   f" {_moved(placed.before, placed.landed)},"
                   f" settled in {placed.frames} frames")
        run.expect(f"placement-{turn:02d}-settled",
                   not loose, f"the settled well holds {sorted(loose)}")

    run.judge(f"thirty-placements-at-level-{level}", True,
              f"score {run.watch('score')}, jewels {run.watch('jewels')},"
              f" level {run.watch('level')}")
    print(f"{PLACEMENTS}/{PLACEMENTS} placed at level {level}")


def _rom():
    """The ROM the profile names."""
    for line in PROFILE.read_text(encoding="utf-8").splitlines():
        if line.startswith("rom:"):
            return pathlib.Path(line.split(":", 1)[1].strip())
    return None


def _binary():
    """The tash the build left behind, or None."""
    beside = [ROOT / "_install" / "tash"]
    beside += sorted((ROOT / "_build").glob("*/bin"))
    for directory in beside:
        if (directory / "tash").exists():
            return directory / "tash"
    return None


def _placed_at(level):
    """Drives this file as a scenario and reads its answer back."""
    import pytest    # the interpreter tash embeds carries no pytest

    binary = _binary()
    rom = _rom()
    if binary is None:
        pytest.skip("this tree holds no built tash")
    if rom is None or not rom.exists():
        pytest.skip(f"the columns rom is not at {rom}")
    done = subprocess.run(
        [str(binary), "run", "--profile", str(PROFILE),
         "--scenario", str(pathlib.Path(__file__).resolve())],
        capture_output=True, text=True, cwd=HERE, timeout=RUN_SECONDS,
        env=dict(os.environ, PYTHONPATH=str(HERE),
                 **{LEVEL_VARIABLE: str(level)}), check=False)
    assert done.returncode == 0, done.stdout + done.stderr
    assert f"{PLACEMENTS}/{PLACEMENTS} placed at level {level}" in done.stdout


def test_thirty_columns_land_where_they_are_asked_at_level_0():
    _placed_at(LEVELS[0])


def test_thirty_columns_land_where_they_are_asked_at_level_5():
    _placed_at(LEVELS[1])


if __name__ == "__main__":
    import tash

    _drive(tash.run, int(os.environ[LEVEL_VARIABLE]))
