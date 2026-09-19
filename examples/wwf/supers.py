"""Bret Hart's super combos and the reversal, as the ROM gates them."""

import combos
import ring

# ROM $2ac06 and $2acee, gated on the meter at $3ef52 and emptying it.
INITIATORS = {"P": 0x310A, "SK": 0x3D96}
# ROM $2add6: the same shape with no gate; it takes the hold back.
REVERSAL_BUTTON = "SP"
REVERSAL = 0x70DA
GRAB = (("F", 2, 2), ("F+SP", 2, 0))
GRAB_TIMEOUT = 110
FIRE_TIMEOUT = 40
FIRE_TRIES = 3
# BOdom: Bret Hart's sixteen hits are "(grab), FF PK PP P K PK".
PUBLISHED = ("SK", "SP", "P", "K", "SK")
FOLLOWS = (PUBLISHED * 2, PUBLISHED, ("P",) * 5, ())
CHAIN_TAPS = 4
CHAIN_HOLD = 3
CHAIN_GAP = 3


def enter(place, button):
    """Tap forward, forward, button the way the listener reads them."""
    way, first = place.toward()
    if not first:
        place.tap([way])
    place.tap([way])
    place.tap([combos.BUTTONS[button]])


def initiate(place, button):
    """Frames to the combo's animation, or -1 when it does not start."""
    for _ in range(FIRE_TRIES):
        enter(place, button)
        for waited in range(FIRE_TIMEOUT):
            if place.action() == INITIATORS[button]:
                return waited
            place.run.step(1)
            if not place.holding():
                return -1
    return -1


def follow(place, buttons):
    """Tap each button of the branch while the combo script is running."""
    run = place.run
    for name in buttons:
        pressed = [combos.BUTTONS[name]]
        for _ in range(CHAIN_TAPS):
            run.hold(1, pressed)
            run.step(CHAIN_HOLD)
            run.release(1, pressed)
            run.step(CHAIN_GAP)


def reverse(place):
    """Take the hold back from the opponent; answers whether it worked."""
    for _ in range(FIRE_TRIES):
        enter(place, REVERSAL_BUTTON)
        if place.action() == REVERSAL or place.holding():
            return True
        if not place.held():
            return False
    return place.holding()


def grab(place):
    """Press the head grab and step to the frame the hold state shows."""
    for symbol, hold, gap in GRAB:
        pressed = combos.pad(place.run, symbol)
        place.run.hold(1, pressed)
        place.run.step(hold)
        place.run.release(1, pressed)
        place.run.step(gap)
    for waited in range(GRAB_TIMEOUT):
        if place.holding():
            return waited
        place.run.step(1)
    return -1
