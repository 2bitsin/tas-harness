# examples/caesars/

`profile.yaml` runs *Caesars Palace* (USA) on Genesis Plus GX from the
cartridge library in `roms/`. The goal this reconnaissance serves is "win
999,999,999 $ while trying every game at least once", so what follows is
the wallet in RAM, the games and their limits, and what the game's RNG
does and does not respond to.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `money` | `0xcdd2` | 2 | little | the wallet in whole dollars, low half of the longword at `0xcdd0` |
| `hand_x` | `0xcbc4` | 2 | little | the pointing hand, in the table's own coordinates |
| `hand_y` | `0xcbc8` | 2 | little | the same, y |
| `player_x` | `0xd3bc` | 2 | little | the walking player on the casino floor |
| `player_y` | `0xd3be` | 2 | little | the same, y |
| `rng` | `0x288a` | 2 | little | moves on every draw and on nothing else |

Little-endian on a 68000 is not a typo: Genesis Plus GX keeps work RAM
byte-swapped, so every 16-bit word reads back the other way round.

The wallet is a 68000 longword and both halves are needed above $65,535:

    dollars = little16(0xcdd0) * 65536 + little16(0xcdd2)

Measured: a roulette win took the wallet from $53,900 to $71,900, and the
dump at that moment is

    0xcdd0  01 00 dc 18

`0x0001 * 65536 + 0x18dc` = 71900, and the wallet panel printed
`BALANCE: 71900.00`. It is plain binary, not BCD -- at $1,900 the word
holds `0x076c`, not `0x1900`.

The wallet is what is in the wallet, not what is on the table. Lifting a
$500 chip at a roulette table took `money` from 1900 to 1400 there and
then; at a slot machine the same lift changed nothing and the machine
charged $100 when the coin went in. A staked chip is gone from `money`
already, so a round that loses leaves `money` alone and a round that wins
adds stake plus payout.

Other blocks matter to a player and are not watches:

| what | address | notes |
|---|---|---|
| the three reel stops | `0xd432`, `0xd436`, `0xd43a` | 2 bytes little-endian each; the same three values are what a spin is |
| the roulette pocket | `0xd886` | 2 bytes little-endian; 0 to 36, and 37 for the double zero |
| the draw counter | `0x288a`, `0x288c` | two words; both move on a spin, neither moves while the machine idles |
| the dollars on the table | `0xd916` | 2 bytes little-endian; up $500 a chip at the $500 roulette table, but flat 0 through a whole stack at the high-limit one, so it is no oracle there |

`0x2894`, `0x2896` and `0x2898` also move across a spin but keep moving
while nothing happens, so they are a frame or sound timer and not the
draw.

A reel stop word is one byte written twice: `(0, 0, 514)` is `0x0000`,
`0x0000`, `0x0202`, and the stop is the high byte, 0 to 21 on each reel.
The pocket at `0xd886` was found by diffing work RAM across a spin the
way the wallet was. `0x07c4`, `0x07c8`, `0xcb5e` and `0xcb62` carry the
same number but read 0 for both zeros and hold the table's own limits
until the ball stops, so `0xd886` is the one to read. The banner says
`THE BALL HAS STOPPED ON THE 9` 620 frames after the spin, but the word
is written long before it -- 21 frames after the press, and it does not
change again; see "The measured minimum".

## The hunt

There is no score panel to search against, so the ground truth was the
cashier: the QUICKASHIER machine prints `YOUR BALANCE IS: $2000`, and a
$100 scratch card takes it to $1900. A dump of work RAM before and after
that purchase leaves one 16-bit word that went 2000 -> 1900, at `0xcdd2`,
and its neighbour `0xcdd0` holds the high half that stays 0 until the
wallet passes $65,535.

The pointing hand had to be found the same way, because it cannot be
tracked from the picture: the sprite sways about four pixels on an eight
frame cycle and its hotspot is not its topmost skin pixel. Holding
`right` and then `down` from a still frame and diffing the dumps leaves
`0xcbc4` and `0xcbc8`. That pair is what every "click this" routine here
steers. On a table it moves on a 16 pixel lattice, one step every four
frames, with nothing at all on the other axis -- the sideways drift the
first reading of it reported was a loop reading the hand mid-glide, and
"hand.py" below has the measurements. It clamps at the edges of the
panel it is on -- at the cashier, x within 24..204 and y within 56..224.

The walking player is `0xd3bc`/`0xd3be`, found by holding a direction on
the floor and diffing. The floor is about 1,280 by 900 pixels, five
screens by four.

## The games

Every one of these was walked to and opened from the floor by pressing
`up` to face the machine and then B; every screenshot named is in the
bundle this directory's `scenario.py` writes. The minimum and maximum are
the game's own welcome banner. "Coins" in a payout is the machine's own
denomination, which is on the sign above it.

| game | where the player stands | minimum | maximum | biggest one round pays |
|---|---|---|---|---|
| QUICKASHIER cashier | (783, 822) and (640, 822) | -- | -- | scratch card $100 each: 60-YARD FIGHT, MATCH TWO, TRIPLE JACKS |
| video poker | (863, 802), (295, 338), (527, 638) | 1 coin | 5 coins | royal flush 5,000 coins |
| slots "JACKPOT JUNGLE" | (1055, 802), (295, 442) | 1 coin | 3 coins | third pay line 5,000 coins |
| slots "TRIPLE JACKPOT" | (552, 802) | 1 coin | 3 coins | third pay line 5,000 coins |
| roulette | (344, 838) | $1 | $500 | straight up pays 35:1, so $18,000 back on $500 |
| roulette, high limit | (624, 115) | $5 | $50,000 | 35:1 again, so $1,800,000 back on a hundred $500 chips |
| craps | (192, 834) | $10 | $1,000 | not measured |
| blackjack | (328, 656), (416, 614) | $100 | $5,000 | 3:2, insurance pays 2:1 |
| blackjack, low limit | (247, 534) | $25 | $1,000 | 3:2 |
| keno | (439, 390) | 1 coin | 20 coins | the sign says $300,000 |
| horse racing terminal | (752, 546), (880, 546) | -- | -- | not measured; betting closes at post time |

The video poker pay table, read off the machine, in coins for 1 to 5
coins played:

| hand | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| pair of jacks | 1 | 2 | 3 | 4 | 5 |
| two pair | 2 | 4 | 6 | 8 | 10 |
| three of a kind | 3 | 6 | 9 | 12 | 15 |
| straight | 4 | 8 | 12 | 16 | 20 |
| flush | 6 | 12 | 18 | 24 | 30 |
| full house | 9 | 18 | 27 | 36 | 45 |
| four of a kind | 25 | 50 | 75 | 100 | 125 |
| straight flush | 50 | 100 | 150 | 200 | 250 |
| royal flush | 250 | 500 | 750 | 1,000 | 5,000 |

