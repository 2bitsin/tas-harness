"""One play of every game on the floor, each for the least it will take."""

import collections
import os
import sys
import tash

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import engine
import hand
import scenario

run = tash.run
Play = collections.namedtuple("Play", "ok stake outcome wallet")
PURSE = "a"
OUT = ("start", "down")
OUT_FRAMES = 20
OUT_AFTER = 300
OUT_TRIES = 4
PICK = "b"
DEAL = "y"
STAND = "a"
CURSOR_BYTES = 6
MONEY_BYTES = 4
PANEL_HOLD = 3
PANEL_AFTER = 5
PANEL_TRIES = 40
PANEL_CLOSE = 8
PANEL_STALLS = 4
AIM_ROUNDS = 6
AIM_CLOSE = 6
STILL_FRAMES = 6
STILL_TIMEOUT = 900
PRESS_FRAMES = 10
ANSWER_FRAMES = 90
DROP_FRAMES = 60
PURSE_FRAMES = 30
DENOMINATIONS = (1, 5, 25, 100, 500)
RACK_BURSTS = 8
RACK_FRAMES = 8
RACK_PITCH = 32
RACK_TRIES = 30
CHIP_LIMIT = 12
BANNER_TRIES = 8
COIN_FRAMES = 150
SLOT_COIN = (184, 304)
SLOT_REELS = (0xD432, 0xD436, 0xD43A)
SLOT_FRAMES = 260
STOP_SHIFT = 8
POKER_COIN = (232, 368)
POKER_DEAL = (232, 392)
POKER_FRAMES = 240
TABLE_MINIMUM = {"craps": 10, "blackjack-high": 100,
                 "blackjack-low": 25}
CIRCLE = (129, 128)
TRAY_Y = 184
BLACKJACK_FRAMES = 480
BLACKJACK_TRIES = 3
PASS_LINE = (256, 296)
CRAPS_TAKE = 300
CRAPS_THROW = 600
CRAPS_PRESSES = 3
CRAPS_TABLE_Y = 200
CRAPS_ROLLS = 6
ROULETTE_CHIPS = 1
HIGH_LEAD = 0
HIGH_NUDGE = 60
HIGH_TRIES = 4
DEAD_POCKET = 0
KENO_COIN = (78, 34)
KENO_NUMBER = (106, 140)
KENO_DONE = (44, 44)
KENO_EXIT = (44, 92)
KENO_DOOR = (439, 390)
KENO_OUT = (439, 538)
LOW_TABLE = (247, 534)
NORTH_TAIL = scenario.NORTH[-1]
KENO_FRAMES = 240
KENO_DRAW = 1200
KENO_TRIES = 3
CASHIER_BALANCE = (204, 48)
CASHIER_EXIT = (204, 112)
CASHIER_OK = (204, 112)
CASHIER_FRAMES = 180
HORSE_BET = (228, 56)
HORSE_KEYS = ((176, 184),)
HORSE_WIN = (228, 56)
HORSE_PICK = (228, 64)
HORSE_SUBMIT = (228, 136)
HORSE_YES = (228, 136)
HORSE_STAKE = 2
HORSE_FRAMES = 180
RACE_STEP = 1500
RACE_LIMIT = 12
BACK = ((752, 560), (752, 802))


def creep(x, y):
    """Steer the hand towards (x, y) in the lattice's own 16-pixel bursts."""
    stalls = [0, 0]
    for _ in range(PANEL_TRIES):
        at = hand.at(run)
        away = [x - at[0], y - at[1]]
        going = [axis for axis in (0, 1) if abs(away[axis]) > PANEL_CLOSE
                 and stalls[axis] < PANEL_STALLS]
        if not going:
            break
        axis = max(going, key=lambda n: abs(away[n]))
        run.hold(1, [hand.NAMES[axis][1 if away[axis] > 0 else 0]])
        run.step(PANEL_HOLD)
        run.release(1)
        run.step(PANEL_AFTER)
        moved = hand.at(run)
        stalls[axis] = 0 if moved[axis] != at[axis] else stalls[axis] + 1
    return hand.at(run)


