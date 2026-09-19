"""The tape to the floor, then a tour that plays every game once."""

import os
import sys
import tash

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hand

run = tash.run
PLAYER = 0xD3BC
MONEY = 0xCDD0
TAPE = "examples/caesars/tapes/title-to-floor.yaml"
FLOOR = "floor"
STEP_FRAMES = 8
CLOSE_ENOUGH = 10
WALK_TRIES = 120
STALL_LIMIT = 3
BURST = "walk/burst"
SEEK_BRANCH = "seek"
SEEK_BUDGET = 3000
FACE_FRAMES = 24
CLICK_FRAMES = 6
SETTLE_FRAMES = 330
BANNER_WAIT = 240
MARK_SETTLE = 1
WEST = ((80, 832), (41, 760), (41, 666))
NORTH = WEST + ((137, 410), (263, 322))
KENO_WAY = NORTH + ((263, 470), (430, 470))
WIDE_ENOUGH = 16
GRID = 16
NEAR = 24
DIRECTIONS = ("left", "right", "up", "down")


def player():
    """Answer the walking player's (x, y)."""
    block = run.memory("system", PLAYER, 4)
    return (int.from_bytes(block[0:2], "little"),
            int.from_bytes(block[2:4], "little"))


def money():
    """Answer the wallet in dollars; the longword is high half first."""
    block = run.memory("system", MONEY, 4)
    return (int.from_bytes(block[0:2], "little") * 0x10000
            + int.from_bytes(block[2:4], "little"))


def step_towards(direction):
    """Hold one direction for a cell's worth of frames."""
    run.hold(1, [direction])
    run.step(STEP_FRAMES)
    run.release(1)
    run.step(1)


def walk(target):
    """Walk to a floor point, one axis per burst; answer where it stopped."""
    stalls = {"x": 0, "y": 0}
    kept = False
    for _ in range(WALK_TRIES):
        at = player()
        dx, dy = target[0] - at[0], target[1] - at[1]
        if abs(dx) <= CLOSE_ENOUGH and abs(dy) <= CLOSE_ENOUGH:
            break
        axis = "x" if abs(dx) >= abs(dy) else "y"
        if stalls[axis] >= STALL_LIMIT:
            axis = "y" if axis == "x" else "x"
        if not kept:
            run.checkpoint(BURST)
            kept = True
        if axis == "x":
            step_towards("right" if dx > 0 else "left")
        else:
            step_towards("down" if dy > 0 else "up")
        if player() == at:
            run.restore(BURST)
            stalls[axis] += 1
        else:
            run.checkpoint(BURST)
            stalls = {"x": 0, "y": 0}
    run.forget(BURST)
    return player()


def seek(target, budget=SEEK_BUDGET):
    """Walk depth-first under restore; the branch that arrives is the line."""
    taken = 0
    stack = [[cell(player()), list(DIRECTIONS), f"{SEEK_BRANCH}/{taken}"]]
    run.checkpoint(stack[0][2])
    seen = {stack[0][0]}
    spent = 0
    while stack and spent < budget:
        at = player()
        if abs(at[0] - target[0]) <= NEAR and abs(at[1] - target[1]) <= NEAR:
            break
        top = stack[-1]
        if not top[1]:
            run.forget(stack.pop()[2])
            if stack:
                run.restore(stack[-1][2])
            continue
        step_towards(top[1].pop())
        spent += 1
        moved = cell(player())
        if moved in seen:
            run.restore(top[2])
            continue
        seen.add(moved)
        taken += 1
        stack.append([moved, list(DIRECTIONS), f"{SEEK_BRANCH}/{taken}"])
        run.checkpoint(stack[-1][2])
    for node in stack:
        run.forget(node[2])
    return player()


def cell(at):
    """Answer the grid cell a point falls in."""
    return (at[0] // GRID, at[1] // GRID)


def enter():
    """Face the machine the player stands at and click it."""
    run.hold(1, ["up"])
    run.step(FACE_FRAMES)
    run.release(1)
    run.step(2)
    hand.press(run, "b", CLICK_FRAMES, SETTLE_FRAMES)


def inside(before):
    """Answer whether the click left the floor for a game screen."""
    now = int(run.observe()["difference"], 16)
    gap = bin(int(before, 16) ^ now).count("1")
    was = hand.at(run)
    step_towards("right")
    return hand.at(run) != was or gap > WIDE_ENOUGH


def stop(name, waypoints, target, play, leave):
    """Walk to a game from where the player is, play it once and judge it."""
    for waypoint in waypoints:
        walk(waypoint)
    reached = walk(target)
    off = [abs(reached[axis] - target[axis]) > NEAR for axis in (0, 1)]
    if any(off):
        reached = seek(target)
    before = run.observe()["difference"]
    enter()
    run.step(BANNER_WAIT)
    played = play() if inside(before) else None
    run.look()
    run.mark(name)
    run.step(MARK_SETTLE)
    if played is None:
        run.judge(f"{name} played", False,
                  f"at {reached} for {target}, the click opened nothing, "
                  f"wallet ${money()}")
    else:
        run.judge(f"{name} played", played.ok,
                  f"staked ${played.stake}, {played.outcome}, wallet "
                  f"${played.wallet} after")
    leave()
    return reached, bool(played and played.ok)


def main():
    """Play the tape, then tour the floor playing every game once."""
    import games
    run.play(TAPE)
    run.checkpoint(FLOOR)
    run.mark("floor")
    run.look()
    run.expect("on the floor", money() == 2000, f"wallet ${money()}")
    for game, waypoints, spot, play, leave in games.GAMES:
        print(game, stop(game, waypoints, spot, play, leave), flush=True)


if __name__ == "__main__":
    main()