The $100 slot machine's pay table, also in coins: third pay line 5,000,
second 2,000, first 1,000; three sevens 200, three bars 80, three
cherries 10, any two 5, any one 2. A coin is worth $100 in a payout as
well as in the slot: over 231 pulls the wins were $200, $400, $500,
$700, $1,000, $1,500 and $4,500, which are 2, 4, 5, 7, 10, 15 and 45
coins, and 45 is 40 + 5 on two pay lines. Nothing in this table adds up
to 4,500 coins over three lines, so the earlier reading of a 200-coin
win paying $200 was a 2-coin win.

Playing a slot by hand is: C opens the wallet, a click on a rack lifts a
chip, C closes it, seven bursts of `down` put the hand on the coin slot
at hand (184, 304), B feeds one coin of the machine's own denomination,
six bursts of `right` reach the lever at hand (228, 224), B pulls. Three
coins is three pay lines and the most a machine takes.

Playing a table is: C opens the wallet, a click on a rack lifts a chip, C
closes it, the hand goes to a square and B drops the chip there, then
Genesis A spins. Genesis A does nothing while the previous round is still
settling, so a spin routine has to watch the screen rather than count
frames. The hand's coordinates on a roulette table are the table's own,
not the screen's -- the camera scrolls to follow it -- so a square has one
address whatever the view: number `n` is at

    x = 88 + 31.2 * (ceil(n / 3) - 1)
    y = 124 for n mod 3 == 1, 90 for n mod 3 == 2, 56 for n mod 3 == 0

which was calibrated by putting a chip on 31 at (400, 124) and winning,
and on 13 at (205, 124) and winning.

The high-limit tables are gated on the wallet, not on a door: the
blackjack table at (1024, 558) answers `SORRY, THIS TABLE IS ONLY OPEN TO
HIGH ROLLERS` to a $2,000 wallet. The high-limit roulette at (624, 115)
opened at $1,900, but its wallet offers the same five racks -- $1, $5,
$25, $100, $500 -- so a $5,000 bet has to be ten chips stacked on one
square. A stack pays per chip -- see "The stack" below -- and the
banner's own limits are not the ones written in the games table above:
they are $5 and $50,000.

Keno, the horse racing terminal and craps were opened and read but not
played to a result. Red dog, big six and baccarat were looked for and not
found: a depth-first walk of the floor that clicked every other cell over
488 cells turned up the eleven kinds above and nothing else.

## Determinism

All of this is from a checkpoint restored in the same run, so the machine
state is bit-identical each time.

On a slot machine (`JACKPOT JUNGLE`, $100 a coin, wallet $1,900):

- The same pull twice gives the same frame: exact hash
  `15139389368529174386` both times, wallet $1,800 both times.
- Waiting 0, 1, 2, 3, 5, 8, 13, 21, 40, 100, 200, 600 or 1,000 frames
  before pulling the lever gives that same hash and that same wallet.
  **A different frame of pressing the button does not give a different
  outcome.** The RNG is draw-driven, not frame-driven, which is the
  classic TAS lever and it is not there.
- The stake does not change the outcome either. One, two, three and then
  one coin from the same checkpoint all stop the reels at
  `(4112, 4883, 257)`; only the payout differs, because the coin count is
  the number of pay lines: one coin -$100, two coins -$200 +$200, three
  coins -$300 +$200.
- Five spins in a row do differ, so the draw advances per spin:
  `15139389368529174386`, `3134166432297032134`,
  `14956941221963093648`, `9823603046242253955`, `7734488897393109789`,
  with the wallet going 1600 -> 1800, 1500 -> 1500, 1200 -> 1900,
  1600 -> 1800, 1500 -> 2500.

On a table game (roulette, $1 minimum, $500 maximum):

- From one checkpoint the ball stopped on 31 with the chip on a losing
  square, and on 31 again with the chip on 31. From the next checkpoint
  it stopped on 13 twice, and from the one after that on 3 twice. **The
  number is drawn before the bet is read.**
- That is the exploit this game does have: spin, read
  `THE BALL HAS STOPPED ON THE n` off the banner, restore, put the chip
  on n, spin again. Five rounds of it took the wallet from $1,900 to
  $71,900, every round paying the full $18,000.

## The stack

The high-limit table at (624, 115) says what it takes itself:

    WELCOME TO ROULETTE!
    THE TABLE LIMITS ARE
    MINIMUM $5
    MAXIMUM $50000

$50,000 is a hundred of the $500 chips the wallet offers, and a hundred
is what the table pays for.

A stack pays per chip. From one checkpoint at that table, wallet
$19,500, the pocket scouted as 25 and the chips dropped straight up on
25 at hand (336, 120):

| chips | staked | wallet after | paid | frames to place |
|---|---|---|---|---|
| 1 | $19,000 | $37,000 | $18,000 | 322 |
| 2 | $18,500 | $54,500 | $36,000 | 539 |
| 5 | $17,000 | $107,000 | $90,000 | 1,190 |
| 10 | $14,500 | $194,500 | $180,000 | 2,275 |

and from a $194,500 checkpoint with the pocket scouted as 5, at hand
(112, 88):

| chips | staked | wallet after | paid | frames to place |
|---|---|---|---|---|
| 100 | $144,500 | $1,944,500 | $1,800,000 | 23,254 |
| 101 | $144,000 | $1,944,000 | $1,800,000 | 23,486 |
| 105 | $142,500 | $1,942,500 | $1,800,000 | 24,978 |

So $18,000 a chip up to a hundred chips, and over the limit the chips
are taken and pay nothing: the 101st and the 102nd to 105th left the
wallet and came back as nothing. There is no refusal and no banner; the
wallet is the only sign.

Four things make a round work that a picture does not show.

The wheel spins with no bet on the table, so a scouting spin costs
nothing at all -- not a chip, not a minimum.

The roulette draw is frame-driven, which the slots are not. From one
checkpoint, a spin pressed 0 to 875 idle frames later drops the ball in
33, one pressed 900 to 3,000 frames later in 0, and one pressed 4,000 to
6,000 frames later in 33 again. So a scouted round has to press its
betting spin the same number of frames after the checkpoint as its
scouting spin was pressed, or it has bet on the wrong number. Some
offsets are dead: a spin pressed 3,600 frames after one checkpoint did
not start the wheel at all and `0x288a` did not move, which is why
`engine.py` checks that counter and tries a later offset.

The table pays about a thousand frames after it names the pocket. The
banner and `0xd886` are both up 620 frames after the spin, but the
wallet did not move until somewhere between 820 and 1,220 frames after
it: 19,000 at +820 and 37,000 at +1,020. A round that reads the wallet
when the banner appears records every win as a loss.

