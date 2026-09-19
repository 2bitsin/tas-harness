# examples/themepark/

`profile.yaml` runs *Theme Park* (USA, Europe) on Genesis Plus GX from the
cartridge library in `roms/`. The goal this reconnaissance serves is "build and sell
the first park for a profit", so the hunt went after money, the date and the
sale, and the sale is where the surprise is: **the game refuses to sell the
first park until the money it would leave you holding clears 500,000.** One
crude play carried it as far as the refusal; everything below is from that
run or from the session that found it.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `cash_high` / `cash_low` | `0xc67c` / `0xc67e` | 2 | little | AVAILABLE CASH, the bank screen's first line |
| `park_value_high` / `_low` | `0xc680` / `0xc682` | 2 | little | PARK VALUE while the park is running |
| `loan_high` / `_low` | `0xc684` / `0xc686` | 2 | little | CURRENT LOAN |
| `sale_value_high` / `_low` | `0x1d98` / `0x1d9a` | 2 | little | the park value the year end screens and the sale offer are computed from |
| `day` | `0x20da` | 1 | - | day of the month, 1..31 |
| `month` | `0x1197` | 1 | - | month, 0 for January |
| `cursor_x` / `cursor_y` | `0x2204` / `0x220c` | 1 | - | the park cursor's tile, 1..46 by 1..43 |
| `pointer_x` / `pointer_y` | `0x19a0` / `0x19a4` | 2 | little | the menu pointer, in the 640 by 448 space the menu screens use |

Little-endian on a 68000 is Genesis Plus GX keeping work RAM byte-swapped,
the same as Columns. Theme Park's money is longwords well past 65536, so a
low half is not enough and a four-byte watch cannot say it either: the
68000 longword at `A` is `(word at A) << 16 | (word at A + 2)`, and a
`width: 4` watch of either endianness reads the four bytes in one run. Each
figure is therefore two watches and the scenario joins them:

    cash = (cash_high << 16) | cash_low

## The hunt

### Money

The bank screen (icon menu, the coin icon) opens on AVAILABLE CASH 200000,
PARK VALUE 0, CURRENT LOAN 100000, RESEARCH/MONTH 0, TICKET PRICES 50,
MAXIMUM LOAN 150000. Guessing at 200000 is useless -- twenty rival
companies start at the same number, laid out on a 0x40 stride from
`0xc898` -- so the hunt used a move only the player's own record makes:
one click of the loan screen's right arrow, which adds 50,000 to the cash
and 50,000 to the loan at once. Two longwords moved together by 50,000,
and they start at `0xc67c` and `0xc684`.

`0xc680` sits between them and went from 0 to 20000 the frame a Ghost
House was placed, which named it PARK VALUE.

Each figure is one watch, `width: 4` with `endian: swapped`: the harness
reads a 68000 longword out of byte-swapped work ram itself, so nothing
joins two halves in python any more.

Reading the whole of work RAM for a number confirms them. After a year with
one Ghost House, cash 175,655 and park value 20,000:

    175655 -> 0xbb4, 0xc67c, 0xc77a, 0xc898
    20000  -> 0x1dc, 0x1d98, 0x5e80, 0xc6ae
    100000 -> 0xf6, 0xfc, 0xc678, 0xc684, 0xc7de, 0xc7e2, ... (38 hits)

`0xc898` is the company table's copy of the player and `0xc6ae` the park
value's; the 38 hits on the loan are the rivals, all starting at 100,000.

`0x1d98` is a different animal and it matters. The year end screens and the
sale offer do not read `0xc680`; they read `$1e(a0)` with `a0` loaded from
`0xff019a`, which holds `0xffff1d7a` -- so the number the sale is computed
from is the longword at `0xff1d98`. The two swap: while the park runs,
`0xc680` holds the value and `0x1d98` is 0; on the year end screens
`0x1d98` holds it and `0xc680` is 0. A plan that reads park value has to
know which screen it is on.

### The date

The day is one byte. Dumping `0x20c0`+0x40 at four known dates, only one
column follows the panel in the corner of the park screen:

    10 Jan  ...0000000000000a00000000...   0x20da = 0x0a
    30 Jan  ...0000000000001e00000000...   0x20da = 0x1e
     9 Mar  ...0000000000000900000000...   0x20da = 0x09
    31 Dec  ...0000000000001f00000000...   0x20da = 0x1f

The month took a differential across month boundaries rather than a fixed
number of frames, because the clock stops while a month is billed. Stepping
until the day byte wrapped, three times, and asking which of the 65536
bytes read `n`, `n+1`, `n+2` at those three moments gives exactly one
answer:

    month candidates [('0x1197', 1)]

so `0x1197` is the month, 0 for January -- it read 1 at the first 1st of
the month after a 10 January start, that is February. There is no year
anywhere in work RAM as a number: searching for 1995 and for 1996 while the
charts screen showed "OVERALL CHARTS 1996" returns nothing, so the year is
rendered from a base and a counter, not stored.

### The cursor

The park cursor is two bytes, `0x2204` and `0x220c`, in tiles. Holding a
direction to the edges gives the plot: x runs 1..46, y runs 1..43. The
cursor starts at 23,41, in the entrance strip.

### The visitor count

Not found, and the reason is worth recording: a park whose only ride is not
joined to the entrance path takes no money at all (TAKINGS 0 over a whole
year), so there is nothing to count and no counter moves. Sampling work RAM
four times 800 frames apart and keeping every word that stayed in 0..160
and moved at least three times leaves 22 candidates, none of which tracks
anything visible. The ROM does carry the label -- `NO PEEPS IN PK %d` at
`0x180e7`, part of a debug panel -- but finding the counter wants a park
with paths, shops and staff, which this reconnaissance did not build.

## Selling

### The menu path

Selling only happens at a year end, and the year end is a fixed sequence:

1. On 31 December the clock stops and the OVERALL CHARTS screen replaces
   the park, ranking ten park owners.
2. Genesis A on the Ratings icon shows the six-category Ratings Chart;
   Genesis A on the Chart icon comes back.
3. The tick, bottom right, opens YEAR END DETAILS: PARK VALUE, BALANCE,
   LOAN, MAXIMUM LOAN, TAKINGS, EXPENSES and LAND TAX, this year against
   last year. One tap of `down` puts the pointer on the tick, Genesis A
   takes it.
4. If the offer clears 10,000, a line appears along the bottom:
   `CLICK TO SELL PARK FOR <n>`, with the auction box in the bottom left
   corner. Walking the pointer left to about x 40 and pressing Genesis A
   clicks it.
5. Genesis A on the tick of the confirmation sends the park to auction;
   the X refuses. The manual adds that a sale hands back a password and
   that the auction bids the price up from the offer.

The buttons are worth stating: in this core Genesis A is retro `y`,
Genesis B is retro `b` and Genesis C is retro `a`. There is no `c` in the
retropad, so a tape that says `p1.c` is refused.

### What the game will and will not do

The offer is not a negotiation. With V the park value the year end screens
read:

    offer = 1000 * floor(2 * V / 1000)

The line is only drawn when `offer > 10000` (the ROM compares `a4` against
`0x2710` at `0x16e4a` before drawing it, and again at `0x16f70` before
accepting the click), which is why a park worth 2,000 shows no sell line at
its first year end and none at its second either. A Ghost House, at 20,000
the dearest thing a new park can buy, clears it on its own: the screen read

    PARK VALUE 20,000   BALANCE 175,655   LOAN 100,000
    MAXIMUM LOAN 150,000   TAKINGS 0   EXPENSES 24,345   LAND TAX 0
    CLICK TO SELL PARK FOR 40000

Clicking it did not sell the park. It answered:

    EST. BALANCE: 115655
    UNFORTUNATELY THE CHEAPEST AVAILABLE PARK COSTS 500000
    WHICH IS MORE THAN YOU WILL BE ABLE TO AFFORD

and returned to the park. That is the gate, and the ROM says it plainly at
`0x17002`: the game works out

    estimated balance = balance + offer - loan

and, unless every plot on the world map is already owned, refuses the sale
when that is below the price of the cheapest plot you do not own. That
price was 500,000 at the first park. 175,655 + 40,000 - 100,000 = 115,655,
which is what the screen printed, so the formula is the game's own.

Two consequences for a plan:

- Borrowing does not help. A loan adds to the balance and to the loan in
  the same breath, so the estimate does not move.
- Every pound spent on a ride moves the estimate up by one: the balance
  falls by the price and the offer rises by twice it. So the estimate is
  roughly `100,000 + (money ever spent on rides) - overheads`, and getting
  it to 500,000 means putting 400,000 into the park -- four times what the
  park starts with, and only takings can find it.

### Profit, in RAM terms

The game never shows a profit. What it shows is BALANCE at the year end and
PARK VALUE beside it, and what it pays for the park is the offer above,
bid up at auction. So profit for this goal is

    cash after the sale - 200,000   (the EASY starting balance)

with the sale credited as at least `2 * park_value` and the loan repaid out
of it -- exactly the EST. BALANCE line the game itself prints, less the
200,000 it began with. The crude play reached an estimated balance of
115,655 against 200,000 started with: a loss of 84,345, and a refused sale.

## Measured

All of it from the bundle of `scenario.py` or the session beside it.

| what | frames | how it was measured |
|---|---|---|
| power on to the main menu | 900 | the tape's first anchor waits 884 frames from its start |
| the whole boot, power on to a live park | 1,002 | `title-to-park.yaml`, twice |
| one game day, default speed | 39 | ten day-byte changes in 390 frames |
| a month boundary | about 150 extra | a year is 16,000 frames against 365 * 39 = 14,235 |
| a year, ride placed to the charts screen | 16,000 | frame 1,511 to frame 17,511 |
| cursor, direction held | 4.3 up, 4.6 across | 29 tiles in 124 frames, 14 tiles in 64 frames |
| cursor, tapped 2 frames on 1 off | about 45 per tile | 22 tiles in 1,004 frames |
| a placement registering | 26 | Genesis A to PARK VALUE moving |
| locking entrance and exit | 52 | two more Genesis A presses |
| a ride being built | 0 | park value is charged and the ride stands on the placement frame; there is no construction |
| charts screen to year end details | 105 | `down`, then Genesis A |
| details to the sale answer | 112 | pointer to the sell box, Genesis A |
| empty park to the sale screen | 16,926 | frame 1,002 to frame 17,928 |

The cursor is worth a second look: a held direction moves a tile every 4-5
frames, a tapped one every 45. Short presses are damped, so a plan that
taps its way across the plot pays ten times over.

There is no speed setting. `GAME SPEED`, `SLOW`, `NORMAL`, `FAST`, `ULTRA`
and `TURBO MODE` are all in the ROM at `0x28bb6` and `0x28e38`, in the same
string-pointer table whose `LOAD SAVED GAME` and `GAME 00` entries the code
does reach, but nothing in the ROM references the speed entries: they are
the Amiga development menu, left in and unreachable. A year costs its
16,000 frames.

### Determinism

The emulator is deterministic from a checkpoint. The same inputs from the
same checkpoint, twice:

    run1 (17628340335987111824, 179210, 1)
    run2 (17628340335987111824, 179210, 1)

and the boot tape, played twice from power on:

    play 1: segments 5 frames 1002 waited 953 exact 08a4c16689d26a08
    play 2: segments 5 frames 1002 waited 953 exact 08a4c16689d26a08

A one-frame delay is absorbed. Inserting 0, 1, 2, 3 and 20 idle frames
before the same 40-frame held move:

    0  exact 16124635696142028517  cursor 9,3
    1  exact 16124635696142028517  cursor 9,3
    2  exact 11523548396893186132  cursor 9,2
    3  exact 16124635696142028517  cursor 9,3
    20 exact 16124635696142028517  cursor 9,3, one day later

so the cursor steps on an even-frame cadence: one frame of slip costs
nothing, two costs a tile. Whether a frame of slip moves a *visitor* is
untested, because the park this run built has no visitors.

## The shortest path, as far as one play shows

The fewest actions that put a sellable number on the screen are: play the
boot tape (four presses), open the icon menu, walk to the rides icon, open
the quick menu with Genesis B, three rights to the Ghost House, Genesis A
to take it, glide the cursor onto grass, Genesis A to place, Genesis A
twice to lock entrance and exit, wait out the year, `down`, Genesis A,
pointer left, Genesis A. Seventeen inputs and 17,928 frames. It ends in a
refusal.

The starting catalogue is the ceiling on doing it faster. The rides quick
menu holds four entries -- CASTLE 2000, TREE HOUSE 4000, MERRYGOROUND 5000,
GHOST HOUSE 20000 -- and the shops quick menu four more -- COFFEE SHOP
1000, MR. WALLEY 2000, BALLOON LAND 2000, HOOK A DUCK 5000. Only one of
each may stand in the park: a second CASTLE answers "YOU ALREADY HAVE THE
'CASTLE' IN YOUR PARK". So the most a first-year park can be worth without
research is 31,000 of rides, an offer of 62,000, and an estimated balance
nowhere near 500,000.

