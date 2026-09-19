"""The game in python: purses, deeds, decks and a turn, with no core."""

import board
import dice
import engine
import state

SEATS = 2
SQUARES = 40
GO_MONEY = 200
JAIL_SQUARE = 10
JAIL_GATE = 30
JAIL_TURNS = 3
JAIL_FINE = 50
HOTEL = 5
FULL_STREET = 4
BANK = 255
DOUBLES_TO_JAIL = 3
HOUSE_SALE = 2
UNMORTGAGE_TENTH = 10
RAILROADS = (5, 15, 25, 35)
UTILITIES = (12, 28)
CHEST_SQUARES = (2, 17, 33)
CHANCE_SQUARES = (7, 22, 36)
# Income tax is the smaller of the flat bill and a tenth of what the
# seat is worth; luxury tax is flat. One sample cannot separate the tenth
# from a flat 150 -- README, the model's checks.
INCOME_TAX = 4
INCOME_FLAT = 200
INCOME_SHARE = 10
TAXES = {38: 75}
UTILITY_TEN = 10

# The two decks, by the ids the README reads off the jump tables at rom
# 0xcfe0 and 0xd530; the value is what applying the card does.
CARDS = {
    0: ("pardon", 0), 1: ("cash", -100), 2: ("repairs", (40, 115)),
    3: ("cash", 10), 4: ("cash", 25), 5: ("cash", 100), 6: ("cash", 100),
    7: ("cash", 45), 8: ("cash", 100), 9: ("cash", 20), 10: ("each", 50),
    11: ("cash", 200), 12: ("goto", 0), 13: ("jail", 0),
    14: ("cash", -150), 15: ("cash", -50),
    16: ("pardon", 0), 17: ("goto", 5), 18: ("repairs", (25, 100)),
    19: ("goto", 0), 20: ("jail", 0), 21: ("goto", 11), 22: ("cash", 150),
    23: ("cash", 50), 24: ("goto", 24), 25: ("back", 3), 26: ("goto", 39),
    27: ("nearest", "utility"), 28: ("cash", -15), 29: ("each", -50),
    30: ("nearest", "railroad"), 31: ("nearest", "railroad"),
}


