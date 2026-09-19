"""The pointing hand every panel in this casino is steered by."""

CURSOR = 0xCBC4
STEP_PIXELS = 16
STEP_FRAMES = 4
AFTER = 2
CORRECTIONS = 1
NAMES = (("left", "right"), ("up", "down"))
PRESS_FRAMES = 6
PRESS_AFTER = 0


def at(run):
    """Answer the pointing hand's (x, y) in the panel's own coordinates."""
    block = run.memory("system", CURSOR, 8)
    return (int.from_bytes(block[0:2], "little"),
            int.from_bytes(block[4:6], "little"))


def steps(away):
    """Answer the steps that land nearest, half a step short of one asking."""
    return (abs(away) + STEP_PIXELS // 2 - 1) // STEP_PIXELS


def push(run, axis, away):
    """Hold one direction the whole way; answer whether it was held at all."""
    count = steps(away)
    if not count:
        return False
    run.hold(1, [NAMES[axis][1 if away > 0 else 0]])
    run.step(STEP_FRAMES * count - 1)
    run.release(1)
    return True


def move_to(run, x, y):
    """Steer the hand onto (x, y), one hold an axis; answer where it rests."""
    target = (x, y)
    for _ in range(CORRECTIONS + 1):
        here = at(run)
        away = [target[axis] - here[axis] for axis in (0, 1)]
        far = 0 if abs(away[0]) >= abs(away[1]) else 1
        moved = push(run, far, away[far])
        moved = push(run, 1 - far, away[1 - far]) or moved
        if not moved:
            break
        run.step(AFTER)
    return at(run)


def press(run, button="b", frames=PRESS_FRAMES, after=PRESS_AFTER):
    """Tap a button where the hand stands and let the panel answer."""
    run.hold(1, [button])
    run.step(frames)
    run.release(1)
    run.step(after)