What stops the run, in the order it will meet them:

1. The sale is refused below an estimated balance of 500,000. This is the
   real boss, and it needs about 400,000 of park built out of takings.
2. Takings need visitors, visitors need the rides joined to the entrance
   path, and this run never laid a path.
3. Overheads are 395 a month whatever the park does; a year with one ride
   costs 4,345 of them, and the ride's price is billed at the next month
   boundary, not when it is placed.
4. Bankruptcy, per the manual, when the park cannot cover its loans plus a
   20,000 leeway, with one year's notice; a park that only ever spends will
   get there.

## The year measured

`engine.py` builds a park from the park tape, holds it for a year and
reads the year end, three times over: the smallest park that ought to
take money, the same with research paid for, and the same with a second
ride. Every number below is from one run of

    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/engine.py --bundle _runs

| shape | frames to build | cash after the build | takings | overheads | park value | offer | visitors | years to 500,000 |
|---|---|---|---|---|---|---|---|---|
| path, CASTLE, COFFEE SHOP, ticket price 2,500, no staff | 6,436 | 200,000, falling to 192,655 as the bills land | 0 | 4,345 | 3,000 | 6,000 | none countable | never |
| the same, RESEARCH/MONTH 2,000 | 6,836 | 200,000, falling to 165,655 | 0 | 4,345 | 3,000 | 6,000 | none countable | never |
| the same plus a TREE HOUSE | 6,889 | 200,000, falling to 188,655 | 0 | 4,345 | 7,000 | 14,000 | none countable | never |

The game's own YEAR END DETAILS screen says the same thing, and the
run shot all three:

    PARK VALUE 3,000  BALANCE 192,655  TAKINGS 0  EXPENSES  7,345
    PARK VALUE 3,000  BALANCE 165,655  TAKINGS 0  EXPENSES 34,345
    PARK VALUE 7,000  BALANCE 188,655  TAKINGS 0  EXPENSES 11,345
    CLICK TO SELL PARK FOR 14000

so the 395 a month is confirmed against the game: 7,345 is 3,000 of
park and eleven months of 395, 11,345 is 7,000 and the same eleven
months, and 34,345 is 3,000, the same 4,345 and 27,000 of research. The
offer on the screen is the formula's: `1000 * floor(2 * 7000 / 1000)`
is the 14,000 the sell line prints, and 6,000 is below the 10,000 the
line needs, so the first two shapes get no sell line at all.

Takings are cash at the year end against cash after the build, with the
overheads and the park's own price added back, because the game bills a
placement at the next month boundary rather than on the frame: cash
after the build is still 200,000 and the year's first two month ends
take the 395 and then the whole park. Every shape lands on exactly
zero. The years-to-500,000 column is empty for the same reason: with
takings at zero the estimated balance falls every year instead of
rising, so the sale is further away after each one, not closer.

### What the build does

The menu paths this run needed, none of which the reconnaissance had:

- **Paths.** The first icon of the icon menu, top left at pointer
  `240,138`. The cursor becomes a trowel and Genesis A lays one tile at
  the cursor; tiles cost nothing and no bill ever names them. The gate's
  own stub reaches tile `23,41`, so eight presses walking up `23,40`
  to `23,34` join the gate to open ground. A held direction while the
  trowel is up draws nothing: paths are laid a press at a time.
- **Shops.** The fourth icon, `384,138`, then Genesis B for the quick
  menu, the entry, Genesis A to take it; the shop then rides the cursor
  and Genesis A drops it. Its footprint is three tiles up and left of
  the cursor and it refuses, silently, any spot where that footprint
  touches a path or another building -- `25,37` is refused, `26,36`
  takes it. A COFFEE SHOP is 1,000 and the park value says so.
- **The fee.** The coin icon at `336,234` opens the bank: AVAILABLE
  CASH, PARK VALUE, CURRENT LOAN, RESEARCH/MONTH and TICKET PRICES,
  with a graph of BALANCE, MONEY IN, MONEY OUT, BUS PEOPLE, GATE CASH,
  SHOP CASH and STAFF COST. TICKET PRICES opens at 50 and its right
  arrow repeats while held, two a step; the ceiling is **2,500**, and
  the price is the word at `0xff1d9c`. The tick at `592,400` closes it.
- **Research.** RESEARCH/MONTH, one line above the ticket price, moves
  250 a click. Eight clicks make 2,000 a month and the bills say so:
  2,000 at every month end and one further 5,000 in the seventh month.
  Over the year that is the 27,000 above. **The cap does move**: the
  rides quick menu opens on four entries and, at the year end after
  that research, holds seven. Prices inflate with it -- the CASTLE that
  cost 2,000 in January is 2,100 the next.
- **Rides.** As the reconnaissance has them, with the correction that
  the entrance and the exit are placed where the cursor stands, not
  where the ride does: the second Genesis A puts the entrance on the
  tile under the cursor and the third the exit. An entrance dropped on
  the last tile of the path joins the ride to the gate.

### Visitors

Not countable, and the hunt says so rather than guessing. A hunt over
work RAM stepped `increased` at each of the twelve month ends of the
first shape leaves six survivors, none of them a visitor count:

    (0x1998, 299) (0x1c98, 20277) (0x1cb0, 6958)
    (0x1cfe, 4800) (0x1d2a, 4800) (0xd028, 11591)

and nothing at all survives when the survivors are held to a plausible
size. The park has no counter that rises with the people in it because
nothing in the park ever counts one: the icon menu's top bar, which the
ROM says is the people in the park, stays empty all year while the line
below it, the people on the next bus, stays full.

What the park does have is people. The bus stops at the gate, two or
three get off, they walk through the turnstiles and up the path, and
the park takes nothing from them: no gate money, no ride money, no shop
money, in any of the three years above or in any of the shapes the
session tried beside them -- ride switched on, ride switched off, ride
told to start with one person aboard, ticket price 0, 50 and 2,500,
shop on the path and shop beside it, four staff hired, railings drawn,
a Ghost House instead of a CASTLE. **No park this step could build
takes a penny**, and that is the finding the plan has to carry.

### Determinism, straight against restored

From the first checkpoint after the build, 300 frames played straight,
then the checkpoint restored and the same 300 played again:

    straight  be416a57202e3264
    restored  be416a57202e3264

No difference, in any character.

## Where the money is

Step 2 measured three parks and every one ended on TAKINGS 0. This
section is the investigation into why. It is a negative result with a
cause: the ROM's condition for admitting a visitor is known exactly,
and no park this example can build satisfies it.

Everything below is from

    TAKINGS_PHASE=<phase> _install/tash/tash run \
      --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/takings.py --bundle _runs

### The first cause: the path was never a path

`engine.glide` holds a direction even when the cursor already sits on
the target, samples the watch every other frame and steps four more
after releasing, so `goto` lands one tile past. `lay_path` therefore
laid its eight tiles at 22,40 23,39 22,39 23,38 22,37 23,36 23,35 23,34
-- a staircase of separate diamonds, not a path, and not joined to the
gate. Every year step 2 measured ran on a park with no path out of the
turnstile. `takings.py`'s `move_to` checks the watch before it presses
anything and releases the frame the watch reaches the target; with it
the eight tiles land on 23,41 .. 23,34 and the park view shows one
continuous band from the gate lane to the ride.

Correcting it changes nothing about the money. It had to be corrected
before anything else could be believed.

### Where a visitor goes

Following one bus load at the gate, thirty frames to the shot
(`TAKINGS_PHASE=gate`, bundle `_runs/2026-09-14T10-00-24Z-themepark`,
shots `shot-0002.png` .. `shot-0221.png`): the bus drives in along the
bottom road, crosses the frame and drives out again. Nobody walks up to
a turnstile, nobody stands in the forecourt more than the one figure
that animates in place, and no sprite ever appears on the path above
the gate. `shot-0045.png` (7 APR) has the bus at the left of the road,
`shot-0077.png` (1 MAY) has it at the right; the park between them is
identical, empty, and paved.

The same read from the earlier run at rate 0 over the bus stop itself
(bundle `_runs/2026-09-14T09-21-11Z-themepark`): peeps do get off,
between `shot-0115.png` and `shot-0143.png`, and mill in the forecourt
at x 174..244, y 90..137; the bus leaves over `shot-0143.png` ..
`shot-0162.png`; one peep animates in place at the stop until the next
bus. None of them reaches the turnstile.

### What the game counts

Four watches were added to `profile.yaml`, all on the park record:

| watch | 68000 address | what |
|---|---|---|
| `gate_chance` | `0xffff1d7b` | the byte the entry decision compares against |
| `peeps_in_park` | `0xffff1d9e` | the count the turnstile routine increments |
| `peeps_ever` | `0xffff1db2` | the same, never decremented |
| `gate_takings` | `0xffff1da2` | the park's own gate cash |

The record's base is the pointer at `0xffff019a`, which reads
`0xffff1d7a`; records are `0x25a` apart, one per park in the world.
Genesis Plus GX keeps 68000 work ram byte swapped, so region offset R
is 68000 address R xor 1, and `takings.py` reads the block whole with
`run.memory` and unswaps it rather than adding a watch per field.

`peeps_in_park`, `peeps_ever` and `gate_takings` read 0 in every shape
tried, over three months and over a whole year. The coffee shop's own
screen agrees: CUSTOMERS SO FAR 0, with STOCK PRICE 10 and SALE PRICE
12 (`_runs/2026-09-14T09-50-19Z-themepark/shots/shot-0001.png`). Cash
falls by exactly 395 a month and by nothing else.

The hunt the brief asked for (`TAKINGS_PHASE=hunt`) keyed `equal` over
300 quiet frames and `increased` over a whole month, four times over:
185, 14, 6, 3 survivors, ending on `0x0bb8`, `0x1196` and `0xfe12`.
None is a peep counter -- they are the clocks that tick once a month.
There is no counter in system ram that a bus load moves, because no bus
load moves one.

### What the game needs

The entry decision is at `0x48cc8`. A per-peep counter at `$3c(a2)`
sends the first pass to `0x48cec`, which calls the rating routine at
`0x1a31e` and compares its answer with the park record's byte at
`$1(a1)`, `0xffff1d7b`:

    048cec  jsr     $1a31e          ; d3 = the rating
    048cf4  movea.l $ffff019a, a1   ; the park record
    048cfc  move.b  $1(a1), d0      ; the gate byte
    048d00  cmp.l   d0, d3
    048d02  bge.w   $48d86          ; rating >= gate: admitted
    048d06  tst.b   $1(a1)
    048d0a  bls.w   $48f0e          ; gate byte 0: admitted

Step 2b read both branches as refusals and both are admissions; "One
visitor followed" below has the corrected block and the measurement
that settles it. `0x1a31e` returns, for a park record at a2:

    worth  = (($10e(a2) >> 1) + $8e(a2)) * $6(a2) >> 7
    keen   = byte at $ffff1d7a + $26(a2)
    rating = keen - (fare - worth) * keen / worth

and short-circuits to 0 when `$8e(a2)` is 3 or less. Measured on this
park (`TAKINGS_PHASE=fields`), `keen` is 20, `$6` is 180, `$10e` is 0
and `$8e` is the sum of the rides' own ratings: 0 with no ride, 30 with
a CASTLE, 35 with a GHOST HOUSE -- not park value, which is 2,000,
3,000 and 21,000 for the same three.

So with one CASTLE, `worth` is 42 and the rating is
`20 - (fare - 42) * 20 / 42`. The fare is the only lever, and the gate
byte moves with it (`TAKINGS_PHASE=fares`, three months each, from one
built park):

| fare | rating | gate byte from February | peeps | gate takings |
|---|---|---|---|---|
| 50 | 17 | 3 | 0 | 0 |
| 60 | 12 | 2 | 0 | 0 |
| 70 | 7 | 2 | 0 | 0 |
| 76 | 4 | 1 | 0 | 0 |
| 80 | 2 | 1 | 0 | 0 |
| 84 | 0 | 0 | 0 | 0 |
| 88 | -1 | 1 | 0 | 0 |
| 92 | -3 | 1 | 0 | 0 |
| 100 | -7 | 0 | 0 | 0 |
| 200 | -55 | 0 | 0 | 0 |
| 500 | -198 | 0 | 0 | 0 |
| 850 | -364 | 0 | 0 | 0 |

The table was read the wrong way up. Fare 88 was picked because it put
the rating below a non-zero gate byte, and that is the one combination
the routine turns away: the rating has to reach the gate byte, not
duck under it. Fare 50, at the top of the table, is the one that works,
and it works the moment the park is open. "One visitor followed" is
the measurement.

The gate byte is 0 through January in every shape, and 0 for the whole
year in a park with no ride at all -- so a ride-less park, whose rating
short-circuits to 0, is refused by the second test instead of the
first.

Every other change the brief listed was tested from the same park and
moved neither GATE CASH nor TAKINGS, by any amount:

- the path joined to the gate's turnstile tile rather than the stub,
  and a nine-by-eight paved blob covering the whole gate area