def aim(x, y):
    """Creep, wait out the glide, and creep again until the hand is on it."""
    at = hand.at(run)
    for _ in range(AIM_ROUNDS):
        creep(x, y)
        run.memory_stable("system", hand.CURSOR, CURSOR_BYTES,
                          STILL_FRAMES, STILL_TIMEOUT)
        at = hand.at(run)
        if abs(at[0] - x) <= AIM_CLOSE and abs(at[1] - y) <= AIM_CLOSE:
            break
    return at


def click(spot, after=ANSWER_FRAMES, button=PICK):
    """Point at a panel spot and press; answer where the hand pressed."""
    at = aim(*spot)
    hand.press(run, button, PRESS_FRAMES, after)
    return at


def slide(x):
    """Move the hand along the row it is already on, and let it stop."""
    for _ in range(RACK_TRIES):
        at = hand.at(run)
        if abs(at[0] - x) <= PANEL_CLOSE:
            break
        run.hold(1, [hand.NAMES[0][1 if x > at[0] else 0]])
        run.step(PANEL_HOLD)
        run.release(1)
        run.step(PANEL_AFTER)
        if hand.at(run)[0] == at[0]:
            break
    run.memory_stable("system", hand.CURSOR, CURSOR_BYTES,
                      STILL_FRAMES, STILL_TIMEOUT)
    return hand.at(run)


def wake():
    """Wait out a table's welcome banner, which holds the hand still."""
    for _ in range(BANNER_TRIES):
        at = hand.at(run)
        run.hold(1, [hand.NAMES[0][1]])
        run.step(PANEL_HOLD)
        run.release(1)
        run.step(PANEL_AFTER)
        if hand.at(run) != at:
            return True
        run.step(engine.BANNER_FRAMES)
    return False


def clamp():
    """Hold right to the panel's right edge; answer where the hand stopped."""
    for _ in range(RACK_BURSTS):
        run.hold(1, [hand.NAMES[0][1]])
        run.step(RACK_FRAMES)
        run.release(1)
        run.step(PANEL_AFTER)
    run.memory_stable("system", hand.CURSOR, CURSOR_BYTES,
                      STILL_FRAMES, STILL_TIMEOUT)
    return hand.at(run)


def settled():
    """Wait for the wallet to stop moving and answer it."""
    run.memory_stable("system", scenario.MONEY, MONEY_BYTES,
                      STILL_FRAMES, STILL_TIMEOUT)
    return scenario.money()


def stocked():
    """Answer the racks the wallet holds: its balance decomposed greedily."""
    left = scenario.money()
    racks = []
    for value in reversed(DENOMINATIONS):
        if left >= value:
            racks.append(value)
            left %= value
    return tuple(racks)


def chip_for(want):
    """Answer the biggest stocked chip no larger than what is left to stake."""
    fits = [value for value in stocked() if value <= want]
    return fits[0] if fits else stocked()[-1]


def rack_x(value, clamp):
    """Answer the x of a denomination's rack, the $500 rack being the clamp."""
    return clamp - RACK_PITCH * (len(DENOMINATIONS) - 1
                                 - DENOMINATIONS.index(value))


def drop_to(y):
    """Press down until the hand is on the row the racks sit on."""
    for _ in range(RACK_TRIES):
        at = hand.at(run)
        if abs(at[1] - y) <= PANEL_CLOSE:
            break
        run.hold(1, [hand.NAMES[1][1]])
        run.step(PANEL_HOLD)
        run.release(1)
        run.step(PANEL_AFTER)
        if hand.at(run) == at:
            break
    run.memory_stable("system", hand.CURSOR, CURSOR_BYTES,
                      STILL_FRAMES, STILL_TIMEOUT)
    return hand.at(run)


def lift(value, tray=0):
    """Take one chip off its rack; answer what the wallet paid for it."""
    before = scenario.money()
    if tray:
        drop_to(tray)
    else:
        hand.press(run, PURSE, PRESS_FRAMES, PURSE_FRAMES)
    row = clamp()
    spot = rack_x(value, row[0])
    if spot != row[0]:
        slide(spot)
    hand.press(run, PICK, PRESS_FRAMES, PURSE_FRAMES)
    if not tray:
        hand.press(run, PURSE, PRESS_FRAMES, PURSE_FRAMES)
    return before - scenario.money()


