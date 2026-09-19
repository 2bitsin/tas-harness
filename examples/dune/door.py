"""The password screen as a verb: power on to a mission, on the game's
own words. The codes are read from the cartridge, never spelled here."""

GRID_X = 0xdbf0
GRID_Y = 0xdbf2
GRID_LEFT = 84
GRID_TOP = 52
GRID_CELL = 16
GRID_COLUMNS = 10
RING = 29
END = 28
LIST_ROW = 0xdbfc
MENU_ARROW = 0xd70e
MENU_START_GAME = 160
MENU_OPTIONS = 168
OPTIONS_X = 12
OPTIONS_Y = 36
PASSWORD_ROW = 5
TYPED = 0xffaf
WORD_LENGTH = 10
MISSION = 0xc04c
CONTROL = 0xbf12
LAST_MISSION = 9

A = "y"
START = "start"
TAP_HOLD = 1
TAP_GAP = 1
PRESS_HOLD = 6
ANSWER_FRAMES = 120
SCREEN_FRAMES = 600
PRESS_TRIES = 12
LETTER_TRIES = 4
BRIEFING_CADENCE = 12
BRIEFING_PRESSES = 90
TITLE = "perceptual_hash 19936665cc6c9993 within 8"
TITLE_SEGMENT = "title-to-password"
DOOR = "door"
TITLE_FRAMES = 3000

CARTRIDGE = "cartridge"
PASSWORD_TABLE = 0x20c14
CODE_STRIDE = 11
FIRST_CODE_MISSION = 2
HOUSES = ("harkonnen", "atreides", "ordos")


def code(run, house, mission=LAST_MISSION):
    """Answer that house's password for that mission, read off the rom."""
    index = (mission - FIRST_CODE_MISSION) * len(HOUSES) + HOUSES.index(house)
    at = PASSWORD_TABLE + index * CODE_STRIDE
    return run.string(CARTRIDGE, at, WORD_LENGTH)


def word(run, address):
    return int.from_bytes(run.memory("system", address, 2), "little")


def tap(run, button, hold=TAP_HOLD, gap=TAP_GAP):
    run.hold(1, [button])
    run.step(hold)
    run.release(1)
    run.step(gap)


def waited(run, answers, limit=ANSWER_FRAMES):
    """Answer whether the game said so inside the limit."""
    try:
        run.run_until(answers, limit)
        return True
    except RuntimeError:
        return False


def arrow(run):
    """Answer the title menu's arrow, in screen pixels down the page."""
    return run.memory("system", MENU_ARROW, 1)[0]


def press_until(run, button, answers, tries=PRESS_TRIES,
                limit=ANSWER_FRAMES):
    """Tap until the game says so; answer the taps it took."""
    for press in range(tries):
        if answers():
            return press
        tap(run, button, PRESS_HOLD, TAP_GAP)
        waited(run, answers, limit)
    return tries if answers() else None


def cell(run):
    """Answer which of the 29 cells the grid cursor stands on."""
    column = (run.memory("system", GRID_X, 1)[0] - GRID_LEFT) // GRID_CELL
    row = (run.memory("system", GRID_Y, 1)[0] - GRID_TOP) // GRID_CELL
    return row * GRID_COLUMNS + column


def typed(run):
    return run.string("system", TYPED, WORD_LENGTH)


def walk(run, want):
    """Tap the shortest way round the ring, each tap waited on."""
    for _ in range(RING):
        here = cell(run)
        if here == want:
            return True
        ahead = (want - here) % RING
        button = "right" if ahead <= RING - ahead else "left"
        tap(run, button)
        waited(run, lambda: cell(run) != here)
    return cell(run) == want


def spell(run, password):
    """Type the word, each letter waited on until the buffer holds it."""
    for letter in password:
        for _ in range(LETTER_TRIES):
            walk(run, ord(letter) - ord("A"))
            grown = len(typed(run)) + 1
            tap(run, A)
            if waited(run, lambda: len(typed(run)) >= grown):
                break
    return typed(run)


def on_options(run):
    return (run.memory("system", GRID_X, 1)[0],
            run.memory("system", GRID_Y, 1)[0]) == (OPTIONS_X, OPTIONS_Y)


def to_options(run):
    """START opens the menu, one down takes OPTIONS and A opens it."""
    run.run_until(TITLE, TITLE_FRAMES)
    press_until(run, START, lambda: arrow(run) == MENU_START_GAME,
                limit=SCREEN_FRAMES)
    press_until(run, "down", lambda: arrow(run) == MENU_OPTIONS)
    return press_until(run, A, lambda: on_options(run), limit=SCREEN_FRAMES)


def to_grid(run):
    """Five downs take ENTER PASSWORD and A opens the letter grid."""
    for row in range(1, PASSWORD_ROW + 1):
        press_until(run, "down", lambda: word(run, LIST_ROW) == row)
    return press_until(run, A, lambda: run.memory("system", GRID_X, 1)[0]
                       == GRID_LEFT, limit=SCREEN_FRAMES)


def submit(run, mission=LAST_MISSION):
    """The tenth letter parks the cursor on END; A there takes the word."""
    waited(run, lambda: cell(run) == END)
    return press_until(run, A,
                       lambda: run.memory("system", MISSION, 1)[0] == mission,
                       limit=SCREEN_FRAMES)


def to_control(run, cadence=BRIEFING_CADENCE, presses=BRIEFING_PRESSES):
    """Press A through the briefing until the map cursor answers."""
    return press_until(run, A, lambda: word(run, CONTROL) != 0,
                       tries=presses, limit=cadence)


def enter_password(run, house, mission=LAST_MISSION):
    """Power on to that house's mission with the player in control."""
    password = code(run, house, mission)
    run.reset()
    run.mark(TITLE_SEGMENT, group=DOOR)
    to_options(run)
    to_grid(run)
    grid = run.frames()
    run.mark(f"password-{password.lower()}", group=DOOR)
    spelt = spell(run, password)
    submit(run, mission)
    submitted = run.frames()
    run.mark(f"mission-{mission}", group=DOOR)
    presses = to_control(run)
    return {"house": house, "mission": mission, "typed": spelt,
            "grid": grid, "submitted": submitted, "presses": presses,
            "frames": run.frames()}