- the second gate lane paved as well as the first
- the ride's entrance and exit on the path, the ride placed and
  standing
- queue railings from the path to the ride's entrance, three tiles
- the fare at 50, 60, 70, 76, 80, 84, 88, 92, 100, 200, 500 and 850
- a shop the path reaches, one COFFEE SHOP at 26,36
- four staff stood on the path
- a GHOST HOUSE worth 21,000 instead of a CASTLE worth 2,000
- the year's first month passing, and the eleven after it

There is a player-facing open and shut state, and this was the miss:
PARK OPEN and PARK SHUT, string indices 342 and 343, are the quick
menu's own caption line, and START held with Genesis C flips the park
record's `$1c`. Every run in this section was made on a shut park.

### The year, corrected

`TAKINGS_PHASE=year` builds the corrected park, sets the fare to the
88 the rating formula picks out, and holds a year. Both of those are
wrong: the park is shut for the whole year, and 88 is the fare that
turns a visitor away. Read the row as the cost of a shut park.

| shape | frames to build | cash after the build | takings | overheads | park value | offer | visitors | years to 500,000 |
|---|---|---|---|---|---|---|---|---|
| corrected path both lanes, railings, CASTLE, COFFEE SHOP, gate fare 88 | 1,631 | 200,000, falling to 192,655 as the bills land | 0 | 4,345 | 3,000 | 6,000 | none countable | never |

200,000 - 192,655 is 7,345, which is the park's 3,000 and eleven
months of 395 and nothing else. The offer is the usual
`1000 * floor(2 * value / 1000)`, below the 10,000 the sale screen
draws, and with no income the estimate line has no year to name.

The smallest park that takes money was not found, and on this evidence
none of the shapes this example can build is one.

## One visitor followed

Step 2b's park was shut. Theme Park hands a plot over with its gate
closed, `takings.py` never opened it, and a visitor that cannot pass a
shut turnstile never reaches the entry decision at all. Opening it is
two buttons, and with the park open at the fare the game itself sets
the same park fills and takes money. This section is the follow that
found it, from

    VISITOR_PHASE=<phase> _install/tash/tash run \
      --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/visitor.py --bundle _runs

bundle `_runs/2026-09-14T10-47-36Z-themepark`, 14,199 frames.

### The visitor table

Every moving thing in the park is a record in one table:

| | |
|---|---|
| base | 68000 `0xffff5b24`, region offset `0x5b24` |
| stride | `0x86`, 134 bytes |
| slots | 204, of which the dispatcher ticks 1..203 |
| span | 68000 `0xffff5b24` .. `0xffffc5eb` |

ROM `0x4d50` writes the base -- `move.l #$ffff5b24,$ffff5b1c` -- and
`0x58ac4` sets the end from it, `$ffff5b20 = $ffff5b1c + 0x6ac8`, which
is 204 strides exactly. `0x4db80` multiplies a slot number by 134 to
address one, `0x58046`, `0x580c6` and `0x58156` divide by `#$86` to get
the number back, and the dispatcher's own loop at `0x48ad8` walks it:

    048ad8  lea.l   $86(a2), a2
    048adc  cmpa.l  a3, a2
    048ade  bcs.w   $4831a

Slot 0 is a header the loop never ticks.

Dumping the table from the same checkpoint before the bus and once the
load is off it (`VISITOR_PHASE=record`) names the rest of the park:

    before: 1,2,3 kind 14 at 22.50,24.50,26.50 x 42.50   the turnstiles
            4     kind 0e at 19.75,45.50               the bus
            5     kind 06 at 23.50,34.00               the ride's entrance
            6     kind 04 at 20.50,33.00               the ride, placed at 22,33
            7     kind 0a at 26.50,36.50               the COFFEE SHOP
    after:  8,9,10 kind 02 at 47.50,45.50              the bus load

**A bus load in this park is three**, and they appear in the first free
slots, 8, 9 and 10, at the same off-map place the bus starts from.

The fields the follow reads, all in the visitor's record:

| offset | what |
|---|---|
| `$04` | the kind the dispatcher at `0x4831a` switches on |
| `$06`,`$08` | position, 8.8 fixed point, one unit to a tile |
| `$0c` | the state the movement dispatch at `0x48362` switches on |
| `$12` | the way it faces |
| `$14` | a word counted down once a logic tick |
| `$2a` | a word the turnstile takes 250 from when it turns a visitor away |
| `$3c` | a longword counted up once a run of the state body |
| `$4d`,`$4e` | the target tile, x and y |
| `$7a` | the visitor's money, which `0x48e6e` compares with the fare |
| `$7c` | the park it is in, `0xff` for none |

**`$14` is not the state.** The brief inherited that from step 2b and it
is a countdown:

    048cbc  lea.l   $14(a2), a1
    048cc0  subq.w  #$1, (a1)
    048cc2  tst.w   (a1)
    048cc4  bgt.w   $48f42          ; still counting: nothing happens
    048cc8  lea.l   $3c(a2), a1
    048ccc  move.l  (a1), d0
    048cce  addq.l  #$1, (a1)       ; the phase within the state

`move.w #$a,$14(a2)` means "come back in ten ticks", and a tick is
three frames. The state is `$0c`, and `$3c` is the phase the state's
own body counts up.

### The states

Kind `$04` picks the dispatch: 02 a visitor, 08 the brain that gives a
visitor its wants, 04 and 06 a ride's parts, 0a a shop, 0e the bus, 14
a turnstile. A visitor's `$0c` indexes an 83-word table at ROM
`0x48382`, `state - 2` to the entry, default `0x485ba`. The six states
one visitor takes between the bus and the gate, named from what the
forecourt crop shows while each is held:

| `$0c` | what the sprite does |
|---|---|
| `0x1c` | aboard the bus: drawn where the bus is, off the map before it arrives |
| `0x02` | stepping down off the bus into the forecourt |
| `0x08` | walking the forecourt or a path, a tile at a time |
| `0x04` | stood on a turnstile tile: the entry decision runs here |
| `0x40` | walking to the tile in `$4d`,`$4e` |
| `0x1e` | stood on that tile with nothing to walk to |
| `0x22` | the last steps back onto the bus |

### One visitor, every frame

Slot 8, from the frame the bus appears, with the cursor parked on the
gate tile and a crop of the forecourt every **60 frames**
(`run.look(region="40,72,240,120")`). Frame 0 is the restore.

The park as the game hands it over, shut (`VISITOR_PHASE=shut`):

| frame | `$0c` | `$14` | `$3c` | at | target | `$7c` | the crop while it holds |
|---|---|---|---|---|---|---|---|
| 0 | `0x1c` | 0 | 0 | 47.50,45.50 | -- | ff | `shot-0001.png` (f0) .. `shot-0010.png` (f540): empty road, then the bus driving in |
| 575 | `0x02` | 7 | 0 | 24.38,44.50 | -- | ff | 21 frames, no shot lands in it |
| 596 | `0x08` | 8 | 0 | 23.50,44.50 | -- | ff | `shot-0011.png` (f600): the bus stopped at the gate |
| 620 | `0x40` | 8 | 0 | 23.50,43.50 | 24,44 | ff | `shot-0012.png` (f660) |
| 716 | `0x1e` | 8 | 0 | 24.50,44.62 | 24,44 | ff | `shot-0013.png` (f720): one figure stood in the forecourt between the middle and right turnstile |
| 740 | `0x22` | 4 | 0 | 25.25,44.62 | 24,44 | ff | 3 frames, no shot |
| 743 | `0x1c` | 4 | 0 | 24.50,45.50 | 24,44 | ff | `shot-0014.png` (f780) on: the forecourt empty again |

Over 1,500 frames it holds `0x1c` for 1,332 of them and never takes
`0x04`. It gets within one tile of the middle turnstile, turns, walks
to 24,44 -- the forecourt tile below it -- stands there, and reboards.
`peeps_in_park` and `gate_takings` end on 0.

The same park with the gate open (`VISITOR_PHASE=open`):

| frame | `$0c` | `$14` | `$3c` | at | target | `$7c` | the crop while it holds |
|---|---|---|---|---|---|---|---|
| 0 | `0x1c` | 0 | 0 | 47.50,45.50 | -- | ff | `shot-0028.png` (f0) .. `shot-0037.png` (f540): empty road, then the bus driving in |
| 572 | `0x02` | 7 | 0 | 24.38,44.50 | -- | ff | 21 frames, no shot lands in it |
| 593 | `0x08` | 8 | 0 | 23.50,44.50 | -- | ff | `shot-0038.png` (f600): a figure on the road left of the bus |
| 641 | `0x04` | 3 | 0 | 23.50,42.50 | -- | ff | `shot-0039.png` (f660): the turnstile row, the figure on the middle turnstile |
| 692 | `0x08` | 0 | 3 | 23.50,42.50 | -- | **00** | `shot-0040.png` (f720) on: the forecourt empty, the bus pulling away |

`$7c` turning from `0xff` to `0x00` is the visitor joining park 0. From
frame 692 it holds `0x08` for the remaining 856 frames, walking the
park. `peeps_in_park` 3, `peeps_ever` 3, `gate_takings` 150 by the end
of the follow: the whole load of three went in.

### The state that refuses

The shut park's refusal is not at the turnstile, it is one tile short
of it, in state `0x08`'s handler at ROM `0x49562`. The visitor has a
turnstile in front of it and asks whether the park is open:

    04997a  btst.b  #$7, d3         ; the tile ahead is a turnstile
    04997e  beq.w   $49aa8
    049982  cmpi.b  #$ff, $7c(a2)   ; and this visitor is in no park
    049988  bne.w   $4a284
    04998c  movea.l $ffff019a, a1   ; the park record
    049992  tst.b   $1c(a1)         ; PARK OPEN
    049996  bne.b   $499a6           ; open: step onto it
    049998  moveq   #$1, d7
    04999a  clr.w   d0
    04999c  move.b  $12(a2), d0
    0499a0  lsl.w   d0, d7          ; shut: mark the way it faces barred
    0499a2  bra.w   $4a284

**The field it reads is `$1c` of the park record, 68000 `0xffff1d96`.**
It is a byte, 0 shut and 1 open, and ROM `0x5de8c` clears it when a plot
is taken, so a park starts shut. Nothing on the visitor's record is
consulted beyond `$7c`. When the byte is 1 the visitor steps onto the
turnstile tile and `0x49a96` puts it in state `0x04`:

    049a96  moveq   #$0, d0
    049a98  move.l  d0, $3c(a2)
    049a9c  move.b  #$4, $c(a2)

When the byte is 0 the direction is barred and the walk handler picks
another, which ends in state `0x40` to a tile away from the gate, then
`0x1e`, then the bus. That is the whole of step 2b's "mills in the
forecourt".

### The condition met

The park's open byte is a player-facing setting. **Hold START and tap
Genesis C** (retro `a`). START raises the quick menu, and while it is
up ROM `0x53b6a` lets the panel see the C press:

    053b6a  tst.b   $ffffcdba       ; START held this frame
    053b70  bne.b   $53b96
    ...
    053d60  tst.b   $ffffcdb9       ; C pressed since the last frame
    053d66  beq.w   $53e0c
    053d6a  move.b  #$0, $ffffcdb9
    053d72  movea.l $ffff019a, a1
    053d78  tst.b   $1c(a1)
    053d7c  bne.b   $53d82
    053d7e  moveq   #$1, d0
    053d80  bra.b   $53d84
    053d82  moveq   #$0, d0
    053d84  movea.l $ffff019a, a1
    053d8a  move.b  d0, $1c(a1)

`$ffffcdba` is START held and `$ffffcdb9` is C's rising edge, both set
by the pad decoder at `0xb3a2`..`0xb3e4`; C alone does nothing because
`0x53b86` clears `$ffffcdb9` on any frame START is not held. The panel
says so itself: its caption line reads COFFEE SHOP with START held
(`shot-0026.png`) and PARK OPEN one C press later
(`shot-0027.png`). Those are the strings step 2b filed as unreachable.

Six months of the open park at the fare the game hands over, 50
(`VISITOR_PHASE=money`):

| month end | cash | gate byte | `peeps_in_park` | `peeps_ever` | `gate_takings` |
|---|---|---|---|---|---|
| FEB | 199,605 | 0 | 0 | 0 | 0 |
| MAR | 196,810 | 3 | 3 | 3 | 150 |
| APR | 197,015 | 3 | 6 | 6 | 300 |
| MAY | 196,620 | 2 | 5 | 6 | 300 |
| JUN | 196,625 | 2 | 7 | 8 | 400 |
| JUL | 196,630 | 2 | 5 | 10 | 500 |

Ten visitors, 500 of gate cash, and cash rising between the months the
395 overhead lands in. GATE CASH is `$28` of the park record, and the
fare is added to it at ROM `0x48edc`, `add.l d0,$28(a1)`, one fare a
visitor. The bank screen at the end of the six months is
`shot-0053.png`.

### The sense of `0x48d02`

