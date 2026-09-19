"""Step 4a: the year end charts, the key they sort on, and the ledger."""

import json
import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import tash
import engine
import player
import takings

run = tash.run
run.pace(0)

PHASE = os.environ.get("THEMEPARK_CHARTS", "idle")
YEARS = int(os.environ.get("THEMEPARK_YEARS", "3"))
DUMP = os.environ.get("THEMEPARK_DUMP", "")

# Twenty company records of $40 bytes from 68000 $ffffc894, the table the
# pointer $ffff23f2 walks; the player is record 0 and byte $0 reads 1
# there and 2 in a rival.
TABLE_BASE = 0xc894
TABLE_STRIDE = 0x40
PARK_BYTES = 0x100
TABLE_COUNT = 20
PLAYER_ROW = 0

KIND = 0x00
CASH = 0x04
LOAN = 0x08
RANK = 0x38
PLACINGS = (0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f)

# One sorted array per category, twenty entries of six bytes: the score
# in the low 27 bits of the longword, the company in the low five bits
# of byte 4; only the low 16 bits of the score are ever compared.
ARRAYS = ((0x00cc, "richest"), (0x04fc, "exciting"), (0x047e, "pleasant"),
          (0x0034, "biggest"), (0x19be, "amenities"),
          (0x0a0e, "satisfying"))
ARRAY_STRIDE = 6
ARRAY_INDEX = 4
INDEX_MASK = 0x1f
WORD = 0x10000
SCORE_SIGN = 0x04000000
SCORE_SPAN = 0x08000000

# The company-record offsets the six category formulas read, as the year
# end's own code at ROM $12d52-$13376 reads them; README names the park
# field $18336 copies into each one.
REC_0C = 0x0c
REC_0E = 0x0e
REC_10 = 0x10
REC_14 = 0x14
REC_18 = 0x18
REC_1A = 0x1a
REC_1C = 0x1c
REC_1E = 0x1e
REC_1F = 0x1f
REC_20 = 0x20
REC_22 = 0x22
REC_26 = 0x26
REC_28 = 0x28
REC_2C = 0x2c
REC_2E = 0x2e
REC_32 = 0x32
REC_34 = 0x34

PARK_POINTER = 0x019a
PARK_CROWDING = 0x88
CROWD_BASE = 20
CROWD_STEP = 4
SPEND_SHIFT = 6

OPENING_BALANCE = 200000
OPENING_LOAN = 100000
CAPITAL = OPENING_BALANCE - OPENING_LOAN

JANUARY = 0
FEBRUARY = 1
CREEP_CHUNK = 200
CREEP_LIMIT = 300
CREEP_STALL = 12
CHARTS_SETTLE = 300
PRESS_AFTER = 120
LOAN_WALK = 4
LOAN_AFTER = 60
FIRST_PLACE = 1


def byte_at(block, offset):
    """One byte of a work-RAM block, undoing the core's byte swap."""
    return block[offset ^ 1]


def word_at(block, offset):
    return (byte_at(block, offset) << 8) | byte_at(block, offset + 1)


def long_at(block, offset):
    return (word_at(block, offset) << 16) | word_at(block, offset + 2)


def signed(value):
    value &= SCORE_SPAN - 1
    return value - SCORE_SPAN if value & SCORE_SIGN else value


def signed16(value):
    return value - WORD if value & WORD // 2 else value


def tap(button, after=PRESS_AFTER):
    engine.tap(button, engine.TAP_HOLD, after)


def richest(record, park):
    return long_at(record, CASH) - long_at(record, LOAN)


def biggest(record, park):
    return (((byte_at(record, REC_1F) * 2 + byte_at(record, REC_20)) * 4
             + word_at(record, REC_10)) * 2 + word_at(record, REC_0E)
            + byte_at(record, REC_0C))


def exciting(record, park):
    return (byte_at(record, REC_32) * byte_at(record, REC_1F)
            + long_at(record, REC_22) + word_at(record, REC_2C) * 4)


