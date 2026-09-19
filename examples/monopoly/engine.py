"""Presses driven by the ram state: one function for every prompt."""

import state

PORT = 1
CONFIRM = "y"
CANCEL = "a"
BID_OUT = "b"
UP = "up"
DOWN = "down"
RIGHT = "right"

PRESS_FRAMES = 6
STILL_FRAMES = 30
SETTLE_LIMIT = 6000
MENU_GAP = 24
# A screen that is still arriving swallows a press and starts over; the
# vault needs this many frames after one before the next is worth placing.
WAIT_GAP = 120
BROWSE_LIMIT = 44
MENU_LIMIT = 8
CLOSE_LIMIT = 6
ANSWER_LIMIT = 24
SEAT = 0
IDLE_DIE = 7
TURN_LIMIT = 4000

# The options panel fills the middle of the screen with one white; the
# board there holds about 330 of those pixels, a deed or a card about
# 5,300, an auction 7,800 and the options list 10,000 of 12,288.
PANEL_CROP = "96,64,128,96"
PANEL_WHITE = (232, 236, 232)
PANEL_WITHIN = 8
MENU_PIXELS = 9000
CARD_PIXELS = 3000

CURSOR = 0x86c4
OPTIONS = ("buildings", "mortgage", "trade", "deeds", "general")
BUILDINGS = ("buy-houses", "buy-hotels", "sell-houses", "sell-hotels")
MORTGAGING = ("mortgage", "un-mortgage")

AUCTION_SQUARE = 0x8860
AUCTION_ASKING = 0x8862
AUCTION_BID = 0x8864
AUCTION_BY = 0x8866
# Left alone, an auction closes itself and hands the deed over at this.
AUCTION_FLOOR = 14

CHANCE_DECK = 0x86c8
CHANCE_CURSOR = 0x86d9
CHEST_DECK = 0x86da
CHEST_CURSOR = 0x86eb
CHEST_HOLDER = 0x86ea
CHANCE_HOLDER = 0x86ed
DRAWN_CARD = 0x86ee
DECK_SIZE = 16
CHANCE_FIRST = 16
NOBODY = 0xff
CHEST_SQUARES = (2, 17, 33)
CHANCE_SQUARES = (7, 22, 36)

JAIL_SQUARE = 10
REC_JAIL = 16
JAIL_FINE = 50


def press(run, button=CONFIRM, frames=PRESS_FRAMES, after=MENU_GAP):
    run.hold(PORT, [button])
    run.step(frames)
    run.release(PORT, [button])
    run.step(after)


def settle(run, quiet=STILL_FRAMES, limit=SETTLE_LIMIT):
    """Step until the state block stops changing: the game wants a press."""
    address, count = state.STATE_BLOCK
    try:
        return run.memory_stable("system", address, count, quiet, limit)
    except RuntimeError:
        return limit


def answer(run, button=CONFIRM):
    """One press placed on a settled screen, so none of it is swallowed."""
    settle(run)
    press(run, button, after=0)
    return settle(run)


def panel(run):
    return run.colours(PANEL_CROP, PANEL_WHITE, PANEL_WITHIN)


def in_menu(run):
    return panel(run) >= MENU_PIXELS


def on_card(run):
    return CARD_PIXELS <= panel(run) < MENU_PIXELS


def swapped(raw):
    """Work ram is byte-swapped, so a byte array reads out pair by pair."""
    return [raw[index ^ 1] for index in range(len(raw))]


def sample(run):
    """One row of the game: frame, whose turn, purses, squares, deeds."""
    return {
        "frame": run.observe()["frame"],
        "current": state.current(run),
        "left": state.left(run),
        "players": [state.player(run, seat) for seat in (0, 1)],
        "dice": state.dice(run),
        "owned": [len(state.owned_by(run, seat)) for seat in (0, 1)],
    }


def decided(run):
    return state.left(run) <= 1


def lost(run, seat=SEAT):
    return state.record(run, seat)[state.REC_SEAT] == 0


def jailed(run, seat=SEAT):
    return state.record(run, seat)[REC_JAIL]


def roll_prompt(run, seat=SEAT):
    return (state.current(run) == seat
            and state.dice(run) == (IDLE_DIE, IDLE_DIE)
            and not in_menu(run))


def roll(run, offset=0):
    """Press at the frame the dice register was read for, then wait."""
    if offset:
        run.step(offset)
    press(run, CONFIRM, after=0)
    settle(run)
    return state.dice(run)


def deed_prompt(run, seat=SEAT):
    """The token stands on a buyable square nobody owns and a card is up."""
    square = state.player(run, seat)["square"]
    return (square in state.BUYABLE
            and state.deeds(run)[square]["owner"] == NOBODY
            and on_card(run))


def buy(run):
    return answer(run, CONFIRM)