d3 at `0x48d00` is the rating and nothing else: `0x48cec` is
`jsr $1a31e` and `0x48cf2` is `move.l d0,d3`, with no draw between
them. What step 2b had backwards is the branch, not the register.
`0x48d86` is not a refusal -- it sets the countdown and stays in state
`0x04`, and the next run of the body has `$3c` at 1, which is the arm
at `0x48d90` that admits:

    048cec  jsr     $1a31e          ; d3 = the rating
    048cfc  move.b  $1(a1), d0      ; the gate byte
    048d00  cmp.l   d0, d3
    048d02  bge.w   $48d86          ; rating >= gate: wait ten ticks, then in
    048d06  tst.b   $1(a1)
    048d0a  bls.w   $48f0e          ; gate byte 0: in on the next tick
    048d0e  jsr     $5c892          ; a random word
    048d22  move.b  $1(a1), d0
    048d26  jsr     $62708          ; d1 = that word mod the gate byte
    048d2e  cmp.l   d3, d1
    048d30  bge.b   $48d44          ; the draw cleared the rating: turned away
    048d32  move.w  #$a, $14(a2)    ; the draw failed: wait ten ticks, then in
    048d40  bra.w   $48f0e
    048d44  subi.w  #$fa, $2a(a2)
    048d4a  move.b  #$40, $c(a2)    ; walk away from the gate

and the phase-1 arm, which is what "in" means:

    048e42  addq.w  #$1, $24(a1)    ; peeps_in_park
    048e4c  addq.w  #$1, $38(a1)    ; peeps_ever
    048e50  move.w  $ffff0186, d0
    048e56  move.b  d0, $7c(a2)     ; the park it is now in
    048e6e  move.w  $7a(a2), d0
    048e72  cmp.w   $22(a0), d0     ; its money against the fare
    048e78  move.w  #$0, $7a(a2)    ; too poor: emptied, and in anyway
    048edc  add.l   d0, $28(a1)     ; GATE CASH

So the only refusal is `0x48d44`, and it needs `0 <= rating < gate` and
a draw in `[0, gate)` that lands at or above the rating. A rating at or
above the gate byte is admitted outright; a gate byte of 0 admits
everyone. The chance of being turned away is `(gate - rating) / gate`,
so **a higher rating is better and the fare table runs the right way
up after all** -- fare 50, rating 17, gate 3, everybody in; fare 88,
rating -1, gate 1, the draw is 0 and 0 >= -1, nobody in. That is the
second half of why step 2b measured nothing: it opened no gate, and it
had picked the one fare in its table that the gate would have refused.

The turn-away is visible in the record. At fare 88 with the park open
the visitor reaches `0x04`, and one tick later `$2a` goes from 100 to
-150 -- the `subi.w #$fa` -- `$0c` becomes `0x40` and `$4d`,`$4e`
become 24,44, the forecourt again. `$7c` stays `0xff` and no counter
moves.

## The sale

`player.py` is the whole line in one run from power on: boot, lay a
continuous path from the gate, buy every ride and every shop the
unresearched catalogue offers, set the fare to what a visit is worth,
open the park, and ask for the sale at every year end. It asks three
times and is refused three times, because on this plot the sale cannot
be reached. The measurement that says so is below, and the run earns the
verdicts that are left.

    THEMEPARK_PHASE=sale THEMEPARK_YEARS=3 \
    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/player.py --bundle _runs

`THEMEPARK_PHASE` is `build`, `open` or `sale`; `THEMEPARK_YEARS` is how
many year ends to ask at. The bundle's tape replays to the same frame:

    _install/tash/tash tape replay --bundle <bundle> --record _runs \
      --name themepark-sale-held

    replayed 63560 frames of the 63560 the tape keeps,
    hash 6e0bd8bac0db7f7c, watches match

### The run

| year | frame | park value | offer | balance | loan | estimate | sold |
|---|---|---|---|---|---|---|---|
| 1 | 22,651 | 41,000 | 82,000 | 168,207 | 100,000 | **150,207** | no |
| 2 | 37,619 | 38,950 | 77,000 | 170,859 | 100,000 | 147,859 | no |
| 3 | 63,092 | 34,850 | 69,000 | 172,279 | 100,000 | 141,279 | no |

The park is built in 7,166 frames and open at frame 9,220, byte `$1c` of
the park record reading 1. The 101 tiles of path are 971 of those frames,
laid as four runs of 13, 39, 10 and 39 tiles, one hold each -- "Laying a
path by holding" below. Tapping them out a tile at a time built the park
in 10,059 and opened it at 12,113. The fare is set to
154, which is what the ROM at `0x19fa0` says a visit is worth with four
rides standing, and the gate byte is 3 at that fare. The best estimate
the game ever prints is **150,207** at frame 22,651, against the
**100,000** the park is worth on the day it opens (balance 200,000, loan
100,000, value 0). The whole run is 63,560 frames.

The game's own arithmetic, on its own screen, at the first year end:

    YEAR END DETAILS         THIS YEAR   LAST YEAR
    PARK VALUE                  41,000           0
    BALANCE                    168,207     167,370
    LOAN                       100,000           0
    MAXIMUM LOAN               150,000     150,000
    TAKINGS                      3,060           0
    EXPENSES                    45,345           0
    LAND TAX                         0           0
    CLICK TO SELL PARK FOR 82000

    EST. BALANCE: 150207
    UNFORTUNATELY THE CHEAPEST AVAILABLE PARK COSTS 500000
    WHICH IS MORE THAN YOU WILL BE ABLE TO AFFORD

### Why 500,000 cannot be reached here

The gate is `0x17002`: the game refuses unless

    balance + offer - loan >= the cheapest plot you do not own

and the offer is `1000 * floor(2 * value / 1000)`. Writing `X` for every
pound ever turned into park value, and remembering that a loan `L` adds
to the balance and to the loan at once,

    estimate = (200,000 + L - X) + 2X - (100,000 + L) = 100,000 + X

minus whatever running the park has cost. `L` cancels: borrowing is not a
lever, it is only a bigger purse. So the ceiling is the purse. The
balance starts at 200,000 and the loan arrow stops at 150,000 with
100,000 already drawn, which leaves 50,000 of headroom, so

    X <= 200,000 + 50,000 = 250,000    estimate <= 350,000 < 500,000

That holds whatever the catalogue offers. It would only fail if the
balance could grow, and it does not: six years with every unlocked ride
built, the fare at worth and research at the cap took cash from 198,960
down to 150,900. The gate byte decayed from 3 to 1 over the second year
and the park never carried more than twenty people.

The catalogue is the second wall, well inside the first. Of 35 rides and
17 shops in the ROM tables, four rides and four shops are unlocked on day
one -- 41,000 in all, which `player.py` buys entire. Research does unlock
more, and slowly: with the research slider up, the first new shop
appeared in month 17, the first new ride in month 23 and a second shop in
month 35, each one bought with money that is then not park value.

### The levers, with the read behind each

| lever | address | width | ROM |
|---|---|---|---|
| cash (balance) | `0xffffc67c` | 4, swapped | `$a(a1)` added at `0x16dae`, so the company record starts at `0xffffc672` |
| loan | `0xffffc684` | 4, swapped | `$12(a1)` subtracted at `0x16daa`, the same record |
| maximum loan | `0xffff1aaa` | 4, swapped | `0x5d902`: `record[$6a] * record[$66] + 50,000` = 150,000, computed once at park init |
| sale value | `0xffff1d98` | 4, swapped | doubled and rounded down to 1,000 into `a4`, which `0x16da8` moves into the estimate |
| plot price | ROM `0x6fdde`, stride 34, word at `$2` | 2 | `0x16dbc`-`0x16e30` takes the cheapest unowned one and scales it by 100,000 with a shift-add chain |
| park open | `0xffff1d96` | 1 | flipped at `0x53d72`; START held with Genesis C tapped reaches it |
| gate byte (bus load) | `0xffff1d7b` | 1 | written at `0x1a304`; `0x19fa0` rates the fare, `0x1a2ba` quarters it, `0x1a2dc` halves it again above 2, `0x1a2ec` drops it to zero past 150 in the park |
| bus capacity | `$00` of the park record, `0xffff1d7a` | 1 | table at ROM `0x6fabe` (20, 24, 28 ... 150), written at `0xde6e` |
| research earned | `$4d` of the park record | 1 | `0xdbfc` accumulates the monthly research charge into `0xffff19a6`; the level rises when `144 * (level + 1) < cumulative / 85`, i.e. every 12,240 spent |
| ride unlocked | `0xffff0596 + 14 * i`, 35 of them | 1 | `0xdc4c`: `ori.b #3` when the research figure passes `8 *` the word at `$88` of the ride's row in the ROM table at `0x6ac1a`, stride 176 |
| shop unlocked | `0xffff0a9a + 12 * i`, 17 of them | 1 | `0xdcc6` over the ROM table at `0x6c79a`, stride 78 |
| ride price | ROM `0x6ac1a + 176 * i`, word at `$82` | 2 | 753,000 for all 35 |
| shop price | ROM `0x6c79a + 78 * i`, word at `$2a` | 2 | 189,000 for all 17 |
| pending bill | `0xffffc680` | 4, swapped | rides and shops are charged at the next month boundary; the watch is the bill waiting, not the park's worth, and a month end clears it to 0 |

Scenery is not a lever: TREE FENCE at 50, APPLE TREE at 75, ROSE BUSH at
100 and TOILET SHED at 750 can be laid in any number and add nothing to
park value, so cash cannot be poured into the estimate through them.
Duplicates are not a lever either: four attempts to place a second CASTLE
left the ride count at 1 and the value at 2,000.

### What the building needed

- The icon panel's quick menu keeps its highlight between visits and the
  catalogue does not shrink when something is bought, so an index is only
  an index after rewinding: eight taps of `left`, which clamps at zero,
  then `index` taps of `right`.
- Once a shop stands, the panel grows a row listing what you own and the
  pointer opens on it. `engine.point_to` closes the x gap before the y
  gap, and that row has nothing to its right, so it taps `right` 160
  times and moves nothing. Snapping `up` for 24 frames first, the way
  `takings.open_the_bank` does, is what unsticks it.
- The first Genesis A after taking a shop tool is swallowed. Pressing
  until the pending bill at `0xffffc680` rises lands every shop; a single
  press lands only the first one.
- A feature's footprint runs up and left of the tile pressed, and how far
  up differs by item, which shows as a placement the game simply refuses.
  Measured: a shop anchored one row below a path is refused, the small
  shops take three rows of clearance, MR. WALLEY needs more than four,
  and a ride needs more than three. The layout that takes all eight is a
  path row at 30 with the rides anchored at 29 and a second row at 19
  with the shops anchored at 25.

### Laying a path by holding

Genesis A is not a press per tile. The trowel lays whatever tile the
cursor stands on for as long as the button is down, so holding it with a
direction lays a whole straight run in one gesture. `TAKINGS_PHASE=drag`
is the measurement, from the bare park each time.

**What one hold walks.** The tool taken and the cursor settled on the
first tile, Genesis A and one direction go down together and are held for
a fixed number of frames. **tiles** is what the cursor crossed, read from
`cursor_x` and `cursor_y` on every frame of the hold, never counted from
the frames; **ended on** is where the cursor stood once both buttons were
let go, which is not where it was last seen.

| direction | hold | tiles | frames a tile | last seen | ended on |
|---|---|---|---|---|---|
| right | 12 | 2 | 6.0 | (11, 30) | (12, 30) |
| right | 24 | 5 | 4.8 | (14, 30) | (15, 30) |
| right | 40 | 9 | 4.4 | (18, 30) | (19, 30) |
| right | 56 | 13 | 4.3 | (22, 30) | (24, 30) |
| right | 72 | 17 | 4.2 | (26, 30) | (28, 30) |
| right | 88 | 21 | 4.2 | (30, 30) | (32, 30) |
| left | 12 | 2 | 6.0 | (28, 30) | (27, 30) |
| left | 24 | 5 | 4.8 | (25, 30) | (24, 30) |
| left | 40 | 9 | 4.4 | (21, 30) | (20, 30) |
| left | 56 | 13 | 4.3 | (17, 30) | (16, 30) |
| left | 72 | 17 | 4.2 | (13, 30) | (12, 30) |
| left | 88 | 21 | 4.2 | (9, 30) | (8, 30) |
| up | 12 | 2 | 6.0 | (23, 40) | (23, 38) |
| up | 24 | 5 | 4.8 | (23, 37) | (23, 35) |
| up | 40 | 9 | 4.4 | (23, 33) | (23, 31) |
| up | 56 | 14 | 4.0 | (23, 28) | (23, 27) |
| up | 72 | 17 | 4.2 | (23, 25) | (23, 23) |
| up | 88 | 21 | 4.2 | (23, 21) | (23, 19) |
| down | 12 | 2 | 6.0 | (23, 21) | (23, 22) |
| down | 24 | 5 | 4.8 | (23, 24) | (23, 26) |
| down | 40 | 9 | 4.4 | (23, 28) | (23, 30) |
| down | 56 | 13 | 4.3 | (23, 32) | (23, 34) |
| down | 72 | 17 | 4.2 | (23, 36) | (23, 38) |
| down | 88 | 21 | 4.2 | (23, 40) | (23, 42) |