A square is a lattice point, not a rectangle. A settled hand only ever
stands on multiples of 16 pixels, and only one of them pays straight up.
Dropping one chip at eleven offsets around number 9, whose square the
old formula put at x 140:

| asked for | hand settled at | paid |
|---|---|---|
| 120 | (112, 56) | nothing |
| 124 to 136 | (128, 56) | $9,000, a split |
| 140 to 152 | (144, 56) | $18,000, straight up |
| 156, 160 | (160, 56) | $9,000, a split |

so the straight-up point is x = 80 + 32 * (column - 1), y = 120, 88 or
56 by row. The 0/00 column at x = 48 is not on that lattice: it has
exactly two rest points, (48, 64) for 00 and (48, 112) for 0, each eight
pixels inside the grid row it stands beside. Asking for 140 put half a
stack on the split at 128 and half on 144, and a hundred-chip round paid
$1,350,000 instead of $1,800,000 before this was found.

## The measured minimum

Every wait in a round was a margin somebody chose, and none of them was
the game's own. All of them were bisected from checkpoints at the
high-limit table, restored before every trial. The press lengths and the
spin and pay waits are from `_runs/2026-09-14T08-55-57Z-caesars-minimum`;
the cycle as a whole was re-measured in
`_runs/2026-09-14T09-46-52Z-caesars-cycle`, whose trials are a hundred
chips long and whose oracle is the wallet: a trial passes only when all
hundred chips come out of it, at four checkpoints taken after zero, one,
two and three rounds.

A trial of three chips is not enough, and that cost a whole run. The
first pass of this work measured every press on its own with a
single-chip oracle, and a player built on it lost two of nine rounds to
stakes that took no chip at all.

The oracle is the wallet, which is the only counter that answers at this
table. `0xd916`, the word that holds the dollars on the table at the $500
roulette, reads 0 through a hundred chips here -- a stake built on it
placed 167 chips where 100 were asked for, in
`_runs/2026-09-14T10-04-16Z-caesars-verified`. The wallet is down $500 as
soon as a chip is lifted, and that is what `stake` counts.

The wallet does not answer at once. The $500 leaves it six frames after
the drop press, measured frame by frame over three chips in a row, so a
stake that reads the wallet the instant it drops a chip reads the chip
before it. `stake` therefore counts once, at the end: it waits
`CHIP_SETTLE` 8 frames, works out how many chips the wallet still owes,
and carries those too, up to `CHIP_TRIES` passes. That costs 8 frames a
stake rather than 8 a chip, and it makes the stake exact by measurement
instead of by margin.

| wait | was | least that works | the engine holds |
|---|---|---|---|
| the wallet-open press | 6 held, 20 after | 1 held, 2 after | 1 held, 4 after |
| the rack click | 6 held, 20 after | 3 held | 6 held |
| the wallet-close press | 6 held, 20 after | 1 held, 1 after | 1 held, 4 after |
| the drop press | 6 held, 40 after | 1 held, 0 after | 1 held, 0 after |
| the first chip's reach for the $500 rack | 8 bursts of 8, 2 between, 80 | 48 held `right` | 96 |
| every later chip's reach for it | the same 80 | nothing at all | nothing |
| the aim | 2 `move_to` passes, 10-frame rest | 1 pass, no rest | 1 pass |
| `hand.PRESS_AFTER` | 30 | 0 | 0 |
| `SPIN_FRAMES` | 6 | 1 | 1 |
| `SETTLE` | 620 | 24 | 24 |
| `PAY_FRAMES`, `PAY_STEP` | 900, 120 | 810, 30 | 810, 30 |
| the pad | `PAD_PER_CHIP` 280 x chips + 400 | a rehearsal + `SPIN_LEAD` | rehearsal + 400 |
| the wallet's answer to a drop | not read | 6 frames | `CHIP_SETTLE` 8, once a stake |

What the game answered, question by question.

It takes input on every frame. A one-frame `right` from each of the eight
phases of the hand's sway moves the hand the same 120 pixels, and a
one-frame C opens the wallet from all eight. There is no cadence to be a
multiple of, unlike Theme Park's cursor.

The open press and the rack click are one wait, not two. What the panel
needs is nine frames from the first frame of the C press to the last
frame of the click, and it does not care how they are split: 1 held with
2 after and a 6-frame click lifts a hundred chips out of a hundred, and
so does 1 held with 5 after and a 3-frame click; every split totalling
eight lifts 67 of 100. The engine spends eleven, 1 held with 4 after and
a 6-frame click.

The wait *after* the close press is the one that a three-chip trial
cannot see, and it is the one that broke a run. With nothing after it the
close is swallowed every other chip, so the panel is already open when
the next chip's C press arrives and that press closes it instead: exactly
50 chips of 100 are lifted, at all three checkpoints tried. One frame
after the close is enough for 100 of 100 at all three; two frames lost
one chip of a hundred at one of them; the engine holds four.

The rack does not have to be reached at all after the first chip. The
wallet panel holds five racks, $1, $5, $25, $100 and $500, and the $500
one is the rightmost, which is why holding `right` finds it; but the
panel reopens on the rack it was last left on, so from the second chip
onward the cursor is already on $500 and no `right` is needed. The first
chip of a stake does have to cross, and how far it has to cross depends
on where the last round left the hand: 26 frames of `right` is enough
from some checkpoints and loses the chip from others, 48, 64 and 96 all
lift every chip from every checkpoint tried. The engine sweeps 96, once
per stake, because a first chip lost costs a whole round.

One chip at a time is right, and it is the game that says so: two clicks
on the rack with the wallet open take $500, not $1,000.

The wallet does not have to be closed for the drop to land -- three chips
carried with the panel left open still stake $1,500 -- which is exactly
why a swallowed close is invisible in a single chip and halves a hundred.

The aim can be one pass with no rest, and the drop needs nothing from the
hand being still: `move_to` reads the hand two frames after its last
hold, the chip is dropped whether or not the glide has ended, and it
lands on the straight-up point every time. What the drop does need is an
aim before *every* chip: with the aim skipped after the first, only the
first chip of three is staked.

The pocket is not written 620 frames after the spin. It is written at
`0xd886` 21 frames after the press, and `0x288a` moves on the same frame:
from two different checkpoints a spin at offset 4,000 wrote 7 and 33 at
+21 and neither changed over the next 880 frames. The 620 was the banner,
not the number. The wallet is credited 849 frames after the press,
measured the same way and the same in every round of
`_runs/2026-09-14T08-54-51Z-caesars-rounds`.

