"""Every game once, then the scouted hundred-chip stack to the wallet's cap."""

import os
import sys
import time
import tash

HERE = (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals()
        else os.path.join(os.getcwd(), "examples", "caesars"))
sys.path.insert(0, HERE)
import engine
import games
import hand
import scenario

run = tash.run
GOAL = 999_999_999
# The game clamps the wallet at seven digits; README "The cap", measured.
CAP = 9_999_999
MILLION = 1_000_000
ROUND_LIMIT = 40
WALLET_HOLD = 300


def scouted_round(number, count):
    """One round of engine.py's loop, marked in its group and not judged."""
    name = f"round-{number}"
    drawn = engine.scouted(name, count)[0]
    run.mark(name, group="round")
    return drawn, engine.credited()


def main():
    started = time.time()
    run.play(scenario.TAPE)
    run.checkpoint(scenario.FLOOR)
    run.mark("floor")
    run.expect("on the floor with the account's $2,000",
               scenario.money() == 2000, f"wallet ${scenario.money()}")
    played = 0
    for game, waypoints, spot, play, leave in games.GAMES:
        at = run.frames()
        reached, ok = scenario.stop(game, waypoints, spot, play, leave)
        played += ok
        print(f"game {game} at {reached} played {ok} "
              f"wallet ${scenario.money()} in {run.frames() - at} frames "
              f"total {run.frames()}", flush=True)
    run.mark("at the high-limit roulette")
    print(f"table wallet ${scenario.money()} frames {run.frames()} "
          f"{time.time() - started:.0f} s", flush=True)

    number = 0
    million = scenario.money() // MILLION
    while scenario.money() < CAP and number < ROUND_LIMIT:
        number += 1
        before = scenario.money()
        count = min(engine.STACK_CAP, before // engine.CHIP)
        at = run.frames()
        tick = time.time()
        drawn, after = scouted_round(number, count)
        print(f"round {number} {count} chips on {drawn} "
              f"${before} -> ${after} in {run.frames() - at} frames "
              f"{time.time() - tick:.1f} s total {run.frames()} frames "
              f"{time.time() - started:.0f} s", flush=True)
        if after // MILLION > million:
            million = after // MILLION
            run.look()

    wallet = scenario.money()
    hand.press(run, "a", engine.OPEN_FRAMES, WALLET_HOLD)
    run.look()
    run.judge("the wallet holds the most the game allows",
              wallet >= CAP and played == len(games.GAMES),
              f"${wallet} after {number} rounds, {played} of "
              f"{len(games.GAMES)} games played; the goal of ${GOAL} "
              f"cannot be held: the game clamps the wallet at ${CAP}, "
              f"judged there")
    print(f"done wallet ${wallet} frames {run.frames()} "
          f"{time.time() - started:.0f} s", flush=True)


main()