The four directions agree to the frame: the first step costs nine
frames, the game's first-repeat delay, and every step after it four, so
a hold of n frames crosses `(n - 9) / 4 + 2` tiles and the cost per tile
falls towards **four frames**. Both buttons at once, the tool first, or the
direction first all lay the same band at the same speed -- the table of
ways below -- so there is no order to get right.

**Where a drag ends.** The cursor takes one more step three frames after
the direction is let go, and under a held tool that step lays a tile too.
The last column above is that: a drag that runs its direction to the end
of the run lays one tile past it, and a long hold whose tool is let go
first ends two tiles past. So `drag_on` lets the **direction** go one
tile early, keeps the tool down while the cursor takes its last step, and
lets the tool go after it: the band ends exactly on the run's last tile
and the cursor stands on it.

**The band, checked.** The drag's view is compared with the same tiles
pressed one at a time, whole frames, both posed on the run's middle tile
the same number of frames after the restore so the date, the ticker and
the clock cannot be what differs. **missed** and **over** are the trail
against the tiles asked for; **floor** is two identical pressings of the
same band, **drag** is the drag against that pressing, and **a tile** is
the same pressing with the tile under the cursor left out.

| direction | tiles | crossed | ended on | missed | over | floor | drag | a tile |
|---|---|---|---|---|---|---|---|---|
| right | 5 | 5 | (14, 30) | 0 | 0 | 0.00000 | 0.00000 | 0.00172 |
| right | 9 | 9 | (18, 30) | 0 | 0 | 0.00000 | 0.00000 | 0.00172 |
| right | 17 | 17 | (26, 30) | 0 | 0 | 0.00000 | 0.00170 | 0.00172 |
| right | 21 | 21 | (30, 30) | 0 | 0 | 0.00000 | 0.00170 | 0.00172 |
| left | 5 | 5 | (25, 30) | 0 | 0 | 0.00000 | 0.00000 | 0.00172 |
| left | 9 | 9 | (21, 30) | 0 | 0 | 0.00000 | 0.00000 | 0.00172 |
| left | 17 | 17 | (13, 30) | 0 | 0 | 0.00000 | 0.00036 | 0.00172 |
| left | 21 | 21 | (9, 30) | 0 | 0 | 0.00000 | 0.00043 | 0.00172 |
| up | 5 | 5 | (23, 37) | 0 | 0 | 0.00000 | 0.00459 | 0.00671 |
| up | 9 | 8 | (23, 34) | 1 | 0 | 0.00000 | 0.00466 | 0.00269 |
| up | 17 | 17 | (23, 25) | 0 | 0 | 0.00000 | 0.00153 | 0.00142 |
| up | 21 | 20 | (23, 22) | 1 | 0 | 0.00000 | 0.00153 | 0.00601 |
| down | 5 | 5 | (23, 24) | 0 | 0 | 0.00000 | 0.00000 | 0.00269 |
| down | 9 | 9 | (23, 28) | 0 | 0 | 0.00000 | 0.00000 | 0.00449 |
| down | 17 | 17 | (23, 36) | 0 | 0 | 0.00000 | 0.00163 | 0.00142 |
| down | 21 | 21 | (23, 40) | 0 | 0 | 0.00000 | 0.00246 | 0.00142 |

No drag ever laid a tile it was not asked for, and the short bands come
out pixel for pixel identical to the pressed ones. Two rows miss the last
tile, both on the up leg, where the cursor does not take its last step:
`draw_on` presses whatever the drag did not cross, which costs one tap
and a 26-frame wait for each and cannot leave a gap.

The drag column does not fall to zero on the long bands, and the reason
is not path. Decoding the two frames of the seventeen-tile right band by
hand, the 116 pixels that differ are two clusters and nothing else: 91 of
them an 18x14 box on the grass beside the band's far end, which is three
small sprites -- a grey one, an orange one and a blue one -- that the
hold leaves and the presses do not, and 25 of them one 7x7 glyph in the
status panel. Along the band itself the two frames are identical. The
sprites do not fade: padding both runs to 9,000 frames instead of 3,000
gives the same number to the last digit.

**Every way of laying twenty tiles**, from the bare park, the tool taken
and the cursor walked to the first tile included -- that preamble is the
same in every row. The last column is against the pressed band again.

| way | frames | frames a tile | ended on | against pressed |
|---|---|---|---|---|
| pressed again | | | | 0.00000 |
| pressed, (20, 30) left out | | | | 0.00172 |
| nothing laid, the tool and the walk only | 282 | 14.1 | (10, 30) | 0.05795 |
| tap, 60 frames after | 2,001 | 100.0 | (29, 30) | 0.00000 |
| tap, 26 frames after | 1,340 | 67.0 | (29, 30) | 0.00000 |
| tap, 12 frames after | 1,041 | 52.0 | (29, 30) | 0.00000 |
| tap, 6 frames after | 921 | 46.0 | (29, 30) | 0.00127 |
| tap, 2 frames after | 860 | 43.0 | (29, 30) | 0.00127 |
| tap, 0 frames after | 801 | 40.0 | (29, 30) | 0.00127 |
| hold, both at once | 378 | 18.9 | (29, 30) | 0.00170 |
| hold, tool first | 380 | 19.0 | (28, 30) | 0.00170 |
| hold, direction first | 378 | 18.9 | (29, 30) | 0.00170 |
| direction held, tap every 0 | 382 | 19.1 | (30, 30) | 0.00170 |
| direction held, tap every 2 | 386 | 19.3 | (31, 30) | 0.02414 |
| draw_on | 378 | 18.9 | (29, 30) | 0.00170 |

Twenty tiles by hold cost **378 frames against 2,001**, and 282 of both
is the preamble: the laying itself is **96 frames against 1,719**, 4.8 a
tile against 86.0, **seventeen times** quicker. Tapping the tool while
the direction is held is the hold again when the gap is nothing and
starts missing tiles as soon as there is a gap: every two frames leaves
0.02414 of the frame changed where no band at all leaves 0.05795, so
about two fifths of the band never goes down. The
60-frame wait the tap-a-tile build used was never minimised and did not
need to be: **12 frames** lays the same band as 60, six does not, and
`press_run` keeps 26 for the runs too short to drag.

There is nothing in work ram to check a path against. Path costs nothing,
so neither cash nor park value moves, and the search for a per-tile byte
found only a redraw queue that drains: bytes around `0x59xx` go from 0 to
4 as tiles are laid and are back to 1 a second later, and `0x176` decays
to 0 after 600 quiet frames. An isometric fit, `0x3dde + 194 * (x - y) +
2 * (x + y)`, matched three tiles and predicted none. `run.memory` only
answers `system` on this core, so whatever holds the park's tiles is not
reachable at all. The cursor trail and the picture are the check.

### Findings about the harness

- `run.checkpoint` inside a `tash run --scenario` is memory only: nothing
  lands under `_checkpoints/<rom hash>/`, so a second `tash run` cannot
  `restore` what the first one saved. Only the MCP `restore_or_play`
  path persists one. Iterating on a screen 22,000 frames in means
  replaying all 22,000 every time.
- `run.expect` raises and ends the scenario when it fails, so a fact you
  want recorded but not fatal has to go through `run.judge` instead. The
  two read as a pair in the API and do not behave as one.
- There is no anchor for "the screen changed". Waiting for the year end
  is a `step` + `observe` poll on `change`, which is the loop the skill
  page tells you not to write, and `observe` eats the change amount the
  next call would have shown.
- A verdict per tile buries a run, which is why `draw_on` now files one
  for the whole list: the bundle has no way to group or fold verdicts,
  so 101 of them for one path is 101 rows on the report.
- A long run drops audio: the 224,818-frame six-year measurement ended
  with `audio 63 blocks never reached the recording`. The video was
  whole.
- `run.memory` answers `system` and nothing else on this core -- "this
  core has no save memory; it has system" -- so anything the game keeps
  outside work ram cannot be read at all. The park's tile state is one
  such thing, and "Laying a path by holding" has the search for it.
- Two frames the harness has already written cannot be compared by the
  harness: `tash.change_from` only takes the live frame against a PNG,
  so a scenario that wants shot A against shot B reads the two files
  itself. It also refuses a crop -- "a 320x170 frame and a 320x224 one
  are not comparable" -- so the comparison is the whole frame or
  nothing, date and ticker included. That is workable but it puts the
  whole burden on the scenario: `run.look(region=...)` writes a
  byte-identical PNG for a byte-identical view, and exact is what makes
  it treacherous, because two views of the same empty grass are equal
  whatever the park holds and two views taken at different points of the
  clock differ whatever the park holds. What makes it sound is a control
  either way -- the same laying twice for the floor, and the band one
  tile short for the ceiling. "Laying a path by holding" reports both.
- The right instrument for "is that band still there" is a count of the
  path's grey over the park view, and `run.colours` is exactly that, but
  it landed on main after this branch was cut and the installed binary
  predates it. Without it the check is a whole frame against a whole
  frame, which drags in the status panel and every sprite the game
  wanders across the grass, and the numbers have to be read against a
  floor and a one-tile scale rather than against zero.
- `tape replay` of the 63,560-frame bundle runs at 212 fps and takes
  five minutes, against the 1,276 fps and fifty seconds of the run that
  made it. It writes a 1280x896 film where the run wrote none, which is
  most of the difference.

## Running it

    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/scenario.py --bundle _runs

and the measured year:

    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/engine.py --bundle _runs

and one visitor followed from the bus to the gate:

    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/visitor.py --bundle _runs

and the sale asked for at every year end:

    _install/tash/tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/player.py --bundle _runs

## Facts a plan rests on (checked 2026-09-14 on dev)

- Theme Park boots to a live park in **1,002 frames** with four presses:
  Genesis A on the main menu, START on the name screen (leaving EASY
  money, HAPPY visitors, EASY opponents, and the 200,000 that EASY
  gives), Genesis A on the world map, START on the UK plot.
  `tapes/title-to-park.yaml` plays it and plays twice to the same frame,
  `08a4c16689d26a08`.
- Genesis A is retro `y`, Genesis B is retro `b`, Genesis C is retro `a`.
  The retropad has no `c`, so a tape naming one is refused.
- Money is 68000 longwords in byte-swapped work RAM: cash `0xc67c`, park
  value `0xc680`, loan `0xc684`, each read as two little-endian 16-bit
  watches and joined. No single watch can express one of them.
- The sale reads a different park value: the longword at `0xff1d98`, via
  the pointer at `0xff019a`. While the park runs, `0xc680` holds the value
  and `0x1d98` is 0; on the year end screens they swap.
- The date is `day` at `0x20da` and `month` at `0x1197` (0 for January).
  The year is not in work RAM as a number.
- One game day is **39 frames** and a year is **16,000**. There is no
  speed setting: the GAME SPEED strings in the ROM belong to an
  unreachable development menu.
- A held direction moves the park cursor a tile every 4-5 frames; a tapped
  one every 45. A placement registers 26 frames after Genesis A and costs
  two more presses to lock the entrance and the exit. Nothing takes time
  to build.
- The sale offer is `1000 * floor(2 * park_value / 1000)`, drawn only when
  it clears 10,000, and paid only when
  `balance + offer - loan >= 500,000`, the price of the cheapest plot not
  yet owned. At the first park that threshold is 500,000 and the game
  prints the estimate itself: 175,655 + 40,000 - 100,000 = 115,655,
  refused.
- Borrowing cannot reach that threshold: a loan moves balance and loan
  together and leaves the estimate where it was. Money spent on rides
  does move it, one for one.
- Without research a park can hold at most 31,000 of rides and 10,000 of
  shops, one of each kind, so its first year end can offer at most about
  82,000. Selling the first park is a several-year game that needs
  takings, which needs paths and shops this reconnaissance did not build.
- Overheads are 395 a month; a ride's price is billed at the next month
  boundary, not on placement. A year with one Ghost House: TAKINGS 0,
  EXPENSES 24,345, BALANCE 175,655.
- The run is deterministic from a checkpoint, and a one-frame delay
  changes nothing because the cursor steps on an even-frame cadence; two
  frames costs a tile.

## Facts a plan rests on, the year measured (addendum, 2026-09-14)

- A year of a built park costs **16,000 frames** and the whole run --
  three shapes, three years, the determinism check and the tape --
  costs **69,020 frames, 54 seconds** at rate 0. A plan can afford to
  try years; it cannot afford to try them interactively.
- Building the smallest park takes **6,436 frames** of input: the path
  is eight presses, the ride four, the shop three, the fee one held
  button. Two thirds of that is the fee: the ticket price walks from 50
  to its 2,500 ceiling two at a time.