The double zero was the one square the shorter cycle could not stake,
and it turned out to be an aim and not a pickup. (48, 76) is not a rest
point and never was: the zero column holds only (48, 64) and (48, 112),
so a hand asked for (48, 76) settles on 0 and the chips go on the wrong
square -- which is what "a hundred chips lift one" was, the panel opening
off a corner the hand had been left in. With `DOUBLE` at (48, 64) and
one hold an axis, a hundred chips go on 00 in full: from a $1,822,000
checkpoint at the high-limit table the wallet came back $1,772,000,
exactly a hundred $500 chips, in 1,961 frames, with the hand at rest on
(48, 64) (`_runs/2026-09-14T10-23-59Z-caesars-hand-measure`). So 00 is
scouted like any other number again and the nudge-and-scout-again that
`scouted` did for a 00 draw is gone.

So a chip costs about 19 frames rather than 232. A hundred chips are
staked in 1,854 to 1,975 frames from three checkpoints and three squares
in `_runs/2026-09-14T10-12-43Z-caesars-verified`, and four chips from a
standing start cost 222 to 272, most of it the first chip's sweep and its
walk to the square.

The pad is now measured instead of estimated, and the measurement is
free. `pad` has to be the frames the stake will take, because the
scouting spin and the betting spin must be pressed the same number of
frames after the checkpoint. The engine therefore *rehearses*: from the
checkpoint it stakes the whole count on square 1 and presses the scouting
spin `SPIN_LEAD` frames after it, which costs exactly what idling for the
same offset would have cost and answers what the stake takes to the
frame. It then restores and stakes on the drawn number. Only the aim
differs between squares -- a hundred chips cost 1,854 to 1,975 frames, a
spread of 121 -- so `SPIN_LEAD` 400 covers the difference, and the idle
at the end of the stake is what is left of it, 294 to 406 frames against
about 5,100 before. The rehearsal has to idle that lead itself before its
scouting spin, or the two spins are pressed at different offsets and the
bet rides on whichever pocket the draw's plateau happens to hold: a
hundred-chip round staked $50,000 on the 0 its scout had named and was
paid nothing, $72,000 down to $22,000, in
`_runs/2026-09-14T09-50-02Z-caesars-costs`, before the lead was moved
ahead of the scout. When a stake does outrun its pad the engine re-scouts
at the longer offset rather than betting on a misaligned spin.

A hundred-chip round is about 5,590 frames, against 59,312 before. These
are the rounds of the player run
`_runs/2026-09-14T11-08-36Z-caesars-hand`, every one paid in full and
every verdict passed:

| round | chips | frames a round | wallet |
|---|---|---|---|
| 1 | 4 | 2,342 | $2,000 -> $72,000 |
| 2 | 100 | 5,548 | $72,000 -> $1,822,000 |
| 3 | 100 | 5,676 | $1,822,000 -> $3,572,000 |
| 4 | 100 | 5,580 | $3,572,000 -> $5,322,000 |
| 5 | 100 | 5,518 | $5,322,000 -> $7,072,000 |
| 6 | 100 | 5,556 | $7,072,000 -> $8,822,000 |
| 7 | 100 | 5,636 | $8,822,000 -> $9,999,999 |

The whole player is 60,275 frames recorded and 27,819 kept, against
201,364 kept in the clean film before this work.

## The slot, 231 pulls deep

At JACKPOT JUNGLE (1055, 802), $100 a coin, three coins in, from one
checkpoint, no restore between pulls. It is 231 pulls and not the 300
asked for, because the wallet ran out: $2,000 is what the tape's account
holds, three coins are $300 a pull, and there is no way to carry a
bigger wallet to a machine (see the findings below).

| the pull paid | in coins | times |
|---|---|---|
| nothing | 0 | 91 |
| $200 | 2 | 64 |
| $400 | 4 | 15 |
| $500 | 5 | 28 |
| $700 | 7 | 11 |
| $1,000 | 10 | 18 |
| $1,500 | 15 | 3 |
| $4,500 | 45 | 1 |

$69,300 staked, $67,500 back: 97.4 %, or -$7.79 a pull, over 1,036
frames a pull. The 5,000-coin line never came up, and neither did the
2,000, the 1,000, the 200 of three sevens or the 80 of three bars. The
best single line in 231 pulls was one 40-coin two-bar, inside that
$4,500 pull, which is 40 + 5 across two lines.

Fewer coins is worse, and not only because fewer lines pay. The same
checkpoint with one coin a pull went broke after 74 pulls, $7,400
staked and $5,400 back, 73.0 %; with two coins after 84 pulls, $16,800
staked and $14,900 back, 88.7 %.

Those three runs also answer a question the determinism section left
open. Their reel stops agree pull for pull up to pull 12 and part after
it -- pull 13 is `(11, 10, 8)` with one and two coins and `(20, 18, 18)`
with three -- so a slot's draw is stable against the frame a lever is
pulled but not against a whole sequence of differently-paced pulls. A
scouting loop cannot assume the nth pull is the same pull whatever the
stake.

The machine ignores which rack the chip came from. Feeding three chips
from the $1 rack and from the $5 rack both took $300 and both stopped
the reels at `(0, 0, 2)`: the denomination is the machine's, and the
wallet's racks are only a way of holding a chip.

## hand.py -- the pointing hand

`move_to(run, x, y)` steers `0xcbc4`/`0xcbc8` onto a target and answers
where the hand came to rest; `press(run, button, frames, after)` clicks
where it stands. `at(run)` reads the pair. `scenario.py` uses them
instead of its own copy.

### What a held direction does

From `_runs/2026-09-14T10-23-59Z-caesars-hand-measure`, on a roulette
table, each direction measured from the corner that gives it the most
room -- `right` from (80, 88), `left` from (432, 88), `down` from
(208, 56), `up` from (256, 152) -- restoring the same checkpoint before
every hold. All four give the same numbers, on their own axis and zero
on the other: there is no drift. "rest" is where the hand stops, "read
at +2" is `0xcbc4` two frames after the release, and "still to come" is
the pixels between the two.

| frames held | rest | read at +2 | still to come |
|---|---|---|---|
| 1 | 16 | 8 | 8 |
| 2 | 16 | 12 | 4 |
| 3 | 16 | 16 | 0 |
| 4 | 16 | 16 | 0 |
| 5 | 32 | 24 | 8 |
| 6 | 32 | 28 | 4 |
| 7 | 32 | 32 | 0 |
| 8 | 32 | 32 | 0 |
| 9 | 48 | 40 | 8 |
| 10 | 48 | 44 | 4 |
| 11 | 48 | 48 | 0 |
| 12 | 48 | 48 | 0 |
| 13 | 64 | 56 | 8 |
| 14 | 64 | 60 | 4 |
| 15 | 64 | 64 | 0 |
| 16 | 64 | 64 | 0 |
| 20 | 80 | 80 | 0 |
| 24 | 96 | 96 | 0 |
| 32 | 128 | 128 | 0 |
| 40 | 160 | 160 | 0 |
| 48 | 192 | 192 | 0 |
| 56 | 224 | 224 | 0 |
| 64 | 256 | 256 | 0 |