def wager(spot, minimum, tray=0):
    """Carry chips onto a betting spot until the bet is the table's minimum."""
    staked = 0
    for _ in range(CHIP_LIMIT):
        if staked >= minimum:
            break
        taken = lift(chip_for(minimum - staked), tray)
        if not taken:
            break
        staked += taken
        click(spot, after=DROP_FRAMES)
    return staked


def reels():
    """Answer the three reel stops, each the high byte of its word."""
    return tuple(int.from_bytes(run.memory("system", address, 2), "little")
                 >> STOP_SHIFT for address in SLOT_REELS)


def slot():
    """Feed one coin, pull the lever and answer the reels it stopped on."""
    before = scenario.money()
    click(SLOT_COIN, after=COIN_FRAMES)
    staked = before - scenario.money()
    clamp()
    hand.press(run, PICK, PRESS_FRAMES, SLOT_FRAMES)
    after = settled()
    return Play(staked > 0, staked, f"the reels stopped on {reels()}", after)


def video_poker():
    """Feed one coin, deal a hand and stand on it."""
    before = scenario.money()
    click(POKER_COIN, after=COIN_FRAMES)
    staked = before - scenario.money()
    click(POKER_DEAL, after=POKER_FRAMES)
    click(POKER_DEAL, after=POKER_FRAMES)
    after = settled()
    return Play(staked > 0, staked, "a hand dealt and stood on", after)


def blackjack(name):
    """Bet the table's minimum, take one hand and play it to the end."""
    before = scenario.money()
    staked = wager(CIRCLE, TABLE_MINIMUM[name], TRAY_Y)
    drop_to(TRAY_Y)
    for _ in range(BLACKJACK_TRIES):
        run.step(BLACKJACK_FRAMES)
        hand.press(run, STAND, PRESS_FRAMES, BLACKJACK_FRAMES)
    after = settled()
    return Play(staked >= TABLE_MINIMUM[name], staked,
                f"a hand dealt and played out, {after - before:+d} on it",
                after)


def blackjack_high():
    """One hand at the $100 table."""
    return blackjack("blackjack-high")


def blackjack_low():
    """One hand at the $25 table."""
    return blackjack("blackjack-low")


def shoot():
    """Take the dice, throw them, and wait for the hand back at the table."""
    hand.press(run, DEAL, PRESS_FRAMES, CRAPS_TAKE)
    for _ in range(CRAPS_PRESSES):
        if hand.at(run)[1] > CRAPS_TABLE_Y:
            break
        hand.press(run, PICK, PRESS_FRAMES, CRAPS_THROW)


def craps():
    """Back the pass line for the minimum and shoot until it settles."""
    wake()
    before = scenario.money()
    staked = wager(PASS_LINE, TABLE_MINIMUM["craps"])
    live = before - staked
    rolls = 0
    while rolls < CRAPS_ROLLS and scenario.money() == live:
        shoot()
        rolls += 1
    after = settled()
    return Play(staked >= TABLE_MINIMUM["craps"] and rolls > 0, staked,
                f"the dice thrown {rolls} times, "
                f"{after - live:+d} on the line",
                after)


def roulette_round(name):
    """Let the engine scout the pocket under restore and bet its chip."""
    wake()
    before = scenario.money()
    drawn = engine.scouted(f"{name}/scout", ROULETTE_CHIPS)[0]
    after = engine.credited()
    run.forget(f"{name}/scout")
    return Play(after > before, ROULETTE_CHIPS * engine.CHIP,
                f"the ball stopped on {drawn}, {after - before:+d} on it",
                after)


def roulette():
    """One scouted round at the open floor's table."""
    return roulette_round("roulette")