- A placement is **billed at the next month boundary**, not on the
  frame. Cash after a build still reads 200,000; the park's price
  leaves the balance at the month end after that. Any plan that reads
  cash to decide what it can afford must read it after a month
  boundary.
- **Overheads are 395 a month and the game bills eleven of them in a
  calendar year**, 4,345, confirmed against its own EXPENSES line
  three times.
- **Research moves the cap and costs 2,000 a month at eight clicks.**
  Four rides become seven by the year end, and the catalogue's prices
  inflate with the years. It buys nothing this goal can use until the
  park has an income, because the estimate needs money spent on rides
  and research spends it on nothing that stands in the park.
- **Nothing this step could build takes money.** Three measured years,
  and a session's worth of variations beside them, all end on TAKINGS
  0. Until that is solved, every fact about takings in a plan is a
  guess.

The park shape to aim for, on this evidence, is **the two-ride park**:
it is the only one of the three whose year end prints a sell line at
all (14,000 against a 10,000 floor), it costs 453 more frames to build
than the one-ride park, and its estimated balance is the highest of the
three, 102,655. The year count to aim for is **not a count**: with
takings at zero the estimate falls by 4,345 a year, so no number of
repeats of this year reaches 500,000, and a plan that budgets years is
budgeting for a mechanism nobody has seen work yet. The honest number
is one year, to the first sell line, and then the question of takings.

The shortest path, as far as these runs show, is the reconnaissance's
seventeen inputs with a path and a shop added and the fee left alone:
tape, paths icon, eight presses up the gate stub, rides icon, quick
menu, Genesis A, three presses to place ride, entrance and exit, shops
icon, quick menu, Genesis A, one press to drop the shop, then the year
and the four presses of the year end. About 6,000 frames of building
and 16,000 of waiting, and it ends where the reconnaissance ended: at a
refusal, with the park worth 14,000 of an offer against a 500,000 gate.

## Facts a plan rests on, where the money is (addendum, 2026-09-14)

- **A visitor enters only when `0x1a31e`'s rating is strictly below the
  park record's byte at `0xffff1d7b`, and that byte is not 0.** Both
  refusals are at `0x48cc8`. Every park this example builds fails one
  test or the other, so **no plan should assume a built park earns**.
  *Wrong, on both clauses: see the step 2c addendum. The rating has to
  reach the gate byte, a gate byte of 0 admits everyone, and the park
  has to be open before a visitor reaches the turnstile at all.*
- **The rating is `keen - (fare - worth) * keen / worth`**, with
  `worth = (($10e >> 1) + $8e) * $6 >> 7` and `keen` the byte at
  `0xffff1d7a` plus `$26`. Measured here: keen 20, `$6` 180, `$10e` 0.
  A plan that wants visitors must move those fields, not park value.
- **`$8e` of the park record is the sum of the rides' own ratings**, not
  park value: 0 with no ride, 30 for a CASTLE, 35 for a GHOST HOUSE,
  against park values of 0, 2,000 and 21,000.
- **The gate byte is 0 through January**, in every shape, and 0 all year
  in a park with no ride. Nothing can be measured about visitors inside
  the first month.
- **A high fare does not bar a poor visitor.** The turnstile routine at
  `0x48e3c` increments the counts first, and at `0x48e68` a peep who
  cannot afford the fare is admitted with its money zeroed. A fare
  above what peeps carry guarantees GATE CASH 0 with a full park, so
  step 2's 2,500 would have taken nothing even with visitors.
- **`engine.glide` overshoots by one tile**, so `lay_path` laid a broken
  staircase and step 2's three measured years all ran on a park with no
  path. `takings.py`'s `move_to` is the replacement; any future build
  must check the cursor watch before pressing, not after.
- **`engine.open_bank` does not open the bank.** `point_to` fixes x
  before y, and the quick menu's pointer cannot move in x from its
  bottom position (296,282), so the loop spins 160 times and the click
  lands on whatever the panel highlights -- the coffee shop's own
  screen. Snapping to row one with a held `up` first makes
  `point_to(BANK_ICON)` work; `takings.py`'s `open_the_bank` does that.
- **The quick menu's pointer accelerates under taps and snaps under
  holds.** A held direction jumps between row one and the panel's foot
  only; short taps walk it 336,138 -> 165 -> 182 -> 207 -> 226 -> 234,
  which is how the bank's row is reached at all.
- **No counter in system ram moves when a bus arrives.** A hunt of four
  rounds, `equal` over 300 quiet frames against `increased` over a
  month, ends on three addresses and all three are monthly clocks.

## Facts a plan rests on, one visitor followed (addendum, 2026-09-14)

- **A park is handed over shut, and a visitor cannot pass a shut
  turnstile.** The condition is `tst.b $1c(a1)` at ROM `0x49992`, with
  a1 the park record: `$1c`, 68000 `0xffff1d96`, 0 shut and 1 open,
  cleared by `0x5de8c` when the plot is taken. Shut, the way the
  visitor faces is barred at `0x4999a` and it never reaches the entry
  decision at all. **Every measurement step 2 and step 2b made was
  made on a shut park.**
- **START held with Genesis C opens it.** START raises the quick menu
  and sets `$ffffcdba`; C's rising edge sets `$ffffcdb9`; `0x53d8a`
  flips `$1c`. C on its own does nothing, because `0x53b86` clears
  `$ffffcdb9` on any frame START is not held. The panel's caption line
  reads PARK OPEN when it lands.
- **A visitor is turned away only at `0x48d44`, and only when
  `0 <= rating < gate byte` and a draw in `[0, gate)` lands at or above
  the rating.** A rating at or above the gate byte is admitted
  outright; a gate byte of 0 admits everyone. **A higher rating is
  better**, so the fare wants to be low: step 2b's 88 is the one fare
  in its own table that the gate refuses.
- **`$14(a2)` is a countdown, not a state.** `0x48cbc` decrements it
  every logic tick and the state body runs only when it reaches 0.
  The state is `$0c(a2)`; `$3c(a2)` is the phase the body counts up,
  and the turnstile's phase 1 at `0x48d90` is the arm that admits.
- **The entity table is 204 records of `0x86` bytes from
  `0xffff5b24`**, slot 0 a header, and the dispatcher at `0x482a0`
  walks it whole. Turnstiles are kind `0x14`, the bus `0x0e`, a shop
  `0x0a`, a visitor `0x02`. **A bus load in this park is three.**
- **The counters live in the turnstile routine**: `0x48e42`
  `peeps_in_park`, `0x48e4c` `peeps_ever`, `0x48edc` GATE CASH, one
  fare a visitor. Nothing else in the game moves them.
- **The open park earns from the first bus.** Six months of the same
  park at the fare the game sets, 50: ten visitors, 500 of gate cash,
  cash rising between the months the 395 overhead lands in
  (`_runs/2026-09-14T10-47-36Z-themepark`). The step 2 conclusion that
  no buildable park takes money is withdrawn.
- **`run.look(region=)` crops but does not scale.** A visitor is a
  handful of pixels in a 240x120 crop of the gate; the record, not the
  shot, is what says which state it is in. The shots are worth taking
  for the bus and for the panel caption, not for reading a peep.

## Facts a plan rests on, the sale (addendum, 2026-09-14)

- The sale cannot be reached on the first plot. `estimate = balance +
  2 * value - loan` reduces to `100,000 + (money ever turned into park
  value) - losses`, because a loan moves balance and loan together. The
  purse is 200,000 of balance plus 50,000 of headroom under the 150,000
  maximum loan, so the estimate can never pass **350,000** against a
  **500,000** gate. No catalogue, no fare and no number of years changes
  that ceiling.
- The maximum loan is a constant of the plot, not of the park: 150,000,
  computed once at park init by ROM `0x5d902` as `record[$6a] *
  record[$66] + 50,000` and read back at `0xffff1aaa`. It does not move
  when a 20,000 ride is standing.
- The balance does not grow. Six years with every unlocked ride built,
  the fare at worth and research at the cap: cash 198,960 down to
  150,900, the gate byte 3 in year one and 1 from the middle of year two.
- The unresearched catalogue is 41,000: rides 2,000 + 4,000 + 5,000 +
  20,000 and shops 1,000 + 2,000 + 2,000 + 5,000. The whole ROM catalogue
  is 753,000 of rides and 189,000 of shops, gated behind research that
  produced one shop at month 17, one ride at month 23 and one more shop
  at month 35.
- One of each kind is all the game allows: four attempts at a second
  CASTLE left the ride count at 1.
- The best line measured is 150,207 at frame 22,692 -- the first year end
  after buying the catalogue -- against 100,000 at the start. It falls
  from there: park value depreciates about 5% a year and takings do not
  cover overheads. (147,743 before the path was laid by holding; the park
  opens 2,816 frames earlier now, so year one takes more at the gate.)

## The ranking and the ledger (step 4a, 2026-09-14)

`charts.py` is this section as a run: `THEMEPARK_CHARTS` picks `idle`
(the bare park), `built` (step 3's park) or `loan` (step 3's park with
the maximum loan drawn), `THEMEPARK_YEARS` how many year ends to record,
`THEMEPARK_DUMP` a json of every company at every one of them.

    tash run --profile examples/themepark/profile.yaml \
      --scenario examples/themepark/charts.py --bundle _runs

### The ten rows on the charts are twenty rows in RAM

The year end never touches a row directly; every access goes through the
pointer `$ffff23f2`, and `lsl.w #$6,d2` before each one is the stride.

| what | where |
|---|---|
| table base | `$ffff23f2` holds `0xffffc894`, written at ROM `0x4d6e` |
| table end | `$ffff1d24` holds `0xffffcd94` |
| stride | `0x40`, from the `lsl.w #$6` at `0x1319e`, `0x132a0`, `0x13830` |
| records | 20, `cmpi.w #$14,d3` at `0x130c4` |
| the player | record 0; byte `$0` reads 1 for the player, 2 for a rival, tested at `0x13742`, `0x13922`, `0x139d2` |

The fields the charts and the key use:

| offset | width | what |
|---|---|---|
| `$00` | 1 | 1 the player, 2 a rival |
| `$04` | 4 | cash |
| `$08` | 4 | loan |
| `$38` | 2 | the key while the year end sums it, the 1-based rank after |
| `$3a`-`$3f` | 1 | the six category placings, 0-based |

The charts widget draws rows 1 to 9 straight from the sorted key array
(`cmpi.w #$9,d3` / `bcc $139b4` at `0x1389e`). The tenth row is the
player's own, with the player's true rank, when the player is outside
the top nine (`0x139b4`-`0x13a4c`, drawn at y `$184`); when the player
is already inside it, the tenth row is the tenth owner (`0x13ae0`).
That is why the idle line's screen reads `15) IT'S YOU BUDDY` on the
tenth line.

### The `rank` watch, and the key it sorts on

The player's `$38` is 68000 `$ffffc8cc`, which is region offset `0xc8cc`
read as `width: 2, endian: little`; the six placings are the bytes
`$3a`-`$3f`, each read at its address xor 1. Both are watches now:
`rank`, and `key_richest`, `key_exciting`, `key_pleasant`,
`key_biggest`, `key_amenities`, `key_satisfying`, whose **sum is the
key** and whose lowest total takes first place.

Two year ends whose rank differs, each a shot in its bundle:

| line | shot | screen | `rank` | placings | sum |
|---|---|---|---|---|---|
| idle, year 1 | `themepark-charts-idle/shots/shot-0001.png` | `15) IT'S YOU BUDDY` | 15 | 16, 11, 13, 9, 11, 10 | 70 |
| built, year 1 | `themepark-charts-built/shots/shot-0004.png` | `7) IT'S YOU BUDDY` | 7 | 4, 10, 10, 2, 1, 19 | 46 |

### How the key is computed

All of it is inside the year end function `0x129ca`, called once from
`0x58f62`, between ROM `0x12d52` and `0x13376`.

1. `jsr $1795e` at `0x12d1e` refreshes every record first: the player
   through `0x18336`, each rival through `0x17b36`.
2. For each of six categories a score is computed per company and
   written as a longword into a six-byte-stride array of twenty, the
   company index going into the low five bits of byte `$4`
   (`andi.b #$1f`).
3. Each array is sorted by the C `qsort` at `0x62790` with the
   comparator `0x222de`, called at `0x130cc`, `0x130ec`, `0x1310c`,
   `0x1312c`, `0x1314c`, `0x1316c`.
4. The loop `0x13192`-`0x13376` walks the six sorted arrays together.
   `add.w d3,$38(a1,d2.w)` at `0x131b6`, `0x131de`, `0x13206`,
   `0x1322e`, `0x13256`, `0x1327e` adds each company's 0-based place to
   its `$38`; `move.b d3,$3x(a1,d2.w)` records the same place.
5. `0x137fc` builds the key array at `$ffff205c`, same six-byte stride:
   `move.w $38(a1,d2.w),d0` at `0x13838`, `neg.l d0` at `0x1383c`,
   `andi.l #$7ffffff,d0` at `0x1384e`, kept alongside the top five bits
   of whatever the longword held (`andi.l #$f8000000,d2` at `0x13856`).