So a step is 16 pixels and four frames, the hand is one frame behind the
pad, and a hold of 4n - 1 frames lands n steps with nothing left to
glide -- the read two frames later is already final. Only holds that are
1 or 2 past a multiple of 4 glide at all, and they glide 8 or 4 pixels.

The panel's own edges are the only thing that breaks it. `up` from
(256, 152) stops at -96, y 56, from 24 frames on. `down` from (208, 56)
is clean to +80, and then leaves the number grid: 24 frames land
(256, 151) and 40 or more land (224, 184), because the outside-bet strip
below the grid is a different lattice and crossing into it moves x too.
`right` off the 0/00 column moves +32 on x and -8 on y for its first
step, that column being neither on the grid's pitch nor on its rows.

The game does not take a diagonal. Holding `right` and `down` together
moves the hand exactly as `right` alone does -- 16 pixels for 1 or 2
frames, 32 for 8, 128 for 32 -- and the vertical not at all; the same
for the other three pairs. So a move is one hold an axis, never both.

### What a move costs

`move_to` holds the far axis once and the near axis once, then makes at
most `CORRECTIONS` one more pass, in case a clamp or the strip boundary
swallowed part of it. From a settled hand at (208, 88) on a roulette
table, restoring the same checkpoint before each, against the burst loop
that was here before:

| asked to move | old frames | old rest | new frames | new rest |
|---|---|---|---|---|
| 0 | 0 | (208, 88) | 0 | (208, 88) |
| 8 right | 3 | (224, 88) | 0 | (208, 88) |
| 8 down | 3 | (208, 104) | 0 | (208, 88) |
| 32 right | 9 | (240, 88) | 9 | (240, 88) |
| 32 down | 9 | (208, 120) | 9 | (208, 120) |
| 32 up | 9 | (208, 56) | 9 | (208, 56) |
| 64 right | 18 | (272, 88) | 17 | (272, 88) |
| 64 down | 39 | (128, 152) | 25 | (256, 151) |
| 128 right | 36 | (336, 88) | 33 | (336, 88) |
| 128 down | 77 | (160, 184) | 53 | (160, 184) |
| 132 left | 36 | (80, 88) | 33 | (80, 88) |
| 184 right | 52 | (400, 88) | 45 | (384, 88) |

Every move that is a whole number of steps now costs one hold and lands
on the square, and the two that are not -- 8 pixels is half a step -- ask
for no hold at all rather than stepping past and stepping back; the old
loop spent three frames to finish 8 pixels further from the target than
it started. 64 down asks for a point on the outside-bet strip and
neither hand lands on it; 128 down is past the panel's clamp, and the
clamp is not a failure -- the panel ends there.

## The cap

The wallet does not stop at $65,535 -- it carried into `0xcdd0` and the
panel printed `71900.00` -- and it does stop at $9,999,999.

`engine.py` won to it. Going into its seventh round the wallet held
$8,822,000; the round staked a hundred $500 chips on 12, the number its
scout had named, the table owed $1,800,000, and the wallet came back
`9999999`. The eighth, ninth and tenth rounds each staked $50,000 on the
scouted number, each won it, and each ended on `9999999` again. So the
counter is a 68000 longword that the game clamps at seven digits, and
999,999,999 cannot be won in this casino: the most a wallet holds is a
hundredth of the goal. That run took seven rounds and 361,424 frames to
get there; the same seven rounds now take 35,856 of `player.py`'s 60,275
(`_runs/2026-09-14T11-08-36Z-caesars-hand`). The rows below that count
to 999,999,999 are arithmetic, not a plan.

The biggest single round measured is $18,000 back on a $500 chip at
roulette, $17,500 of it profit. The biggest a round can be, on the
banners: $180,000 back on a $5,000 straight-up bet at the high-limit
roulette, if ten chips stack; 5,000 coins on a $100 slot machine or a
$100 video poker machine, which is $500,000; and keno's advertised
$300,000.

What a round costs is measured, and it was cut by a factor of eleven when
every wait in it was measured too -- "The measured minimum" above. A
scouted round at the high-limit roulette is: checkpoint, rehearse the
stake on square 1, spin, read `0xd886`, restore, carry the chips one at
a time onto the drawn number, idle the difference, spin, wait for the
wallet. The rehearsal is what makes the offset exact and it costs
nothing, because the scouting spin has to be pressed the same number of
frames after the checkpoint as the betting one and those frames have to
go somewhere. The `was` column is the old margins, from `engine.py` runs
at the table; the rest is `_runs/2026-09-14T10-13-50Z-caesars-costs`:

| engine | was | frames a round | profit a round | rounds | frames to 999,999,999 |
|---|---|---|---|---|---|
| $500 straight up, one chip | 3,872 | 2,176 | $17,500 | 57,143 | 1.2 x 10^8 |
| four chips, $2,000 | 5,552 | 2,278 | $70,000 | 14,286 | 3.3 x 10^7 |
| ten chips, $5,000 | 8,912 | 2,440 | $175,000 | 5,715 | 1.4 x 10^7 |
| a hundred chips, the table's cap | 59,312 | 5,500 | $1,750,000 | 572 | 3.1 x 10^6 |
| a $100 slot, three coins | 1,036 | -- | -$7.79 | never | -- |

The hundred-chip stack is still the engine: 3.1 x 10^6 frames, about half
an hour at the 1,500 fps this profile runs at, and a fortieth of what one
chip a round would cost. The slot is not an engine at all -- it
gives 97.4 % back, so it walks away from the goal at $7.79 a pull.

What is left of a round is carrying, not waiting: 3,824 of a hundred-chip
round's 5,500 are the two stakes, 930 is the table paying, 400 is the
lead the rehearsal idles so both spins fall on the same offset, and the
idle at the end of the betting stake is 294. An engine that could bet without
moving the hand a chip at a time, or one that found two idle offsets in
the same plateau of the draw -- the plateaus measured were 875 and 2,100
frames wide -- is the only thing left to cut. Neither is needed to reach
the goal.

## tapes/title-to-floor.yaml -- power on to the casino floor

    tash tape check examples/caesars/tapes/title-to-floor.yaml
    tash run --profile examples/caesars/profile.yaml \
      --tape examples/caesars/tapes/title-to-floor.yaml \
      --frames 60 --bundle _runs

Four segments:

| segment | anchor | waits |
|---|---|---|
| `attract` | none | 1,400 frames |
| `palace-front` | perceptual hash `50402f2f1cbf9e58` within 2 | 91 frames |
| `cashier` | perceptual hash `4437cbc837c0acbc` within 2 | 24 frames |
| `floor` | watch `player_x` greater than 0 | 32 frames |

The attract billboard pans and redraws the screen inside its frame, so
nothing on it holds still long enough to anchor on and no crop of it is
stable; from power on the frame count is, so that segment's anchor is
`none`. The palace front and the cashier are still pictures and their
perceptual hashes are exact.

The cashier will not deal with a walk-in. Pressing start twice reaches
`QUICKASHIER!`, whose third red button is `NEW ACCOUNT`; a first name is
typed on the keyboard below it, `OK` is clicked, and three more clicks
pass the two password screens. Skipping that and walking onto the floor
leaves the player with $0 and no account, and every machine refuses him.
The tape therefore ends on the floor at (704, 832) with $2,000 in the
wallet.

The cashier segment is 72 transitions, which is the hand's whole path
recorded from a feedback run and replayed open loop: 36 presses, where
before it was 155. The panel is not the table -- it ignores input for its
first 360 frames while its text prints, and then every direction animates
the hand to the next selectable widget rather than 16 pixels -- so
`move_to`'s lattice does not apply to it and a closed-loop hop is still
what records the path. Played twice from power on it ends both times on
frame 2,448, exact hash `12151488734187522061`, `money` 2000, player at
(704, 832).

The floor segment idles 32 frames because the floor is not walkable the
instant it appears: a step held 8 frames moves the player nothing 0, 4,
8, 12 or 16 frames after the tape's last click, 8 pixels at 20 and the
full 16 from 24 on. It has to be in the tape rather than in the walker,
because `scenario.walk` restores a step that moved nothing -- that is
how it finds a wall -- so a walk started too early throws away the very
frames that would have woken the floor and stalls on the spot. The old
tape idled 160 frames there without meaning to.

## tapes/casino-walk.yaml -- the walk the recon recorded

    tash tape check examples/caesars/tapes/casino-walk.yaml

    tape    2026-09-14T01-44-39Z-casino-walk (2 segments)
    core    genesis_plus_gx
    profile examples/caesars/profile.yaml
      start        none                        timeout 600   304 transitions
      north-slots  exact_hash e1a60926ac89f921 timeout 600  1096 transitions

It replays from power on, and twice to the same frame:

    tash run --profile examples/caesars/profile.yaml \
      --tape examples/caesars/tapes/casino-walk.yaml \
      --frames 60 --bundle _runs

9,477 frames of tape and 60 frames on, 9,537 in all, both times: exact
hash `af5b835ab9bbf1ba`, `money` 2000, hand (228, 224), player (280,
410), 0 retries, 4,400 watch records and none refused.

The finding is the length. The run this tape came out of was 23,872
frames and opened all twelve machines; the tape it wrote is 9,477 frames
in two segments, because that run restored the floor checkpoint before
each visit and a recorded tape keeps only the line that was kept. What
replays is the last thread through the run, not the walk. A tape from a
bundle is the shortest input that reaches the bundle's last frame, which
is the useful thing for a player and the wrong thing to reach for if
what is wanted is everything the run did.

## scenario.py -- the tape, then every game once

    tash run --profile examples/caesars/profile.yaml \
      --scenario examples/caesars/scenario.py --frames 1 --bundle _runs

Plays the tape, checkpoints the floor, and from that checkpoint tours the
floor: `stop()` walks to a game from wherever the player is standing,
faces it, clicks it, waits `BANNER_WAIT` for the welcome banner that
holds the hand still, plays it once, shoots it, marks it, judges it and
leaves by the game's own exit. There is no restore to the floor between
games, so the eleven plays are one line and the wallet carries from each
into the next. `MARK_SETTLE` is the one frame between a mark and
whatever checkpoints next, without which the probe's restore folds the
mark out of the film.

Walking is a greedy two-axis walk with a stall counter, and where that is
not enough -- the north half of the floor is a maze of machine banks --
it falls back to the same depth-first walk with backtracking that mapped
the place. `inside()` is the verdict's oracle and it takes two signals,
because neither alone is sound: a direction that moves the hand rather
than the player means a game screen, and a difference hash more than 16
bits from the floor's means the screen changed altogether. A player
walking one cell scrolls the camera and moves 30 bits of that hash on its
own, which is why a screen-change test by itself calls a wall a game.

## games.py -- one play of each, and the order that costs least

    tash run --profile examples/caesars/profile.yaml \
      --scenario examples/caesars/player.py --bundle _runs \
      --name caesars-tour --video-stride 10

One function per game, each playing it once for the least the wallet can
put on it, and a `GAMES` tuple that is the tour's order: the game, the
waypoints its walk needs, where the player stands, the play and the exit.

### The order

The tour is a path through all eleven that ends at the high-limit table,
where the round loop takes over. Each leg was measured once on its own,
from the previous game's exit: walk it, read `run.frames()` before and
after, restore. The order below is the cheapest of the ones measured.

| # | leg | frames |
|---|---|---|
| 1 | floor -> cashier | 46 |
| 2 | cashier -> video poker | 64 |
| 3 | video poker -> horse racing | 442 |
| 4 | horse racing -> jungle slots | 408 |
| 5 | jungle slots -> triple jackpot slots | 280 |
| 6 | triple jackpot slots -> roulette | 172 |
| 7 | roulette -> craps | 91 |
| 8 | craps -> blackjack, high limit | 337 |
| 9 | blackjack, high limit -> blackjack, low limit | 226 |
| 10 | blackjack, low limit -> keno | 262 |
| 11 | keno -> roulette, high limit | 7,560 |

That is 9,888 frames of walking for the whole tour, and the last leg is
three quarters of it: everything but the high-limit table is on the
south half of the floor, and the north half is the maze the depth-first
walk was written for. The last leg is measured with the fallback in it
-- 3,519 frames of walking to the dead end at (247, 402) and 4,041 of
seeking from there -- because no run of waypoints alone arrives.

Leaving keno is its own problem, and it sets that leg's shape. The
ticket window sits in an alcove, and the player who steps out of it
stands at (385, 330) inside a pocket about 110 by 76 pixels wide: the
greedy walk can reach the alcove's door at (433, 394) and nothing else,
and asked for any point outside it stops at (463, 330) every time. The
way out is the way in, south down x = 439 to the aisle at y = 538, and
the leg is written as that retreat -- door, aisle, the low-limit
blackjack table, then the north corridor's last waypoint (263, 322),
which is where `engine.py` leaves the walk and lets the seek finish.
Asking the seek to solve the pocket instead costs 28,080 frames and
still misses the table; going west first through (137, 410) arrives but
costs 22,248.

