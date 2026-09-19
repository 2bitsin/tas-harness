"""WWF WrestleMania: the tape to the first bell, then a match mashed out."""

import pathlib

import tash

HERE = pathlib.Path(__file__).resolve().parent

KICK = "y"
TAP_FRAMES = 3
RECOVER_FRAMES = 14
STEP_FRAMES = 4

# A punch or a kick connects at this distance or less, measured by walking in
# a frame at a time and kicking at each gap until the opponent's health moved.
REACH = 85

MATCH_BUDGET = 12000
FALL_SETTLE_FRAMES = 240


def gap(run):
    """How far the opponent is to the right of the player, in pixels."""
    return run.watch("cpu_x") - run.watch("p1_x")


def close_in(run):
    """Walk one step toward the opponent."""
    run.hold(1, ["right" if gap(run) > 0 else "left"])
    run.step(STEP_FRAMES)
    run.release(1)


def swing(run):
    """Throw one kick and stand still long enough to recover."""
    run.hold(1, [KICK])
    run.step(TAP_FRAMES)
    run.release(1)
    run.step(RECOVER_FRAMES)


def in_the_ring(run):
    """Whether a match is still being fought."""
    return run.watch("match") == 1 and run.watch("timer_tens") is not None


def main():
    run = tash.run

    run.mark("tape")
    played = run.play(HERE / "tapes" / "title-to-first-bell.yaml")
    at_the_bell = run.observe()["watches"]

    run.expect("the tape reached the first bell", played["segments"] == 4,
               f"{played['segments']} segments, {played['frames']} frames")
    run.expect("match one of the ladder", at_the_bell["match"] == 1)
    run.expect("both wrestlers are whole",
               at_the_bell["p1_health"] == at_the_bell["cpu_health"] == 164,
               f"p1 {at_the_bell['p1_health']}, "
               f"cpu {at_the_bell['cpu_health']}")

    run.mark("bell")
    started = run.frames()
    falls = 0
    health = (at_the_bell["p1_health"], at_the_bell["cpu_health"])
    while run.frames() - started < MATCH_BUDGET and in_the_ring(run):
        if abs(gap(run)) > REACH:
            close_in(run)
            continue
        swing(run)
        now = (run.watch("p1_health"), run.watch("cpu_health"))
        if 0 in now and 0 not in health:
            falls += 1
            run.mark(f"fall {falls}", "falls")
            run.step(FALL_SETTLE_FRAMES)
        health = now

    run.mark("match over")
    run.look()
    fought = run.frames() - started
    run.judge("one match, bell to bell", True,
              f"{fought} frames, {falls} falls, "
              f"p1 {run.watch('p1_health')}, cpu {run.watch('cpu_health')}")
    print("crude match:", fought, "frames,", falls, "falls,",
          "match watch", run.watch("match"))


main()