6. `qsort` again at `0x13878`, then `move.w d0,$38(a1)` at `0x13ace`
   writes the 1-based position back over the key.

| category | score array | placing byte | stored at |
|---|---|---|---|
| RICHEST | `$ffff00cc` | `$3a` | `0x132a6` |
| EXCITING | `$ffff04fc` | `$3b` | `0x132f6` |
| PLEASANT | `$ffff047e` | `$3c` | `0x13346` |
| BIGGEST | `$ffff0034` | `$3d` | `0x132ce` |
| AMENITIES | `$ffff19be` | `$3e` | `0x1331e` |
| SATISFYING | `$ffff0a0e` | `$3f` | `0x1336e` |

### What feeds each score

Read off `0x12d52`-`0x130c2` and checked against every array entry the
game sorted: **360 of 360** at three year ends, the judge
`the six category formulas reproduce every score` in the `built`
bundle. `charts.py` holds them as code.

| category | score, from the company record |
|---|---|
| RICHEST | `long $4 - long $8` |
| BIGGEST | `((byte $1f * 2 + byte $20) * 4 + word $10) * 2 + word $e + byte $c` |
| EXCITING | `byte $32 * byte $1f + long $22 + word $2c * 4` |
| AMENITIES | `byte $20 * 8 + word $26 + byte $1e * 8` |
| SATISFYING | `long $14` |
| PLEASANT | `-word $1c * 2 - long $34 + word $28 * 8 + word $1a * 64 + word $18 + (word $2e >> 6) + crowding` |

`crowding` is `4 * (20 - park[$88] / byte $1e)` when `byte $1e` is not
zero, and the park record it divides by is **always the player's**, the
one `$ffff019a` names: the rivals' crowding term is scored against our
park. Every score is stored masked to 27 bits, so a negative PLEASANT
appears as `0x07ffffxx`.

`0x18336` refreshes the player's record from the park record: `$4` from
cash, `$c` from park `$1`, `$e` from park `$24`, `$10` from park `$38`,
`$14` from park `$48`, `$18` from park `$80`, `$1a` from park `$82`,
`$1c` from park `$84`, `$1e` from park `$87`, `$1f` from park `$86`,
`$20` from park `$e6`, `$26` from park `$c8`, `$28` from park `$d2`,
`$2a` from park `$d6`, `$2c` from park `$da`, `$2e` from the low word of
park `$3c`. **It writes neither `$8` nor `$22` nor `$32`**, which are
only ever written when the company is created (`0x17898`, `0x17922`).

### The competitors are a script

`0x17b36` does not simulate a park. It random-walks each rival's fields
through `0x18296(value, spread, personality)`, seeded from the
personality byte `$1(a2)` that `0x5c892` rolls at creation, and charges
the movement with `sub.l d0,$4(a2)`. The fields it nudges are `$2c`,
`$26`, `$28`, `$2a`, `$1a`, `$18`, `$1e`, `$1f`, `$20`, `$1c`, `$10`,
`$e`, `$c`, `$2e`, `$14`. Nothing else moves them, and their `$8` never
moves at all: twenty companies read 100,000 at all four year ends.

### The comparator only looks at sixteen bits

    0222de  movea.l $4(a7), a1
    0222e2  movea.l $8(a7), a0
    0222e6  move.l  (a0), d0
    0222e8  andi.w  #$ffff, d0
    0222ec  move.l  (a1), d2
    0222f0  andi.w  #$ffff, d2
    0222f2  sub.w   d0, d2
    0222f4  neg.w   d2
    0222f6  move.w  d2, d0
    0222f8  rts

`sub.w` and `neg.w`, so a score above 65,535 wraps and the order is by
`score mod 65,536`, descending. The same comparator sorts all six
categories and the key. For the key that is harmless -- keys run 0 to
114 -- but for RICHEST, where scores are six figures, it decides
everything, and quicksort's partitioning on a non-transitive comparator
leaves the array in runs rather than in order. The whole RICHEST array
at the built line's first year end:

| place | company | score | score mod 65,536 |
|---|---|---|---|
| 0 | 17 | 97,694 | 32,158 |
| 1 | 19 | 86,892 | 21,356 |
| 2 | 10 | 85,619 | 20,083 |
| 3 | 4 | 78,576 | 13,040 |
| 4 | **0** | **68,207** | **2,671** |
| 5 | 14 | 61,925 | 61,925 |
| 6 | 6 | 55,375 | 55,375 |
| 7 | 9 | 47,060 | 47,060 |
| 8 | 13 | 43,328 | 43,328 |
| 9 | 8 | 108,320 | 42,784 |
| 10 | 12 | 103,388 | 37,852 |
| 11 | 3 | 103,238 | 37,702 |
| 12 | 2 | 101,694 | 36,158 |
| 13 | 16 | 101,694 | 36,158 |
| 14 | 5 | 101,694 | 36,158 |
| 15 | 11 | 101,694 | 36,158 |
| 16 | 7 | 101,694 | 36,158 |
| 17 | 18 | 101,594 | 36,058 |
| 18 | 1 | 101,494 | 35,958 |
| 19 | 15 | 101,294 | 35,758 |

Three descending runs, not one order: the array is sorted within
places 0-4, again within 5-8 and again within 9-19, and the company
holding 108,320 -- the most money on the board -- sits in tenth place
behind four companies with a third of it. The player, poorest of the
twenty, takes fifth. A quicksort whose comparator is not transitive
does not produce an order at all, and no plan may assume the remainder
alone decides the place: it has to be measured.

### The loan the company record never hears about

`$8` is written at creation and never again, so the RICHEST score is
`cash - 100,000` however much is actually owed. Drawing the whole
headroom -- one click of the bank's loan arrow, cash 166,420 to 216,420
and loan 100,000 to 150,000 -- moves the score and not the subtrahend:

| line | live loan | record `$8` | RICHEST score | mod 65,536 | placing |
|---|---|---|---|---|---|
| built, year 1 | 100,000 | 100,000 | 68,207 | 2,671 | 4 |
| loan, year 1 | 150,000 | 100,000 | 116,425 | 50,889 | 3 |
| loan, year 3 | 150,000 | 100,000 | 115,745 | 50,209 | 0 |

The score moves by the whole 50,000 and the subtrahend does not; what
the placing then does with it is the comparator's business, and it was
worth one place in year one and four in year three. The arrow is two rows above the ticket one at
`528,346` in the pointer's space, reached by `point_to(TICKET_UP)` and
four taps of `up`; the panel's pointer cannot be walked to it directly.

### The ledger

The year end details screen reads four numbers, all of them watches:

| word on the screen | watch | address |
|---|---|---|
| PARK VALUE | `sale_value` | `0xff1d98` |
| BALANCE | `cash` | `0xffffc67c` |
| LOAN | `loan` | `0xffffc684` |
| MAXIMUM LOAN | `max_loan` | `0xffff1aaa` |

Capital put in is the opening balance less the opening loan: 200,000 -
100,000 = **100,000**, and it never changes, because a loan adds the
same amount to the balance and to the loan. So

    return = (park value + balance - loan - 100,000) / 100,000

Against the game's own two screens:

| shot | PARK VALUE | BALANCE | LOAN | return |
|---|---|---|---|---|
| `themepark-charts-idle/shots/shot-0002.png` | 0 | 195,655 | 100,000 | **-4.345%** |
| `themepark-charts-built/shots/shot-0005.png` | 41,000 | 168,207 | 100,000 | **+9.207%** |

The idle screen's EXPENSES reads 4,345 and the built one's 45,345
against TAKINGS 3,080, which is the same arithmetic from the other side.

Borrowing is not free to the return even though it cancels in the
formula: the loan drawn in March cost 1,782 in interest by the January
after it (216,425 instead of 218,207), so the loan line's first year
returns 7.425% against the built line's 9.207%. It buys chart places,
not money.

### The competitors' curves

Three lines, three year ends each, every company's key read out of its
`$3a`-`$3f`. `idle` is the park as the tape leaves it, with nothing
built and nothing done; `built` is step 3's park, built and opened and
then left alone; `loan` is the same park with the loan arrow clicked
once.

| year end | frame, idle / built / loan | idle rank/key | built rank/key | loan rank/key | best rival, idle / built / loan |
|---|---|---|---|---|---|
| 1 | 16,028 / 21,446 / 21,928 | 15 / 70 | 7 / 46 | 7 / 45 | 28 / 26 / 23 |
| 2 | 31,232 / 36,650 / 37,132 | 15 / 67 | 6 / 47 | 5 / 45 | 25 / 22 / 20 |
| 3 | 46,436 / 51,854 / 52,336 | 17 / 71 | 14 / 66 | 5 / 51 | 26 / 27 / 27 |

The player's six placings behind those keys:

| year end | idle | built | loan |
|---|---|---|---|
| 1 | 16, 11, 13, 9, 11, 10 | 4, 10, 10, 2, 1, 19 | 3, 10, 10, 2, 1, 19 |
| 2 | 13, 10, 7, 14, 13, 10 | 5, 10, 9, 3, 1, 19 | 3, 10, 9, 3, 1, 19 |
| 3 | 15, 11, 10, 14, 10, 11 | 15, 10, 18, 3, 1, 19 | 0, 10, 18, 3, 1, 19 |

Three things the curves say.

- **The rivals do not climb.** The best rival's key sits between 20 and
  28 on every line and every year, and the sum of all twenty keys is
  fixed at `6 * (0 + 1 + ... + 19) = 1140`, so there is no drift to
  wait out. A year spent not improving the park buys nothing.
- **The best rival's key is not a fixed target.** It moves with us:
  every place we take is a place a rival loses, which is why drawing
  the loan pushed the field's best from 26 to 23 in year one. What has
  to be beaten is the field, not a number.
- **The built park does not improve with keeping.** Its key goes 46,
  47, 66 as the gate byte falls from 3 to 1 and park value is eaten by
  depreciation; only the loan line holds its place, and only because
  the loan keeps the RICHEST remainder where it is.

### The fewest years to a first place

**One.** The key carries nothing between years: `0x12d52` recomputes
every score from the record as it stands at that year end, and
`0x1795e` has just refreshed the record from the park. There is no
cumulative term, no rival that grows, and no category that needs a
second year to be scored.

What the first year end already gives, on the loan line: AMENITIES 1,
BIGGEST 2, RICHEST 3. Three of the six categories are near the top in
year one, for 6 of the key's 45. The other 39 sit in three categories
where the player's score is zero or worse:

| category | player score | best rival | player's placing |
|---|---|---|---|
| EXCITING | 0 | 4 (company 9) | 10 |
| PLEASANT | 0 | 193 (company 10) | 10 |
| SATISFYING | -1,707 | 71 (company 9) | 19 |

Those are not big numbers, and they are not money: EXCITING for the
player is exactly `4 * park[$da]`, because `$22` and `$32` are never
refreshed; SATISFYING is `park[$48]`; PLEASANT is a weighted sum of
park `$80`, `$82`, `$84`, `$d2`, `$3c` and the crowding term. Step 3's
park -- four rides, four shops, a hundred tiles of path, eighteen
visitors -- leaves EXCITING and PLEASANT at zero and SATISFYING
negative, so the whole of the climb from 45 to under 23 is in park
fields nothing has yet moved.

The cost of that one year, measured on the built line:

| leg | frames |
|---|---|
| power on to the live park (the tape) | 1,002 |
| the build, paths laid by holding | 7,166 |
| the fare and opening the gate | 1,052 |
| the gate open to the charts | 12,226 |
| **the first charts** | **21,446** |
| the charts to the year end details | ~480 |
| each further year end | 15,204 |

So a first place, if it is reachable at all, is reachable in about
**21,450 frames**, and every year it is not reached costs 15,204 more.

### Findings about the harness, step 4a