def decline(run):
    return answer(run, BID_OUT)


def auction_prompt(run):
    return state.word(run, AUCTION_ASKING) > 0


def auction(run, ceiling, seat=SEAT, limit=ANSWER_LIMIT):
    """Bid the asking price while it is under the ceiling, else stand off."""
    square = state.word(run, AUCTION_SQUARE)
    for _ in range(limit):
        if not auction_prompt(run):
            break
        if state.word(run, AUCTION_ASKING) > ceiling:
            settle(run)
            continue
        cursor_to(run, seat)
        answer(run, CONFIRM)
    run.run_until("not watch die_1 equal 7 or watch die_1 equal 7",
                  PRESS_FRAMES)
    return square, state.deeds(run)[square]["owner"]


def open_options(run, item):
    """C opens WHOM AM I TALKING TO; A takes the seat, then the list."""
    answer(run, CANCEL)
    answer(run, CONFIRM)
    return cursor_to(run, OPTIONS.index(item)) and answer(run, CONFIRM)


def cursor_to(run, want, limit=MENU_LIMIT, button=DOWN):
    for _ in range(limit):
        if state.byte(run, CURSOR) == want:
            return True
        answer(run, button)
    return state.byte(run, CURSOR) == want


def browse_to(run, squares, limit=BROWSE_LIMIT):
    seen = set()
    for _ in range(limit):
        here = state.byte(run, CURSOR)
        if here in squares:
            return True
        if here in seen:
            return False
        seen.add(here)
        answer(run, RIGHT)
    return False


def close_menu(run, times=CLOSE_LIMIT):
    """Back out a press at a time; a cancel at the board reopens the menu."""
    for _ in range(times):
        if not in_menu(run):
            return True
        answer(run, CANCEL)
    return not in_menu(run)


def build(run, group, count, kind="buy-houses"):
    """Buy on a held group through OPTIONS, BUILDINGS, the deed browser."""
    before = state.houses_on(run, group)
    open_options(run, "buildings")
    cursor_to(run, BUILDINGS.index(kind))
    answer(run, CONFIRM)
    answer(run, CONFIRM)
    if browse_to(run, state.GROUPS[group]):
        for _ in range(count - 1):
            answer(run, RIGHT)
        answer(run, CONFIRM)
    close_menu(run)
    return before, state.houses_on(run, group)


def mortgage(run, square, kind="mortgage"):
    """Raise or repay a loan on one deed through OPTIONS, MORTGAGE."""
    open_options(run, "mortgage")
    cursor_to(run, MORTGAGING.index(kind))
    answer(run, CONFIRM)
    done = browse_to(run, [square]) and answer(run, CONFIRM) is not None
    close_menu(run)
    return done and state.deeds(run)[square]["mortgaged"] == (
        kind == "mortgage")


def unmortgage(run, square):
    return mortgage(run, square, "un-mortgage")


def deck_of(run, kind):
    """The sixteen ids in draw order, and where the cursor stands."""
    if kind == "chance":
        raw, at = run.memory("system", CHANCE_DECK, DECK_SIZE), CHANCE_CURSOR
        first = CHANCE_FIRST
    else:
        raw, at = run.memory("system", CHEST_DECK, DECK_SIZE), CHEST_CURSOR
        first = 0
    return [card + first for card in swapped(raw)], state.byte(run, at)


def next_card(run, kind):
    """The id the next draw of that deck will turn up, before the square."""
    cards, at = deck_of(run, kind)
    return cards[at % DECK_SIZE]


def drawn_card(run):
    return state.word(run, DRAWN_CARD)


def holds_pardon(run, seat=SEAT):
    return (state.byte(run, CHEST_HOLDER) == seat
            or state.byte(run, CHANCE_HOLDER) == seat)


def card_prompt(run):
    square = state.player(run, state.current(run))["square"]
    return square in CHEST_SQUARES + CHANCE_SQUARES and on_card(run)


def acknowledge(run):
    """Rent, a card, a tax: one press on the settled panel dismisses it."""
    return answer(run, CONFIRM)


def leave_jail(run, how="roll", offset=0):
    """Roll for a double, pay the fine, or spend the pardon card."""
    if how == "card":
        open_options(run, "deeds")
        return close_menu(run)
    if how == "pay":
        return answer(run, CANCEL)
    return roll(run, offset)


def wait_out(run, seat=SEAT, limit=TURN_LIMIT):
    """Hold the confirm through the other seat's turn; it answers alone."""
    start = run.frames()
    run.hold(PORT, [CONFIRM])
    try:
        run.run_until("watch current_player equal %d" % seat, limit)
    finally:
        run.release(PORT, [CONFIRM])
    return run.frames() - start


def affordable(run, seat, group):
    cash = state.player(run, seat)["cash"]
    return cash // state.HOUSE_PRICE[group]