def amenities(record, park):
    return (byte_at(record, REC_20) * 8 + word_at(record, REC_26)
            + byte_at(record, REC_1E) * 8)


def satisfying(record, park):
    return long_at(record, REC_14)


def pleasant(record, park):
    """The crowding term reads the player's own park record, for everyone."""
    rides = byte_at(record, REC_1E)
    crowd = 0
    if rides:
        crowd = CROWD_STEP * (CROWD_BASE - byte_at(park, PARK_CROWDING)
                              // rides)
    return (-signed16(word_at(record, REC_1C)) * 2 - long_at(record, REC_34)
            + word_at(record, REC_28) * 8 + word_at(record, REC_1A) * 64
            + word_at(record, REC_18) + (word_at(record, REC_2E)
                                         >> SPEND_SHIFT) + crowd)


def scores(record, park):
    """Each category as the year end computes it, before it is sorted."""
    return {"richest": richest(record, park),
            "exciting": exciting(record, park),
            "pleasant": pleasant(record, park),
            "biggest": biggest(record, park),
            "amenities": amenities(record, park),
            "satisfying": satisfying(record, park)}


def park_record():
    """The park the pointer $ffff019a names, which is always the player's."""
    pointer = run.memory("system", PARK_POINTER, 4)
    at = ((pointer[1] << 24) | (pointer[0] << 16)
          | (pointer[3] << 8) | pointer[2])
    return run.memory("system", at & (WORD - 1), PARK_BYTES)


def formulas_wrong(sorted_arrays):
    """Every array entry against the formula for the company it names."""
    park = park_record()
    block = run.memory("system", TABLE_BASE, TABLE_STRIDE * TABLE_COUNT)
    records = [block[i * TABLE_STRIDE:(i + 1) * TABLE_STRIDE]
               for i in range(TABLE_COUNT)]
    wrong = []
    for name, places in sorted_arrays.items():
        for place in places:
            want = place["score"]
            got = signed(scores(records[place["company"]], park)[name])
            if got != want:
                wrong.append((name, place["company"], want, got))
    return wrong


def owner(index, record):
    return {"company": index, "kind": byte_at(record, KIND),
            "cash": long_at(record, CASH), "loan": long_at(record, LOAN),
            "rank": word_at(record, RANK),
            "placings": [byte_at(record, at) for at in PLACINGS]}


def table():
    """Every company as the year end left it, the player first."""
    block = run.memory("system", TABLE_BASE, TABLE_STRIDE * TABLE_COUNT)
    return [owner(i, block[i * TABLE_STRIDE:(i + 1) * TABLE_STRIDE])
            for i in range(TABLE_COUNT)]


def chart_key(row):
    """The charts sort on the sum of the six category placings, low wins."""
    return sum(row["placings"])


def category(address):
    """One sorted category: company, its score, and the word sorted on."""
    block = run.memory("system", address, ARRAY_STRIDE * TABLE_COUNT)
    places = []
    for place in range(TABLE_COUNT):
        at = place * ARRAY_STRIDE
        raw = long_at(block, at)
        places.append({"place": place,
                       "company": byte_at(block, at + ARRAY_INDEX)
                       & INDEX_MASK,
                       "score": signed(raw), "word": raw % WORD})
    return places


def categories():
    return {name: category(address) for address, name in ARRAYS}


def ledger():
    """The year end's own four numbers, and the return they make."""
    value = run.watch("sale_value")
    balance = run.watch("cash")
    loan = run.watch("loan")
    net = value + balance - loan
    return {"value": value, "balance": balance, "loan": loan,
            "max_loan": run.watch("max_loan"), "capital": CAPITAL,
            "net": net, "return": (net - CAPITAL) / CAPITAL}


def draw_max_loan():
    """The bank's loan arrow, two rows above the ticket one it can reach."""
    takings.open_the_bank()
    engine.point_to(engine.TICKET_UP)
    for _ in range(LOAN_WALK):
        engine.tap("up", engine.STEP_HOLD, engine.STEP_AFTER)
    tap(engine.PICK, LOAN_AFTER)
    drawn = {"cash": run.watch("cash"), "loan": run.watch("loan")}
    engine.close_bank()
    return drawn


def creep_to(month):
    """A modal event stops the clock, so a year is chunks and an A press."""
    stalled = 0
    was = run.watch("day")
    for _ in range(CREEP_LIMIT):
        if run.watch("month") == month:
            return True
        run.step(CREEP_CHUNK)
        now = run.watch("day")
        stalled = 0 if now != was else stalled + 1
        was = now
        if stalled >= CREEP_STALL:
            tap(engine.PICK)
            stalled = 0
    return run.watch("month") == month


def to_charts(year):
    """A build can end past January, so only leave a January already on."""
    run.mark(f"year {year}", "years")
    if run.watch("month") == JANUARY:
        creep_to(FEBRUARY)
    creep_to(JANUARY)
    run.step(CHARTS_SETTLE)
    tap(engine.PICK)
    return run.frames()


def leave_charts():
    tap("down")
    tap(engine.PICK)
    numbers = ledger()
    run.look()
    tap(engine.PICK)
    return numbers


def year_end(year):
    """One year end: the charts, the table behind them, then the details."""
    frame = to_charts(year)
    rows = table()
    keys = [chart_key(row) for row in rows]
    sorted_arrays = categories()
    wrong = formulas_wrong(sorted_arrays)
    run.look()
    numbers = leave_charts()
    return {"year": year, "frame": frame, "rank": rows[PLAYER_ROW]["rank"],
            "key": keys[PLAYER_ROW], "best_rival": min(keys[1:]),
            "placings": rows[PLAYER_ROW]["placings"], "keys": keys,
            "rows": rows, "numbers": numbers, "wrong": wrong,
            "categories": sorted_arrays}


def report_year(seen):
    print(f"| {seen['year']} | {seen['frame']} | {seen['rank']} |"
          f" {seen['key']} | {seen['best_rival']} |"
          f" {seen['placings']} | {seen['numbers']['value']} |"
          f" {seen['numbers']['balance']} | {seen['numbers']['loan']} |"
          f" {seen['numbers']['return']:.3%} |", flush=True)


def judge_the_years(years):
    best = min(years, key=lambda seen: seen["key"])
    run.judge("the rank watch matches the key order",
              all(seen["rank"] == 1 + sorted(seen["keys"]).index(seen["key"])
                  or seen["keys"].count(seen["key"]) > 1 for seen in years),
              "rank is the position of the player's key among twenty")
    run.judge("the player reached first place",
              best["rank"] == FIRST_PLACE,
              f"best rank {best['rank']} with key {best['key']} at year"
              f" {best['year']}, the best rival on {best['best_rival']}")
    wrong = [row for seen in years for row in seen["wrong"]]
    run.judge("the six category formulas reproduce every score",
              not wrong,
              f"{TABLE_COUNT * len(ARRAYS) * len(years)} array entries,"
              f" {len(wrong)} off: {wrong[:2]}")
    run.judge("the return is the ledger's own numbers", True,
              f"{best['numbers']['return']:.3%} at year {best['year']}")
    return best


def open_the_park():
    built = player.build()
    print(f"built in {built['frames']} frames, cash {built['cash']}",
          flush=True)
    gate = player.open_for_business()
    print(f"park open {gate['open']}, fare {gate['fare']}", flush=True)


def main():
    run.mark("tape", "build")
    played = run.play(engine.TAPE)
    run.expect("the tape reached the park", "watch cash equal 200000",
               f"{played['segments']} segments, {played['frames']} frames")
    if PHASE != "idle":
        open_the_park()
    if PHASE == "loan":
        print(f"loan drawn: {draw_max_loan()}", flush=True)
    years = [year_end(year) for year in range(1, YEARS + 1)]
    for seen in years:
        report_year(seen)
    judge_the_years(years)
    if DUMP:
        pathlib.Path(DUMP).write_text(json.dumps(years))


if __name__ == "__main__":
    main()