- **A year is not a `run_until`.** A GOODS NEGOTIATION modal ("YOU HAVE
  FAILED TO REACH AN AGREEMENT WITH YOUR SUPPLIERS") stops the game
  clock until A is pressed, so `run_until("watch month equal 0", 40000)`
  raises `tape: watch month equal 0 did not hold within 40000 frames`
  on a line that is running perfectly well. `charts.py`'s `creep_to`
  is the replacement: 200-frame chunks, and an A press after twelve
  chunks in which the day watch has not moved.
- **A build can end past January.** Step 3's build costs eight game
  months, so the park opens in month 4 and a `creep_to(February)`
  before `creep_to(January)` sails straight through the first year end
  and costs three extra years. Leave January only when already in it.
- **The python API has no memory write.** Every experiment that needs a
  different game state has to be walked to through the pad; there is no
  way to set the loan, or any other word, and re-measure.
- **`run.shot()` does not exist**, though the MCP `shot` tool does:
  `run.look(path=None)` is the python side of both, and writes into the
  bundle's `shots/` when given no path.
- **`tash session python` gives up after sixty seconds** --
  `error: mcp: http connection: 127.0.0.1:7794 gave no answer before the
  deadline` -- while the server runs the job to completion anyway. The
  answer and everything the job printed are then only reachable through
  `python_output`, and a caller who does not know that has lost them.
  `--detach true` up front is the only safe way to run a year.
- **The bank's loan arrow is unreachable by `point_to`.** The panel's
  pointer will not close an x gap from the rows it opens on, so
  `point_to((528, 346))` walks into the graph legend instead. Opening
  the bank the way `takings.open_the_bank` does, pointing at the ticket
  arrow and tapping `up` four times, is what reaches it.

## First place (step 4b, 2026-09-14)

### What an action costs and what it moves

Every row measured from the `park` checkpoint -- the park as the tape
leaves it -- one action at a time, reading the park record before and
after (`$ffff1d7a`, stride `0x25a`), the balance, the pending bill and
the frames the action took. The frames include taking the tool out of
the quick menu, which is about 280 of them; a second item of the same
kind costs only the press, around 30.

| action | park field | score it moves | credits | frames |
|---|---|---|---|---|
| ten path tiles, held drag | none | none | 0 | 313 |
| ten queue tiles, held drag | none | none | 0 | 329 |
| ride 0 | `$86` +1, `$8e` +30 | BIGGEST, and the worth a fare is judged on | 2,000 billed | 409 |
| ride 1 | `$86` +1, `$8e` +25 | as above | 4,000 billed | 418 |
| ride 2 | `$86` +1, `$8e` +10 | as above | 5,000 billed | 427 |
| ride 3 | `$86` +1, `$8e` +35 | as above | 20,000 billed | 436 |
| shop 0 | `$e6` +1 | BIGGEST, AMENITIES +8 | 1,000 billed | 421 |
| shop 1 | `$e6` +1 | as above | 2,000 billed | 431 |
| shop 2 | `$e6` +1 | as above | 2,000 billed | 439 |
| shop 3 | `$e6` +1, `$10e` +20 | as above, and the worth | 5,000 billed | 448 |
| TREE FENCE | nothing | nothing | 50 cash | 413 |
| APPLE TREE | `$80` +1 | PLEASANT +1 | 75 cash | 422 |
| ROSE BUSH | `$80` +1 | PLEASANT +1 | 100 cash | 431 |
| TOILET SHED | `$87` +1 | AMENITIES +8, and the crowding divisor | 750 cash | 440 |
| hire a handyman | `$c8` +1, `$d6` +1 | AMENITIES +1 | 100 a month | 298 |
| hire staff kind 1 | `$c8` +1, `$da` +1 | EXCITING +4, AMENITIES +1 | 150 a month | 309 |
| hire staff kind 2 | `$c8` +1, `$d2` +1 | PLEASANT +8, AMENITIES +1 | 250 a month | 316 |
| hire staff kind 3 | `$c8` +1, `$e0` +1 | AMENITIES +1 | 450 a month | 325 |
| the fare, 50 to 162 | `$22` +114 | RICHEST, through the takings | 0 | 602 |
| the maximum loan | company `$4` +50,000 | RICHEST +50,000, less interest | the interest | 486 |
| open the gate | `$1c` = 1 | every field a visitor feeds | 0 | 28 |

A ride or a shop is a bill, not a payment: the balance does not move
until the month end, while `sale_value` counts the thing from the frame
it is placed. Scenery and staff come straight out of the balance.

The three categories step 4a found at zero, and what actually moves
them:

- **EXCITING** is `4 * park[$da]` and `$da` is staff kind 1 and nothing
  else. Two of them is 8, which is the whole category: the best rival
  scored 4.
- **PLEASANT** is won on `$d2` (staff kind 2, +8 each) and the crowding
  term `4 * (20 - park[$88] / park[$87])`, which needs one toilet to
  exist at all. `$80` moves by **one** per apple tree or rose bush -- the
  75 and 100 are prices, not weights -- so buying the category through
  scenery costs 75 credits a point against 31 credits a point for a
  guard's monthly wage.
- **`$82`, the `x64` term, cannot be bought.** `$52b70` adds one when a
  landscape tile whose entry in the table at `$6d908` has bit 11 set
  appears, and `$52b10` takes one away when one is built over. Fifteen
  tile types carry that bit, indices 184 to 198, and every one of them
  is scenery the plot came with. Nothing in the catalogue plants one, so
  the term can only be lost. Step 3's rows cross none of them: `$82`
  reads 0 before the build and 0 after it.
- **SATISFYING** is `park[$48]`, the running sum of the happiness of
  visitors as they leave. An open park goes deeply negative (-1,424
  with four rides and a hundred tiles of path) and takes place 19; a
  park that never opens keeps 0 and takes place 10. Nine places, and
  the only way to buy them is to shut the gate, which costs every
  visitor-fed field and the whole of the takings.

### The player, decision by decision

`player.py` phase `first` builds and then hands the year to
`season.Season`, which is a program with one decision in it, taken at
every frame until the charts:

1. **the clock is held** -- the day watch has not moved for 240 frames,
   which is six days' worth, so a modal is up: press A.
2. **the gate is shut** -- park `$1c` is not 1: open it.
3. **the orders are due** -- it is December and the day is past the
   third: run the search below, then hire what it chose.
4. **the fare is under what a visit is worth** -- `worth_now()` has
   risen 10 above the last fare asked for: hold the bank's ticket arrow
   up to `104/100` of it.
5. otherwise run one frame.

Rule 4 is the model's own price: the ROM at `$19fa0` values a visit at
`((long $10e >> 1) + long $8e) * word $6 >> 7`, 154 for the built park,
and `takings.rating_of` turns the gap between fare and worth into the
chance a peep at the gate comes in. A whole year at each of eleven
fares says where to sit; every row is the same park with the same
December hire, two of staff kind 1 and six of kind 2, so the fare is
the only difference between them:

| fare | visitors in the year | gate takings | balance in December | rank | return |
|---|---|---|---|---|---|
| 50 | 20 | 1,000 | 157,905 | 1 | -2.495% |
| 100 | 20 | 2,000 | 161,905 | 1 | +1.905% |
| 150 | 20 | 3,000 | 165,905 | 1 | +6.305% |
| 160 | 20 | 3,200 | 166,185 | 1 | +7.185% |
| 200 | 14 | 2,800 | 165,105 | 1 | +5.905% |
| 220 | 14 | 3,080 | 166,225 | 1 | +6.305% |
| 250 | 13 | 3,250 | 166,905 | 1 | +6.105% |
| 280 | 6 | 1,680 | 160,625 | 1 | -0.175% |
| 300 | 7 | 2,100 | 162,305 | 1 | +1.505% |
| 320 | 0 | 0 | 153,905 | 1 | -6.895% |
| 400 | 0 | 0 | 153,905 | 1 | -6.895% |

Above 300 the gate turns everyone away and the park earns nothing; the
takings peak just above the worth, and `104/100` of 154 is 160. The
arrow overshoots to 162 because it repeats while held, which is the
cheap way to move it: holding reaches 162 in 602 frames, where tapping
the arrow fifty-six times at twelve frames a tap is about a thousand.

### The year-end orders, searched

Rule 3 checkpoints December and tries ten hires -- counts of staff kind
1 and kind 2 -- running each one to the charts, reading `rank` and the
ledger, restoring, and keeping the best by rank first and return
second. The line the run recorded:

| kind 1, kind 2 | rank | key | best rival | placings | balance | return |
|---|---|---|---|---|---|---|
| 0,0 | 10 | 53 | 35 | 13, 10, 11, 0, 0, 19 | 170,207 | +11.207% |
| 1,6 | 1 | 29 | 32 | 6, 3, 1, 0, 0, 19 | 168,557 | +9.557% |
| 1,7 | 1 | 29 | 32 | 6, 3, 1, 0, 0, 19 | 168,307 | +9.307% |
| **2,5** | **1** | **24** | **37** | **0, 0, 3, 2, 0, 19** | **168,657** | **+9.657%** |
| 2,6 | 1 | 24 | 37 | 0, 0, 3, 2, 0, 19 | 168,407 | +9.407% |
| 2,7 | 1 | 24 | 37 | 0, 0, 3, 2, 0, 19 | 168,157 | +9.157% |
| 2,8 | 1 | 24 | 37 | 0, 0, 3, 2, 0, 19 | 167,907 | +8.907% |
| 3,6 | 1 | 29 | 32 | 6, 0, 3, 1, 0, 19 | 168,257 | +9.257% |
| 3,7 | 1 | 29 | 32 | 6, 0, 3, 1, 0, 19 | 168,007 | +9.007% |
| 4,6 | 4 | 41 | 22 | 18, 0, 3, 1, 0, 19 | 168,107 | +9.107% |

Hiring nobody is the best return on the screen and the tenth place on
the charts, which is the whole shape of the problem: the rank is bought
with the last 1,550 credits of the year. The search costs 24,151
frames, every one of them folded off the line by the restore, so the
tape the run records is 21,304 frames long.

The rivals are not a fixed target. Their random walk draws from the
same generator the park does, so every candidate faces a slightly
different field -- the best rival's key came out anywhere between 22
and 37 across the ten. That is why the search is run on the line rather
than a table of orders being written into the player: the same hire
that wins by thirteen against one field loses by nineteen against
another.

### What the search rejected

- **Waiting for December to build.** Bills land at the month end, so a
  park built between Christmas and the year end is counted at
  `sale_value` while the balance still has the money: 31,000 of park
  for nothing, a return of +26.655%. It takes rank 13, because nobody
  ever visited it, and it is an unpaid bill rather than a return. The
  user's direction rules it out twice over -- a player that sits still
  for eleven months is the film step 3 was criticised for.
- **Keeping the gate shut.** SATISFYING is 0 instead of -1,424 and
  PLEASANT rises, worth nine places, but the year earns nothing at all:
  -6.895% against +9.657%, and rank 1 either way.
- **Drawing the maximum loan.** +50,000 on RICHEST for 1,782 of
  interest over a year. It moved our RICHEST placing by two or three
  and never by enough to matter once the fare was right; it costs 486
  frames and about 1.8% of the return.
- **Buying PLEASANT with scenery.** 75 credits a point against 31 for a
  guard, and the guard also carries AMENITIES.
- **Hiring on the 25th of December** rather than the 3rd, to dodge the
  wage bill. The staff menu does not answer that late in the month in a
  way the probe could confirm, and the orders that reached the charts
  were the ones placed early in December.
- **Eleven fares** from 50 to 400, above.

### The line, beside step 3's

| | step 3, built and left alone | step 4b, phase `first` |
|---|---|---|
| first charts at | 21,446 frames | **20,926 frames** |
| frames on the whole line | 21,446 | 21,304 |
| rank | 7 | **1** |
| key | 46 | **24** |
| best rival's key | 26 | 37 |
| placings | - | 0, 0, 3, 2, 0, 19 |
| PARK VALUE | 41,000 | 41,000 |
| BALANCE | 168,207 | 168,657 |
| LOAN | 100,000 | 100,000 |
| return | +9.207% | **+9.657%** |

The run is `_runs/2026-09-14T14-53-35Z-first-place` and the clean
playthrough `tape replay --record` took off it is
`_runs/2026-09-14T14-54-34Z-first-place-replay`: 21,304 frames of the
21,304 the tape keeps, hash `01d63b26a7d0eadc`, watches match. Seven
verdicts, all of them passed -- the tape, the 101 tiles, the gate, the
restore probe, the rank, the return and the frames.

Both lines are read at the charts, where part of the build is still an
unpaid bill: the balance at that moment is what the game's own BALANCE
word says, and the return is the formula step 4a took off the details
screen. The build is 1,887 frames faster than step 3's because the
waits after a placement press came down from 60 frames to 30 and the
quick menu's steps from 12 to 6; the park therefore opens a month
earlier and sells a month more of tickets, at 162 instead of 50.

### Findings about the harness, step 4b

- **`run.frames()` is the session's counter, not the line's.** A
  restore folds frames off the tape but not off `run.frames()`, so a
  scenario that searches cannot report the length of the line it
  produced: `player.py` has to subtract the frames its own search spent
  to get the number the run line then prints as `kept`. A
  `run.kept()` would say it directly.
- **`/workspace/.mcp.json` names one port, 7777.** A second checkout
  serving on its own port has no MCP tools at all and has to drive
  everything through `tash session <tool> --port ...`, which works but
  costs a shell round trip per call and cannot be used inside a
  scenario.
- **`tash session python` still gives up after sixty seconds** while
  the job runs on; `--detach true` and `python_status` are the only
  safe way to run anything longer than a few thousand frames.
- **The python API still has no memory write**, so every one of the
  twenty-one rows in the cost table had to be walked to through the pad
  from a restored checkpoint.
- **`takings.open_the_bank` writes a shot on every call**, because
  `run.look()` with no path is how the python side writes into the
  bundle. A player that reprices during the year fills `shots/` with
  bank panels.
- **The restore probe costs 480 frames per checkpoint**, which a
  per-frame searcher pays once per search and never notices; the run
  line reports `probed 1 checkpoints, 0 parted`.
