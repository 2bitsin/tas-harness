"""One game from power on: the tape, then a press at every rest."""

import pathlib
import sys

import tash

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import engine
import state

SEAT = 0
RIVAL = 1
GAME_BUDGET = 420000
SETTLE_AFTER_TAPE = 600
BUILD_FLOOR = 2
STALL_PRESSES = 40
BACK_OUT_AFTER = 2
FULL_STREET = 4
VICTORY_LIMIT = 3000

# The bankrupt plate and the winner's plate, both read off a rigged game
# that ended in one landing; the exact hash animates, the perceptual does
# not.
BANKRUPT = "perceptual_hash 497072db0b360dcd within 6"
VICTORY = "perceptual_hash 47b938c18c1e71dc within 6"

run = tash.run


def interesting(before, after):
    return (before["current"] != after["current"]
            or before["left"] != after["left"]
            or [p["square"] for p in before["players"]]
            != [p["square"] for p in after["players"]]
            or [p["cash"] for p in before["players"]]
            != [p["cash"] for p in after["players"]])


def wanted(run):
    """Answer a held group short of houses and the number to buy on it."""
    cash = state.player(run, SEAT)["cash"]
    for group in state.monopolies(run, SEAT):
        houses = state.houses_on(run, group)
        room = FULL_STREET * len(houses) - sum(houses)
        price = state.HOUSE_PRICE[group]
        afford = max(0, cash // price - BUILD_FLOOR)
        if room and afford:
            return group, min(room, afford)
    return None, 0


def build(run, builds):
    group, count = wanted(run)
    if not group:
        return True
    started = run.frames()
    before, after = engine.build(run, group, count)
    order = {"frame": started, "group": group, "asked": count,
             "before": before, "after": after,
             "frames": run.frames() - started}
    builds.append(order)
    print("build", order, flush=True)
    return sum(after) > sum(before)


def turns_from(rows):
    """Split the rows where the mover changes; answer a row a turn."""
    turns = []
    start = rows[0]
    for row in rows[1:]:
        if row["current"] != start["current"]:
            turns.append({"seat": start["current"],
                          "frames": row["frame"] - start["frame"],
                          "cash": [p["cash"] for p in row["players"]],
                          "owned": row["owned"]})
            start = row
    return turns


def show(row, spent):
    print("row", row["frame"], "seat", row["current"], "cash",
          [p["cash"] for p in row["players"]], "square",
          [p["square"] for p in row["players"]], "owned", row["owned"],
          "dice", row["dice"], "left", row["left"], "rest", spent,
          flush=True)


def play(run, budget, builds):
    """Confirm at every rest, waiting longer after a press that changed
    nothing, and back out of a menu a stray press opened."""
    rows = [engine.sample(run)]
    mover = rows[0]["current"]
    building = True
    quiet = 0
    while run.frames() < budget and not engine.decided(run):
        spent = engine.settle(run)
        now = engine.sample(run)
        if now["current"] != mover:
            mover = now["current"]
            if building and mover == SEAT:
                building = build(run, builds)
        stuck = quiet >= BACK_OUT_AFTER and engine.in_menu(run)
        engine.press(run, engine.CANCEL if stuck else engine.CONFIRM,
                     after=engine.MENU_GAP if not quiet else engine.WAIT_GAP)
        after = engine.sample(run)
        if interesting(rows[-1], after) or spent >= engine.SETTLE_LIMIT:
            rows.append(after)
            show(after, spent)
            quiet = 0
        else:
            quiet += 1
            if quiet >= STALL_PRESSES:
                return rows, run.observe()["perceptual"]
    return rows, None


def main():
    run.mark("tape")
    played = run.play(HERE / "tapes" / "title-to-first-roll.yaml")
    run.step(SETTLE_AFTER_TAPE)
    opening = run.observe()
    run.expect("the tape reached the first roll", played["segments"] == 10,
               f"{played['segments']} segments, {played['frames']} frames")
    run.expect("the player holds the turn", "watch current_player equal 0",
               f"exact {opening['exact']}")
    run.expect("the purse opens on 1500", "watch p1_cash equal 1500",
               f"rival {run.watch('p2_cash')}")
    run.checkpoint("first-roll")
    run.mark("game")
    builds = []
    rows, stalled = play(run, GAME_BUDGET, builds)
    turns = turns_from(rows)
    mine = [t["frames"] for t in turns if t["seat"] == SEAT]
    theirs = [t["frames"] for t in turns if t["seat"] == RIVAL]
    won = engine.decided(run) and not engine.lost(run)
    if won:
        run.run_until(VICTORY, VICTORY_LIMIT)
    for turn in turns:
        print("turn", turn["seat"], turn["frames"], turn["cash"],
              turn["owned"], flush=True)
    print("turns", len(turns), "mine", len(mine), "theirs", len(theirs),
          flush=True)
    if mine and theirs:
        print("mean mine", sum(mine) // len(mine),
              "mean theirs", sum(theirs) // len(theirs), flush=True)
    print("stalled on", stalled, flush=True)
    print("frames", run.frames(), "won", won,
          "purses", [state.player(run, s)["cash"] for s in (SEAT, RIVAL)],
          "owned", [len(state.owned_by(run, s)) for s in (SEAT, RIVAL)],
          flush=True)
    run.expect("the game is decided", "watch players_left equal 1",
               f"{len(turns)} turns, {run.frames()} frames", raises=False)
    run.judge("a game was played to the session's limit", True,
              f"{len(turns)} turns in {run.frames()} frames, won {won}, "
              f"stalled on {stalled}")
    run.report()


main()
