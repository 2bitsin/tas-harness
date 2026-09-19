"""Every engine number in the README, measured again from power on."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import engine
import state

TAPE = "examples/scooby/tapes/title-to-control.yaml"
SETTLE = 30
SWEEP_LEFT = 10
SWEEP_RIGHT = 240
SWEEP_ROW = 80
SCENE_BOX = (0, 24, 244, 152)
SCENE_STEP = 4
DETERMINISM_TRIES = 3
SWEEP_LIMIT = 400
ROOM_REST = 400
WALK_FRAMES = 40
REST_FRAMES = 20
FREE_FRAMES = 12


def to_control(run):
    played = run.play(TAPE)
    run.step(SETTLE)
    run.checkpoint("control")
    return played


def cursor_speed(run, cursor, words):
    """Answer the frames a crosshair crossing takes, held then tapped."""
    cursor.raise_cursor()
    cursor.move(SWEEP_LEFT, SWEEP_ROW)
    start = run.observe()["frame"]
    run.hold(1, ["right"])
    for _ in range(SWEEP_LIMIT):
        run.step(1)
        if words.cursor()[0] >= SWEEP_RIGHT:
            break
    run.release(1)
    run.step(1)
    held = run.observe()["frame"] - start
    cursor.move(SWEEP_LEFT, SWEEP_ROW)
    start = run.observe()["frame"]
    taps = 0
    for _ in range(SWEEP_LIMIT):
        cursor.tap("right", engine.MASH_DOWN, engine.MASH_UP)
        taps += 1
        if words.cursor()[0] >= SWEEP_RIGHT:
            break
    return held, run.observe()["frame"] - start, taps


def caption_cost(run, cursor, words):
    """Answer the frames a looked-at line costs, mashed and untouched."""
    run.restore("cafe")
    run.step(ROOM_REST)
    cursor.raise_cursor()
    picked = cursor.pick_verb(state.LOOK)
    said, at = cursor.rest_on_any("Look at ", SCENE_BOX, SCENE_STEP)
    run.checkpoint("cafe-look-radio")
    mashed, shown = cursor.click_and_read()
    run.restore("cafe-look-radio")
    start = run.observe()["frame"]
    run.hold(1, [engine.B])
    run.step(engine.CLICK_FRAMES)
    run.release(1)
    seen = False
    for _ in range(engine.CAPTION_LIMIT):
        run.step(2)
        if cursor.caption() >= engine.CAPTION_LEAST:
            seen = True
        elif seen:
            break
    return (picked, said, at), mashed, shown, \
        run.observe()["frame"] - start


def pick_up(run, cursor, words):
    """Take the bottle of oil; answer the line before it and the frames."""
    run.restore("cafe")
    run.step(ROOM_REST)
    cursor.raise_cursor()
    cursor.pick_verb(state.TAKE)
    _, at = cursor.rest_on_any("Take Bottle of Oil", SCENE_BOX, SCENE_STEP)
    before = words.line()
    start = run.observe()["frame"]
    run.hold(1, [engine.B])
    run.step(engine.CLICK_FRAMES)
    run.release(1)
    run.step(engine.CAPTION_LIMIT)
    run.checkpoint("cafe-oil")
    return at, before, run.observe()["frame"] - start


def rooms(run, cursor, words):
    """Walk the three rooms a held direction reaches; answer their costs."""
    walk = {}
    run.restore("control")
    cursor.lower_cursor()
    run.checkpoint("lobby")
    for name, direction in (("hallway", "up"), ("cafe", "down")):
        run.restore("lobby")
        cursor.lower_cursor()
        walk[name] = cursor.walk_through(direction)
        run.step(ROOM_REST)
        run.checkpoint(name)
    return walk


def determinism(run, cursor):
    """Answer the hash the same inputs reach from one checkpoint, each try."""
    hashes = []
    for _ in range(DETERMINISM_TRIES):
        run.restore("lobby")
        run.step(2)
        run.hold(1, ["right"])
        run.step(WALK_FRAMES)
        run.release(1)
        run.step(REST_FRAMES)
        cursor.tap(engine.B)
        run.hold(1, ["down"])
        run.step(WALK_FRAMES)
        run.release(1)
        run.step(REST_FRAMES)
        hashes.append(run.observe()["exact"])
    return hashes


def free_running(run):
    """Answer the words that move every frame with the pad untouched."""
    hunt = run.hunt("system", width=2, endian="little")
    run.restore("lobby")
    run.step(ROOM_REST)
    hunt.step("changed")
    for _ in range(FREE_FRAMES):
        run.step(1)
        left = hunt.step("changed")
    return left, hunt.candidates()


def main():
    run = tash.run
    words = state.State(run)
    cursor = engine.Cursor(run)
    played = to_control(run)
    run.expect("the tape reaches the lobby with the bar up",
               "watch room equal %d" % state.LOBBY)
    run.expect("the verb bar is drawn", cursor.playing())
    walk = rooms(run, cursor, words)
    held, tapped, taps = cursor_speed(run, cursor, words)
    at, mashed, shown, alone = caption_cost(run, cursor, words)
    took, line, cost = pick_up(run, cursor, words)
    hashes = determinism(run, cursor)
    left, free = free_running(run)
    run.expect("the crosshair is twice as quick held as tapped",
               tapped >= 2 * held - 4)
    run.expect("a mashed caption is far cheaper than an untouched one",
               mashed * 4 < alone)
    run.expect("the same inputs reach one hash", len(set(hashes)) == 1)
    run.judge("the bottle of oil is taken", took is not None,
              "rested on %r at %s, the click cost %d frames"
              % (line, took, cost))
    print("tape", played, flush=True)
    print("rooms", walk, flush=True)
    print("cursor held %d tapped %d over %d taps" % (held, tapped, taps),
          flush=True)
    print("caption at %s mashed %d shown %s untouched %d"
          % (at, mashed, shown, alone), flush=True)
    print("determinism", hashes, flush=True)
    print("free running words", left, [hex(a) for a, _ in free], flush=True)
    run.report()


if __name__ == "__main__":
    main()