class Model:
    """Both seats, every deed and both decks, moved by rolls alone."""

    def __init__(self, table):
        self.table = table
        self.cash = [0] * SEATS
        self.square = [0] * SEATS
        self.jail = [0] * SEATS
        self.doubles = [0] * SEATS
        self.pardon = [False] * SEATS
        self.out = [False] * SEATS
        self.owner = [BANK] * SQUARES
        self.houses = [0] * SQUARES
        self.mortgaged = [False] * SQUARES
        self.decks = {"chance": [], "chest": []}
        self.cursor = {"chance": 0, "chest": 0}
        self.register = 0
        self.counter = 0
        self.choices = []

    def read(self, run):
        """Take every word this model carries out of the machine's ram."""
        for seat in range(SEATS):
            raw = state.record(run, seat)
            self.cash[seat] = int.from_bytes(
                bytes(raw[state.REC_CASH:state.REC_CASH + 4]), "little")
            self.square[seat] = raw[state.REC_SQUARE]
            self.doubles[seat] = raw[state.REC_DOUBLES]
            self.jail[seat] = raw[engine.REC_JAIL]
            self.out[seat] = raw[state.REC_SEAT] == 0
        table = state.deeds(run)
        for square in range(SQUARES):
            self.owner[square] = table[square]["owner"]
            self.houses[square] = table[square]["houses"]
            self.mortgaged[square] = table[square]["mortgaged"]
        for kind in self.decks:
            self.decks[kind], self.cursor[kind] = engine.deck_of(run, kind)
        for seat in range(SEATS):
            self.pardon[seat] = engine.holds_pardon(run, seat)
        self.register = dice.register_of(run)
        self.counter = dice.counter_of(run)
        return self

    def clone(self):
        copy = Model(self.table)
        copy.cash = list(self.cash)
        copy.square = list(self.square)
        copy.jail = list(self.jail)
        copy.doubles = list(self.doubles)
        copy.pardon = list(self.pardon)
        copy.out = list(self.out)
        copy.owner = list(self.owner)
        copy.houses = list(self.houses)
        copy.mortgaged = list(self.mortgaged)
        copy.decks = {kind: list(cards) for kind, cards in self.decks.items()}
        copy.cursor = dict(self.cursor)
        copy.register = self.register
        copy.counter = self.counter
        copy.choices = list(self.choices)
        return copy

    def owned_by(self, seat):
        return [square for square in range(SQUARES)
                if self.owner[square] == seat]

    def group_whole(self, seat, square):
        group = self.table.group(square)
        if group is None or group in ("railroad", "utility"):
            return False
        return all(self.owner[other] == seat
                   for other in self.table.squares_of(group))

    def count_of(self, seat, squares):
        return sum(1 for square in squares if self.owner[square] == seat)

    def rent(self, square, total):
        """What the square's owner is owed by the seat standing on it."""
        holder = self.owner[square]
        if holder == BANK or self.mortgaged[square]:
            return 0
        group = self.table.group(square)
        if group == "railroad":
            owned = self.count_of(holder, RAILROADS)
        elif group == "utility":
            owned = self.count_of(holder, UTILITIES)
        else:
            owned = 1
        return self.table.rent(square, houses=self.houses[square],
                               whole_group=self.group_whole(holder, square),
                               owned=owned, dice=total)

    def liquid(self, seat):
        """Cash plus all the seat can raise before the game folds it."""
        raised = 0
        for square in self.owned_by(seat):
            if not self.mortgaged[square]:
                raised += self.table.mortgage(square)
            raised += (self.houses[square] * self.table.house_price(square)
                       // HOUSE_SALE)
        return self.cash[seat] + raised

    def worth(self, seat):
        """Cash and every printed price the seat holds, as the rules count."""
        return self.cash[seat] + sum(
            self.table.price(square)
            + self.houses[square] * self.table.house_price(square)
            for square in self.owned_by(seat))

    def raise_money(self, seat, need):
        """Sell houses and mortgage deeds until the bill is covered."""
        for square in sorted(self.owned_by(seat), reverse=True):
            while self.houses[square] and self.cash[seat] < need:
                self.houses[square] -= 1
                self.cash[seat] += (self.table.house_price(square)
                                    // HOUSE_SALE)
        for square in sorted(self.owned_by(seat)):
            if self.cash[seat] >= need:
                break
            if not self.mortgaged[square]:
                self.mortgaged[square] = True
                self.cash[seat] += self.table.mortgage(square)
        return self.cash[seat] >= need

    def pay(self, seat, amount, to=None):
        """Move money; fold the seat when nothing it holds covers it."""
        if amount <= 0:
            return True
        if self.cash[seat] < amount and not self.raise_money(seat, amount):
            if to is not None:
                self.cash[to] += self.cash[seat]
            self.cash[seat] = 0
            self.out[seat] = True
            return False
        self.cash[seat] -= amount
        if to is not None:
            self.cash[to] += amount
        return True

    def to_jail(self, seat):
        self.square[seat] = JAIL_SQUARE
        self.jail[seat] = JAIL_TURNS
        self.doubles[seat] = 0

    def advance(self, seat, to, collect=True):
        if collect and to < self.square[seat]:
            self.cash[seat] += GO_MONEY
        self.square[seat] = to

    def draw(self, kind):
        deck = self.decks[kind]
        card = deck[self.cursor[kind] % len(deck)]
        self.cursor[kind] = (self.cursor[kind] + 1) % len(deck)
        return card

    def repairs(self, seat, rates):
        house_rate, hotel_rate = rates
        due = 0
        for square in self.owned_by(seat):
            if self.houses[square] == HOTEL:
                due += hotel_rate
            else:
                due += house_rate * self.houses[square]
        return due

    def nearest(self, seat, kind):
        squares = RAILROADS if kind == "railroad" else UTILITIES
        here = self.square[seat]
        ahead = [square for square in squares if square > here]
        return ahead[0] if ahead else squares[0]

    def card(self, seat, kind, total, events):
        """Turn the deck's next id and do what the card says."""
        card = self.draw(kind)
        what, value = CARDS[card]
        events.append(("card", kind, card, what))
        other = 1 - seat
        if what == "cash":
            if value >= 0:
                self.cash[seat] += value
            else:
                self.pay(seat, -value)
        elif what == "each":
            if value >= 0:
                self.pay(other, value, seat)
            else:
                self.pay(seat, -value, other)
        elif what == "pardon":
            self.pardon[seat] = True
        elif what == "jail":
            self.to_jail(seat)
        elif what == "repairs":
            self.pay(seat, self.repairs(seat, value))
        elif what == "goto":
            self.advance(seat, value)
            self.settle_on(seat, total, events, card=False)
        elif what == "back":
            self.advance(seat, (self.square[seat] - value) % SQUARES,
                         collect=False)
            self.settle_on(seat, total, events)
        elif what == "nearest":
            self.advance(seat, self.nearest(seat, value))
            square = self.square[seat]
            if self.owner[square] not in (BANK, seat):
                due = self.rent(square, total * UTILITY_TEN
                                if value == "utility" else total)
                if value == "railroad":
                    due *= 2
                self.pay(seat, due, self.owner[square])
                events.append(("rent", square, due))
            elif self.owner[square] == BANK:
                self.buy_or_not(seat, square, events)
        return card

    def buy_or_not(self, seat, square, events):
        """Take the deed, or let the auction hand it to the other seat."""
        price = self.table.price(square)
        other = 1 - seat
        want = (self.choices.pop(0) if self.choices
                else self.wants(seat, square))
        if want and self.cash[seat] >= price:
            self.cash[seat] -= price
            self.owner[square] = seat
            events.append(("bought", square, price))
            return True
        self.owner[square] = other
        self.cash[other] -= engine.AUCTION_FLOOR
        events.append(("auctioned", square, engine.AUCTION_FLOOR))
        return False

    def wants(self, seat, square):
        """The rule the machine plays: take what the purse covers."""
        return self.cash[seat] >= self.table.price(square)

    def settle_on(self, seat, total, events, card=True):
        """Answer for the square the token now stands on."""
        square = self.square[seat]
        if square == JAIL_GATE:
            self.to_jail(seat)
            events.append(("jailed", square, 0))
            return
        if square == INCOME_TAX:
            due = min(INCOME_FLAT, self.worth(seat) // INCOME_SHARE)
            self.pay(seat, due)
            events.append(("tax", square, due))
            return
        if square in TAXES:
            self.pay(seat, TAXES[square])
            events.append(("tax", square, TAXES[square]))
            return
        if card and square in CHEST_SQUARES:
            self.card(seat, "chest", total, events)
            return
        if card and square in CHANCE_SQUARES:
            self.card(seat, "chance", total, events)
            return
        if not self.table.buyable(square):
            return
        if self.owner[square] == BANK:
            self.buy_or_not(seat, square, events)
            return
        if self.owner[square] != seat:
            due = self.rent(square, total)
            self.pay(seat, due, self.owner[square])
            events.append(("rent", square, due))

    def move(self, seat, roll, events):
        total = sum(roll)
        self.advance(seat, (self.square[seat] + total) % SQUARES)
        events.append(("moved", self.square[seat], total))
        self.settle_on(seat, total, events)

    def serve(self, seat, roll, events):
        """A turn spent in jail: a double walks out, the third turn pays."""
        self.jail[seat] -= 1
        if roll[0] == roll[1]:
            self.jail[seat] = 0
            self.move(seat, roll, events)
            return True
        if self.jail[seat] == 0:
            self.pay(seat, JAIL_FINE)
            events.append(("fine", JAIL_SQUARE, JAIL_FINE))
            self.move(seat, roll, events)
            return True
        events.append(("jail", JAIL_SQUARE, self.jail[seat]))
        return False

    def turn(self, seat, rolls, buys=()):
        """Play one seat's turn from the rolls the machine drew for it."""
        events = []
        self.choices = list(buys)
        for roll in rolls:
            if self.out[seat] or self.out[1 - seat]:
                break
            if self.jail[seat]:
                self.serve(seat, roll, events)
                continue
            if roll[0] == roll[1]:
                self.doubles[seat] += 1
                if self.doubles[seat] >= DOUBLES_TO_JAIL:
                    self.to_jail(seat)
                    events.append(("jailed", JAIL_SQUARE, 0))
                    break
            else:
                self.doubles[seat] = 0
            self.move(seat, roll, events)
        return events

    def build(self, seat, group, count):
        """Spread houses over a held group, as the game builds evenly."""
        squares = self.table.squares_of(group)
        spent = 0
        for _ in range(count):
            square = min(squares, key=lambda at: (self.houses[at], at))
            if self.houses[square] >= HOTEL:
                break
            price = self.table.house_price(square)
            if self.cash[seat] < price:
                break
            self.cash[seat] -= price
            self.houses[square] += 1
            spent += price
        return spent

    def mortgage(self, seat, square):
        if self.owner[square] != seat or self.mortgaged[square]:
            return 0
        self.mortgaged[square] = True
        self.cash[seat] += self.table.mortgage(square)
        return self.table.mortgage(square)

    def offsets_for(self, total, span=dice.WINDOW):
        return dice.offsets_for(self.register, total, span)

    def roll_at(self, offset):
        return dice.roll_at(self.register, offset)

    def snapshot(self):
        return {"cash": list(self.cash), "square": list(self.square),
                "jail": list(self.jail), "owned": [len(self.owned_by(s))
                                                  for s in range(SEATS)],
                "liquid": [self.liquid(s) for s in range(SEATS)]}