Legs measured and not taken, which is why the order is this one:

| leg | frames |
|---|---|
| floor -> video poker | 109 |
| floor -> jungle slots | 217 |
| cashier -> jungle slots | 172 |
| cashier -> horse racing | 280 |
| video poker -> jungle slots | 109 |
| triple jackpot slots -> horse racing | 505 |
| roulette -> blackjack, high limit | 427 |
| craps -> blackjack, low limit | 499 |
| blackjack, low limit -> roulette, high limit | 5,897 |
| keno -> roulette, high limit, waypoints only | never arrives |
| keno -> roulette, high limit, west through (137, 410) | 22,248 |
| jungle slots -> horse racing | 10,550 |
| horse racing -> jungle slots, without the waypoints | 5,151 |
| horse racing -> video poker | 9,398 |
| horse racing -> cashier | 10,730 |
| horse racing -> triple jackpot slots | 21,837 |
| horse racing -> blackjack, high limit | 26,660 |
| blackjack, high limit -> keno | 25,652 |

The horse racing terminal is the reason the order looks odd. Walking
away from it costs thousands of frames in every direction except back
the way the player came, so it is entered from the video poker end and
left through the two waypoints `(752, 560)` and `(752, 802)`, which is
the 408-frame leg above; anything else is five to twenty-six thousand.

### Playing each of them once

Every play takes the smallest chip the wallet can put on that game, runs
it to its outcome and reads the wallet after. The wallet's racks are not
five fixed stacks: the balance is decomposed greedily into $500, $100,
$25, $5 and $1, and only the denominations in that decomposition have
chips, so at $2,000 the only rack with chips on it is the $500 one. The
"stake" column is therefore the least the game took at that point of the
tour, not the least its banner allows.

| game | stake | outcome | frames | wallet after |
|---|---|---|---|---|
| cashier | $0 | the screen reading YOUR BALANCE IS $2000 | 2,918 | $2,000 |
| video poker | $5 | a hand dealt and stood on | 2,989 | $2,000 |
| horse racing | $2 | a race run, -$2 on it | 27,337 | $1,998 |
| slots "JACKPOT JUNGLE" | $100 | the reels stopped on (9, 6, 20) | 12,050 | $1,898 |
| slots "TRIPLE JACKPOT" | $5 | the reels stopped on (13, 2, 19) | 9,773 | $1,893 |
| roulette | $500 | the ball stopped on 15, +$17,500 | 7,961 | $19,393 |
| craps | $10 | the dice thrown once, +$20 on the line | 6,179 | $19,403 |
| blackjack, high limit | $100 | a hand dealt and played out, +$150 | 15,668 | $19,553 |
| blackjack, low limit | $25 | a hand dealt and played out, -$25 | 8,769 | $19,528 |
| keno | $1 | a ticket registered and drawn against | 9,427 | $19,527 |
| roulette, high limit | $500 | the ball stopped on 37, +$17,500 | 90,540 | $37,027 |

The frames are each stop's whole cost -- the walk to it, the play, and
the way out -- as `player.py` prints them, and they carry the restore
probe with them: every checkpoint a walk or a scout takes costs 240
frames of probing, so the walk-only leg table above is the smaller half
of what a stop pays. The run below spends 196,299 frames reaching the
high-limit table and 250,892 altogether, of which 85,584 are kept; the
tour is the first 67,983 kept frames of the film, where the
`roulette-high` mark sits, and the six rounds are the rest.

    _runs/2026-09-14T13-16-18Z-caesars-tour
    ran 250,892 frames (85,584 kept, probed 565 checkpoints, 0 parted)
    _runs/2026-09-14T13-19-19Z-caesars-tour-clean
    replayed 85,584 of the 85,584 the tape keeps, watches match

Notes the table cannot hold:

- The cashier is on the list because the goal counts every screen on the
  floor. `BALANCE` costs nothing and its screen is the verdict's outcome.
- Craps is the one table whose own minimum was affordable in chips: $10,
  one chip, and the first come-out roll paid $20.
- The two slots are different machines: the jungle slot is a $100 one
  and the triple jackpot a $5 one, which is why one play costs a hundred
  dollars and the other five.
- Roulette is scouted, because the pocket is frame-driven and this
  README's own scout-and-restore reads it: rehearse the stake, spin,
  read `0xd886`, restore, stake one $500 chip on that number and spin
  again. The open floor's table is `engine.scouted` itself; the
  high-limit table is the tour's own `high_scout`, for the reason two
  notes below. Both paid 35:1.
- Keno takes a coin and registers the ticket, and no draw ever appears on
  the board within 1,200 frames of it; the ticket is the outcome the
  screen gives.
- The horse race is the tour's most expensive play by far in its own
  right -- 27,000 frames for a $2 bet -- because a race is run in real
  time and there is no way found to skip it. The high-limit stop costs
  more only because the walk to it is the 7,560-frame leg.
- The high-limit table answers the spin button only in a window that
  opens with the stake. One $500 chip staked and then idled over
  `engine.SPIN_LEAD`'s 400 frames never spins: `0x288a` ticks for about
  150 frames after the chip lands and then stands still, and the press
  does nothing. Four chips spin on the same lead because carrying them
  takes 881 frames instead of 230. `engine.scouted` cannot recover from
  it, because its retry re-spins without re-staking, so the one-chip
  stake it is given at this table leaves it turning forever -- a run
  that sat 29 minutes on one game. The tour scouts this table itself,
  with the lead measured to answer, and leaves `engine.py` alone.
- A chip on the zero square is taken and never pays. Scouted with a
  100-frame lead the ball stopped on 0, the tour bet 0 on the same lead,
  the ball stopped on 0 again and the wallet went $19,527 -> $19,027.
  The tour skips that pocket and nudges the lead instead, which is what
  the old `DOUBLE_ZERO` guard did for the other end of the layout.
- A mark taken at the same frame as the next checkpoint does not reach
  the film. The probe restores to the checkpoint, the fold takes the
  mark with it, and the replayed line has ten of eleven. One frame
  between the mark and whatever checkpoints next is enough, and that is
  what `MARK_SETTLE` is.

## engine.py -- the engine, ten scouted rounds of it

    tash run --profile examples/caesars/profile.yaml \
      --scenario examples/caesars/engine.py --bundle _runs

Plays `tapes/title-to-floor.yaml`, walks north to the high-limit
roulette at (624, 115), opens it, and runs ten scouted rounds with
`hand.py`. Each round is: checkpoint, rehearse the stake on square 1,
idle `SPIN_LEAD`, spin, read `0xd886`, restore, carry up to a hundred
$500 chips onto that number one at a time, idle what is left of the same
offset, spin, wait for the wallet, mark, and judge it against
`before + chips * $17,500`. It bets what the wallet can
afford up to the table's cap, so the first round is four chips and the
rest are a hundred.