def high_scout(name, count):
    """Scout on the lead this table answers, then bet the pocket it drew."""
    lead = HIGH_LEAD
    for _ in range(HIGH_TRIES):
        run.checkpoint(name)
        engine.stake(count, engine.REHEARSAL)
        run.step(lead)
        drawn, moved = engine.spin()
        run.restore(name)
        run.forget(name)
        if moved and drawn != DEAD_POCKET:
            engine.stake(count, drawn)
            run.step(lead)
            engine.spin()
            return drawn
        lead += HIGH_NUDGE
    return None


def roulette_high():
    """One scouted round at the high-limit table, where the player stays."""
    run.step(engine.BANNER_FRAMES)
    before = scenario.money()
    drawn = high_scout("roulette-high/scout", ROULETTE_CHIPS)
    after = engine.credited()
    return Play(after > before, ROULETTE_CHIPS * engine.CHIP,
                f"the ball stopped on {drawn}, {after - before:+d} on it",
                after)


def keno():
    """Mark one number for a coin, register the ticket, watch the board."""
    before = scenario.money()
    for _ in range(KENO_TRIES):
        click(KENO_COIN, after=KENO_FRAMES)
        if scenario.money() < before:
            break
    click(KENO_NUMBER, after=KENO_FRAMES)
    click(KENO_DONE, after=KENO_FRAMES)
    staked = before - scenario.money()
    run.step(KENO_DRAW)
    after = settled()
    return Play(staked > 0, staked, "a ticket registered and drawn against",
                after)


def keno_exit():
    """The keno board has its own exit button beside the ticket's Done."""
    click(KENO_EXIT, after=KENO_FRAMES)
    if not outside():
        leave()


def cashier():
    """Ask the cashier's own screen for the account, which costs nothing."""
    click(CASHIER_BALANCE, after=CASHIER_FRAMES)
    after = settled()
    click(CASHIER_OK, after=CASHIER_FRAMES)
    return Play(True, 0, f"the screen reading YOUR BALANCE IS ${after}", after)


def horses():
    """Put the terminal's minimum on a horse to win and let the race run."""
    before = scenario.money()
    click(HORSE_BET, after=HORSE_FRAMES)
    for key in HORSE_KEYS:
        click(key, after=HORSE_FRAMES)
    click(HORSE_WIN, after=HORSE_FRAMES)
    click(HORSE_PICK, after=HORSE_FRAMES)
    click(HORSE_SUBMIT, after=HORSE_FRAMES)
    click(HORSE_YES, after=HORSE_FRAMES)
    staked = before - scenario.money()
    run.step(RACE_STEP * RACE_LIMIT)
    after = settled()
    return Play(staked == HORSE_STAKE, staked,
                f"a race run, {after - before:+d} on it", after)


def outside():
    """Answer whether the player can walk, which only the floor lets him."""
    was = scenario.player()
    scenario.step_towards("right")
    return scenario.player() != was


def leave():
    """Start and down together, the exit every game's banner offers."""
    for _ in range(OUT_TRIES):
        run.hold(1, list(OUT))
        run.step(OUT_FRAMES)
        run.release(1)
        run.step(OUT_AFTER)
        if outside():
            break


def log_out():
    """The horse terminal gives the floor back to a click on LOG OUT only."""
    clamp()
    hand.press(run, PICK, PRESS_FRAMES, HORSE_FRAMES)


def stay():
    """The high-limit table is where the round loop runs; the player stays."""


GAMES = (
    ("cashier", (), (783, 822), cashier, leave),
    ("video-poker", (), (863, 802), video_poker, leave),
    ("horse-racing", (), (752, 546), horses, log_out),
    ("jungle-slots", BACK, (1055, 802), slot, leave),
    ("triple-jackpot-slots", (), (552, 802), slot, leave),
    ("roulette", (), (344, 838), roulette, leave),
    ("craps", (), (192, 834), craps, leave),
    ("blackjack-high", scenario.WEST, (328, 656), blackjack_high, leave),
    ("blackjack-low", (), LOW_TABLE, blackjack_low, leave),
    ("keno", (), KENO_DOOR, keno, keno_exit),
    ("roulette-high", (KENO_DOOR, KENO_OUT, LOW_TABLE, NORTH_TAIL),
     engine.TABLE, roulette_high, stay),
)