Reaching the table takes 16,233 frames: the tape is 4,086 of them and
the walk is the rest, and the walk needs its depth-first fallback --
the greedy walk stops dead at (328, 306) against a bank of machines,
whichever way it is asked to go from there.

    _runs/2026-09-14T03-03-12Z-engine
    555,593 frames in 357.72 s, 1,553 fps, D2, $2,000 -> $9,999,999

| round | chips | pocket | wallet after | frames | judged |
|---|---|---|---|---|---|
| 1 | 4 | 0 | $72,000 | 5,552 | pass |
| 2 | 100 | 14 | $1,822,000 | 59,312 | pass |
| 3 | 100 | 24 | $3,572,000 | 59,312 | pass |
| 4 | 100 | 2 | $5,322,000 | 59,312 | pass |
| 5 | 100 | 37 | $7,072,000 | 59,312 | pass |
| 6 | 100 | 26 | $8,822,000 | 59,312 | pass |
| 7 | 100 | 12 | $9,999,999 | 59,312 | fail, wanted $10,572,000 |
| 8 | 100 | 3 | $9,999,999 | 59,312 | fail, wanted $11,749,999 |
| 9 | 100 | 28 | $9,999,999 | 59,312 | fail, wanted $11,749,999 |
| 10 | 100 | 2 | $9,999,999 | 59,312 | fail, wanted $11,749,999 |

Ten rounds, ten correct predictions of the pocket -- round 5 drew 37,
the double zero, and a hundred chips on it were paid in full
-- and four failed verdicts, all of them the wallet's ceiling rather
than a missed bet. That run is what the waits cost before they were
measured: the offset the spins were aligned on was a flat 28,400 because
carrying a hundred chips took between 21,526 and 26,800 frames. It is
now the rehearsal's own length plus `SPIN_LEAD`.

`player.py` runs the same rounds and ends on the wallet panel: after the
last round it presses C and holds for `WALLET_HOLD` frames before the
final shot and the verdict. Without it the film's last frame is the
table with the hand on it and no figure anywhere, so a viewer cannot see
that $9,999,999 was reached.

## Facts a plan rests on (checked 2026-09-14 on dev)

- The wallet is the 68000 longword at `0xcdd0` in Genesis Plus GX's
  byte-swapped work RAM: `little16(0xcdd0) * 65536 + little16(0xcdd2)`,
  plain binary, dollars. It passes $65,535 without stopping -- $71,900
  read back as `0x0001`, `0x18dc` and printed `71900.00` -- but the game
  clamps it at $9,999,999, won to and measured, so 999,999,999 cannot be
  reached in this game at all. The roulette pocket is `0xd886`. The
  pointing hand is
  `0xcbc4`/`0xcbc8`, the walking player `0xd3bc`/`0xd3be`, the reel stops
  `0xd432`, `0xd436`, `0xd43a`, the draw counter `0x288a`/`0x288c`.
- A slot's RNG is draw-driven, not frame-driven. Pulling a slot's lever
  0, 1, 2, 3, 5, 8, 13, 21, 40, 100, 200, 600 or 1,000 frames later than
  the checkpoint gives the identical frame hash and the identical
  payout. A roulette wheel is the other way round: the pocket is decided
  by the frame the spin is pressed on, in plateaus 875 and 2,100 frames
  wide, so a scouted round has to press both its spins at the same offset
  from the checkpoint, and some offsets do not spin at all.
- The outcome is drawn before the stake is read. Three coins and one coin
  stop a slot's reels at the same `(4112, 4883, 257)`; a roulette ball
  stops on the same number whatever square the chip is on. So the
  exploitable move is scout-and-restore: play a round to see it, restore,
  and bet the maximum on what is about to happen. Five such rounds took
  $1,900 to $71,900 at $18,000 a round. The stake is read late but not
  forever: a sequence of slot pulls diverges once the pulls are paced
  differently, so only the next draw is safe to predict.
- A stack pays per chip, up to the table's limit and not past it. A
  hundred $500 chips straight up at the high-limit roulette pay
  $1,800,000; the 101st and the 105th chip are taken and pay nothing.
  That round costs 59,312 frames measured, $1,750,000 profit, so
  999,999,999 is 572 rounds and 3.4 x 10^7 frames -- the cheapest engine
  in the casino. One chip a round is 3,872 frames and 2.2 x 10^8.
- A game screen does have a way out, and it is a chord: start and down
  held together for 20 frames leaves a slot, a table, video poker and the
  cashier, and the winnings stay in the wallet. No single button does it
  -- start, select, X, Y, Z and the shoulder buttons alone do nothing,
  which is what the note here used to record, and the wallet panel's
  `<|>` glyph does sit outside the hand's clamp. Two screens are their
  own case: the horse racing terminal answers only a click on LOG OUT,
  and the keno board has an exit button of its own beside the ticket's
  Done. The chord does not always take on the first press, so the tour
  presses it up to four times and checks by trying to walk. This is what
  lets the tour keep one line instead of restoring the floor per game,
  and why the slot scout's 231 pulls were bounded by the $2,000 account
  rather than by the way back.
- Eleven kinds of game, all opened from the floor: cashier scratch cards,
  video poker, two slot families, roulette at two limits, craps,
  blackjack at two limits, keno, and a horse racing terminal. Red dog,
  big six and baccarat are not there. High-limit tables refuse a small
  wallet with a banner rather than a locked door.
- The floor is about 1,280 by 900 pixels and the route between two of its
  halves is not a straight line; a greedy walk gets stuck against machine
  banks and needs a depth-first walk with backtracking, about 5,400 steps
  to cross to the north-west corner.
- Every screen in this game is driven by a pointing hand, and which
  panel it is on decides how it moves. On a table it is a 16 pixel
  lattice, one step every four frames, straight and with no drift, so a
  move is one hold an axis and needs no feedback at all; on the cashier
  it is a widget selector that ignores input for its first 360 frames and
  animates between hotspots. Open-loop input reproduces exactly, but
  writing the cashier's takes a closed loop first; that segment is 72
  transitions of recorded feedback.
- The harness has no memory write, so a cap or a jackpot cannot be set up
  directly, and `tash run` takes no positional scenario arguments, so a
  scenario is parameterised through the environment. A bundle's
  `tape.yaml` is the folded kept line and not the run -- 9,477 frames out
  of a 23,872-frame walk -- and the run says which is which: the exit
  line and the report read `N recorded, M kept`, and `run.yaml` carries
  the kept count as `kept`.
