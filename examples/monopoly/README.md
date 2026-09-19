# examples/monopoly/

`profile.yaml` runs *Monopoly* (USA) on Genesis Plus GX from the cartridge
library in `roms/`. The goal this reconnaissance serves is "one game won against the
computer opponents as fast as the game allows, from power on, on one tape
that replays", so the hunt went after the purses, the board positions, the
deed table and the dice. **Nothing here is a plan and no game was won by
play**: the win was reached once from a rigged setup, to see what the game
draws when it ends, and `game.py` played 74 turns of a real game before a
blind player ran out of answers.

Three findings shape any plan built on this.

**The frame a roll is pressed on chooses the dice.** The RNG is a 16-bit
word that shifts once a frame whether or not the pad is touched, and the
dice are two consecutive samples of it taken when Genesis A goes down. One
frame of waiting is a different roll; every total from 2 to 12 is inside a
36-frame window. The dice are a search space, not a hazard.

**The only win the game declares is bankruptcy.** SHORT GAME's time limit
does end the game — a five-minute game stops 20,278 frames after the start
press — but it stops it into a black screen and the attract demo, with no
winner plate and the board reset. The plate that says a game was won is the
one behind "AMANDA IS BANKRUPT!".

**A player that only presses Genesis A cannot finish a game.** Pressed at
every rest it plays turns correctly, but it wanders into the options menu,
where A only goes deeper, and it hammers screens that are still arriving,
which keeps them from arriving. With a rule for each of those it reached
turn 74 and stopped where a blind player must: "YOU CAN'T PAY YOUR BILL —
PRESS A TO RAISE MONEY OR PRESS C TO DECLARE BANKRUPTCY", a decision no
press-at-every-rest player has an answer to. The plan's player has to read
the state and choose the press, which is what the addresses below are
for.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `p1_square` | `0x84de` | 1 | - | the player's board square, 0 GO .. 39 |
| `p1_seat` | `0x84e1` | 1 | - | seat number, 1 for the human, 0 once bankrupt |
| `p1_cash` | `0x84e4` | 4 | little | the player's purse in dollars |
| `p1_doubles` | `0x84ef` | 1 | - | doubles rolled in a row, 3 sends to jail |
| `p2_square` | `0x84fa` | 1 | - | the opponent's square, the record 0x1c on |
| `p2_seat` | `0x84fd` | 1 | - | the opponent's seat number |
| `p2_cash` | `0x8500` | 4 | little | the opponent's purse |
| `players_left` | `0x84dd` | 1 | - | players still in the game, 1 when it is won |
| `current_player` | `0x85dd` | 1 | - | whose turn it is, 0 the human, 1 the first computer |
| `die_1` / `die_2` | `0x867e` / `0x867f` | 1 | - | the dice, 1..6 each, both 7 between rolls |
| `rng` | `0x84b8` | 2 | little | the word the dice are drawn from |
| `game_minutes` | `0x86bc` | 1 | - | SHORT GAME's limit in minutes, 0 no limit |

Little-endian on a 68000 is Genesis Plus GX keeping work RAM byte-swapped:
region offset R is 68000 address R xor 1, so a 68000 word at an even address
reads as a little-endian 16-bit word at the same offset. The purse is a
longword and reads the same way. A name in a player record is the exception
that proves it: `0x84f0` holds `LPYA REY 1` and comes out as `PLAYER 1`
after swapping each pair, which is how the record layout was confirmed.

## The hunt

### The player records

A width-4 hunt for 1500 at the first roll, filtered by a second `step` after
the first purchase (the first `step` only seeds), leaves `0x84e4` for the
human. The opponent's purse sits 28 bytes later at `0x8500`, and everything
else about a player repeats on the same 28-byte stride from `0x84de`:

| offset | width | what |
|---|---|---|
| +0 | 1 | board square, 0..39 |
| +3 | 1 | seat number, 1 and 2 here, 0 when the player is out |
| +6 | 4 | cash |
| +17 | 1 | consecutive doubles |
| +18 | 8 | the name, byte-swapped ASCII |

`state.player` reads a record whole and `state.name_of` unswaps the name;
printing both players at the first roll answers `PLAYER 1` and `AMANDA`,
which is the proof that the stride and the offsets are right rather than a
coincidence of two numbers.

### Who is in the game, and whose turn it is

`0x84dd` counts the players still in the game. It reads 2 through every
game played here and fell to 1 at the moment the rigged game ended, which
is the flag a scenario has to stop on: a press after it is a press into the
*next* game, and the harness watched one start itself that way, with a new
opponent, ELIZABETH, and $1500 in her purse.

`0x85dd` is the player on turn. It came out of intersecting full 64 KB
snapshots taken mid-turn during the human's turns against snapshots taken
mid-turn during the opponent's: one byte in the intersection holds 0 in
every sample of the first set and 1 in every sample of the second. It is
what `game.py` counts turns with, and what the tape's last two segments are
anchored on, because the two players' roll prompts differ by four bits of
perceptual hash and no picture anchor can tell them apart.

### The deed table

40 entries of 4 bytes from `0x85dc`, one for every board square including
the ones that are not deeds:

| offset | what |
|---|---|
| +0 | always 0 in everything played here |
| +1 | mortgaged, 0 or 1 |
| +2 | houses, 0..4, and 5 for a hotel |
| +3 | the owner's index, 255 for the bank |

Each column was moved on its own and watched. Buying Reading Railroad wrote
0 into `0x85dc + 4*5 + 3` and took $200 out of the purse in the same motion.
Mortgaging Mediterranean Avenue through OPTIONS -> MORTGAGE wrote 1 into
`0x85dc + 4*3 + 1` and put $30 in. Buying four houses on the brown group
wrote 2 and 2 into the two `+2` bytes and took $200. The hotel value was
read off PLACE HOTELS in the pre-game options, which writes 5.

### The dice

`0x867e` and `0x867f`, a byte each. They were found by a differential no
hunt can do: eight rolls from one checkpoint, each pressed one frame later
than the last, giving the totals 12, 7, 5, 8, 8, 8, 7, 9, then a sweep over
all 65,536 offsets for the one pair `raw[i]`, `raw[i+1]` whose sum matched
every total with both bytes in 1..6. Exactly one offset survives. Between
rolls both bytes read 7, which is the value a plan should treat as "no roll
on the table".

### The RNG, and what advances it

`0x84b8` as a little-endian word. It does not sit still: from one
checkpoint, stepping a frame at a time reads

    0x1a12  0x3424  0x6849  0xd092  0xa125  0x424b  0x8496  0x092c  0x1258

which is x -> 2x mod 65536 with a feedback bit coming in at the bottom — a
16-bit shift register clocked once a frame. Two checkpoints settle what
clocks it: 30 frames of nothing and 30 frames with `up` held produce the
*same* sequence of words, so **the frame advances it and the pad does
not**.

What the pad decides is *when* it is read. From the tape's first roll,
pressing Genesis A at offsets 0..7 from the same checkpoint gives

| offset | rng at the press | dice | total |
|---|---|---|---|
| 0 | 31524 | 2, 1 | 3 |
| 1 | 63048 | 3, 2 | 5 |
| 2 | 60560 | 6, 3 | 9 |
| 3 | 55584 | 5, 6 | 11 |
| 4 | 45633 | 4, 5 | 9 |
| 5 | 25731 | 3, 4 | 7 |
| 6 | 51463 | 5, 3 | 8 |
| 7 | 37391 | 5, 5 | 10 |

Every offset is a different roll, and the second die of the press at offset
k+1 is the first die of the press at offset k, which is what two
consecutive samples of a once-a-frame stream look like. A 36-frame sweep
from a later checkpoint gives the totals 12, 7, 5, 8, 8, 8, 9, 12, 11, 9,
7, 5, 7, 9, 7, 4, 2, 4, 4, 5, 7, 8, 10, 11, 12, 8, 5, 5, 5, 5, 6, 5, 2, 2,
3 — eleven of the eleven possible totals inside 36 frames of waiting.

**A held A does not roll again.** The hand shakes the dice for as long as A
is down and the move starts on the release, but the dice are latched at the
press: holds of 1, 2, 4, 8, 16 and 600 frames from one checkpoint all give
6, 6. A hold of 1,800 frames never resolves the turn — there is no
auto-repeat anywhere in this game, and every menu needs a press a step.

### The turn phase

Not found, and looked for twice: no byte in work RAM is constant across
every roll prompt and different at every mid-turn sample. The phase a plan
needs — is the game asking for a roll, offering a purchase, showing a card
— is on screen and not in a word. What stands in for it is the pair
(`current_player`, a still state block): `engine.settle` hands
`0x84dc..0x86ff` to `run.memory_stable`, and a block that has been still
for 30 frames means the game is waiting for a press. That is the whole of
the timing model `game.py` uses, and it is right for every screen except
one that is still arriving — the block is still behind a screen that has
not drawn yet, and the press is swallowed. "The press cadence" below is the
rule that covers it.

### Bankruptcy, and the plate that says the game is won

A game was rigged to end in one landing: ASSIGN PROPERTY in the pre-game
options gave all 28 deeds to the human, SET CASH with `down` held took the
opponent's purse to $0, and her first rent ended her. The sequence, from
the press that left the prompt:

| frames | what |
|---|---|
| 548 | "AMANDA IS BANKRUPT!", perceptual `497072db0b360dcd` |
| 775 | the winner's plate, perceptual `47b938c18c1e71dc` |

The plate is Mr. Monopoly on a golden dollar sign over "PLAYER 1 · $7190",
and it stayed up for at least 2,478 frames. In RAM the same moment is her
seat byte `0x84fd` going to 0 and `players_left` going 2 -> 1. There is no
WINNER or CONGRATULATIONS string anywhere in the ROM — the plate is drawn
as tiles — so a plan that wants to prove the win should expect on
`players_left` and use the perceptual hash as the picture beside it.

## The setup

### Power on to the first roll

Nothing in the setup can be held; every screen takes one press and ignores
the rest. From power on:

| screen | press | why |
|---|---|---|
| title, the walking man | START | Genesis A does nothing here |
| the vault | A | |
| NUMBER OF PLAYERS | A | **2 is already chosen and 2 is the minimum** |
| PLAYER 1 IS HUMAN | A | |
| the name grid | `down` x4, A | walks to END and takes the default PLAYER 1 |
| SELECT YOUR TOKEN | A | the token the browser opens on; no token differs |
| PLAYER 2 IS | `right`, A | moves HUMAN -> COMPUTER |
| the opponent gallery | A | opens on Amanda; the opponents differ only in style |
| PRESS A TO START | A | C here opens the pre-game options instead |

The game rolls for turn order itself, Amanda wins it in this line, and her
whole first turn is played before the human's first roll — twelve presses
of A at a 60-frame cadence cover it with room to spare.

**No game-length setting is taken, and that is a decision.** The pre-game
options hold SET CASH, ASSIGN PROPERTY, PLACE TOKEN, PLACE HOUSES, PLACE
HOTELS, SHORT GAME and LOAD PRESET GAME. SHORT GAME asks for LENGTH OF GAME
in minutes ("NO TIME LIMIT IF 0"), `right` adding 5 a press into
`0x86bc`, and then whether each player starts with two deeds. A
five-minute game was played to its end: the board is up until frame 20,278
after the start press, the screen is black from 20,279, and at 20,302 the
attract demo is running with both purses back at $1500. **The clock ends
the game without declaring anybody the winner**, so a plan that wants a win
cannot take the short game, and the twelve preset games (THE BIG BOYS,
CHAMPIONSHIP GAME, IT'S NOT FAIR and nine more) are shortcuts to a *board*,
not to an ending.

### The tape

`tapes/title-to-first-roll.yaml`, 10 segments, 2,621 frames, 50
transitions, 0 retries. Two things in it are not obvious:

- **The first segment has no anchor.** The title's walking man moves every
  frame, so no picture rests long enough for a perceptual hash to hold, and
  an anchor there fails on a frame's timing. Every other setup segment
  anchors on a perceptual hash within 2 to 4.
- **The last two segments anchor on a watch, not a picture.** The human's
  roll prompt and the opponent's are four bits of perceptual hash apart,
  which is inside the noise of the board's own animation;
  `watch current_player equal 1` and `watch current_player equal 0` say
  exactly whose prompt is up.

Played twice with 600 frames of settling after it, both runs stand at frame
3,221, exact `4f6f34c4f3c1a48c`, perceptual `15bf581b36c24bc4`, with
p1_cash 1500, p2_cash 1360, p2_square 11, current_player 0, die_1 and die_2
both 7 and rng 31524.

## The play

### What a turn asks, and the press for each

| the game asks | the press |
|---|---|
| roll | Genesis A; the dice are latched on the press |
| an unowned deed, after A on the deed card | A buys, **B auctions**, C shows the deed |
| rent, a card, a tax, GO TO JAIL | A dismisses |
| a bill the purse cannot pay | the vault: **A raises money**, C declares bankruptcy |
| the options, from the roll prompt | C opens BUILDINGS / MORTGAGE / TRADE / DEEDS / GENERAL, "A TO SELECT, C TO QUIT" |
| buildings | OPTIONS -> BUILDINGS -> BUY BUILDINGS -> the player -> the group browser, `right` a house, A accepts |
| mortgage | OPTIONS -> MORTGAGE -> the deed browser; the square the browser shows is at `0x86c4` |
| trade | OPTIONS -> TRADE -> the opponent gallery -> the DEAL panel |
| speed, timer, end game | OPTIONS -> GENERAL |

Genesis A is retropad `y`, Genesis B is `b` and Genesis C is `a`; `z` is
not a pad button and the tape refuses it.

### The press cadence

A press is 6 frames down and then a gap before the next one. **A screen
that is still arriving swallows a press and starts arriving again**: the
vault that says a bill cannot be paid is black while it comes in, the state
block does not move while it is black, and a player pressing every 30
frames sat on that black screen for 18,000 frames and 300 presses without
ever seeing the vault. One press with 120 frames after it brought the vault
up at once. `game.py` therefore waits `MENU_GAP` (24 frames) after a press
that moved the game and `WAIT_GAP` (120) after one that did not; that one
rule took the same player from 15 turns to 74.

### The opponent's turn

The opponent's turn needs the human's presses too — every screen of it
waits. Answering each rest at once is not faster than answering it a second
later, and only a slow cadence costs frames. The same turn from one
checkpoint, with the press placed at four different delays after the state
goes still:

| delay | frames the turn took |
|---|---|
| 0 | 600 |
| 60 | 540 |
| 120 | 600 |
| 240 | 720 |

(The delay changes the dice, so the first three are one number with the
dice's own spread on it.) **No press shortens the opponent's turn below
about 540 frames**, and there is no press that skips it.

### Game speed

OPTIONS -> GENERAL -> GAME SPEED offers SLOW and FAST, SLOW selected, and
takes nine presses to change (C, A, `down` x4, A, `down`, A). Ten turns
from the same checkpoint at each setting:

| setting | ten turns | per turn |
|---|---|---|
| SLOW | 7,550 frames | 540 to 1,510, mean 755 |
| FAST | 5,640 frames | 470 to 660, mean 564 |

The dice differ between the two runs, so this is a comparison of means over
ten turns rather than a controlled one; the direction is not in doubt and
the setting is worth its nine presses within three turns.

GENERAL's other entries are TIMER, which only *reports* ("GAME TIME
REMAINING / NO TIME LIMIT SET") and cannot set a limit once the game has
started, COMPUTERIZE, TURN ORDER and END GAME.

### Trading

The DEAL panel puts both players' cash at the top and PROPERTY / CASH /
ACCEPT under each name, with "PRESS A TO SELECT ACTION" and "PRESS C TO
REFUSE TRADE"; A opens a picker showing both boards with the owned deeds
labelled, "A TO SELECT OR DESELECT, C TO RETURN TO TRADE SCREEN", and a
selected deed appears on the pedestal at the bottom of the panel.

**Whether the computer accepts, and on what terms, is not established.** A
deed was put on the pedestal and the panel's ACCEPT pressed at, and nothing
moved in the deed table or the purses. There is no RAM word behind the
offer that this reconnaissance could find: a full 64 KB differential across
each press on the panel moves only the RNG and three bytes near `0x81be`
that look like sprite positions. A plan that wants to trade will have to
drive that screen by picture.

### A lap of the board

From the game `game.py` played, counting a lap as a crossing of GO and not
a trip to jail:

| player | laps seen | frames a lap |
|---|---|---|
| the human | 7 | 11,338, 9,072, 4,350, 13,831, 9,753, 6,902, 8,610 |
| the opponent | 6 | 6,824, 14,352, 10,711, 10,224, 9,493, 9,235 |

Both players lap in the same window of frames because the turns alternate,
so **one lap of the whole table is about 9,000 frames**, and a full game is
tens of laps.

## A game at rate 0

`game.py` plays the tape in, checkpoints the first roll and then presses at
every rest: Genesis A, because A at the buy prompt buys what the player
landed on; Genesis C when A has stopped moving the game and the options
panel is on screen; and a longer wait after any press that changed nothing.
It builds when it holds a colour group whole. It stops on `players_left`
reaching 1, on its frame budget, or when 40 presses in a row change
nothing.

The run recorded in `_runs/2026-09-14T16-04-17Z-monopoly-game`:

| what | number |
|---|---|
| frames, power on to the vault | 78,692 |
| turns played | 74, 37 each |
| a turn, the human | 370 to 2,314 frames, mean 871 |
| a turn, the opponent | 365 to 2,316 frames, mean 966 |
| houses bought | none: a blind buyer never completed a colour group |
| purses at the end | $141 and $23 |
| deeds at the end | 7 and 19 |
| won | no; it stopped on "YOU CAN'T PAY YOUR BILL" |

The purses over the run, sampled every thirtieth recorded change:

| frame | the human | the opponent | deeds each |
|---|---|---|---|
| 3,833 | 1500 | 1360 | 0, 1 |
| 9,512 | 900 | 710 | 3, 4 |
| 16,523 | 407 | 788 | 6, 5 |
| 22,386 | 384 | 256 | 6, 7 |
| 30,790 | 604 | 31 | 7, 9 |
| 38,606 | 655 | 167 | 7, 10 |
| 47,581 | 452 | 337 | 6, 14 |
| 55,437 | 460 | 49 | 6, 15 |
| 64,187 | 149 | 300 | 7, 18 |
| 72,548 | 141 | 23 | 7, 19 |

**Neither purse trends to zero, and both hover just above it.** The
opponent was down to $31 at frame 30,790 and to $23 at 72,548 and survived
both, because rent without houses is small and she mortgages to pay. The
same player with building switched off and no rule for the menus stopped at
frame 13,143 after 15 turns; the earlier variant that polled the state
every ten frames instead of every frame reached turn 51 at frame 49,571.
None of them won.

So the estimate a plan should carry is not "a few more turns of this". At
about 900 frames a turn and no drift in the purses, **a game decided by
bankruptcy is decided by houses**: a $28 rent has to become a $700 one, and
that needs a colour group held whole, which this player reached once in
five runs. A plan that buys deliberately, trades or builds towards one
group, and answers the vault with A can expect the deciding turns to cost
the same 900 frames each — the length of the game is a question of how many
turns it takes to build a monopoly out, not of how fast a turn is.

## Determinism

From the checkpoint at the first roll, a six-press burst run straight, then
again after `restore`, then a third time, ends on the same exact hash
`963ee9bd6a327a4b`, the same dice (4, 1), the same square and the same
purse every time; only the run's own frame counter differs (3,821, 4,181,
4,541), because a restore folds the tape back but the frame count goes on.
The restore probe agrees: every run in this reconnaissance reports
`probed 1 checkpoints, 0 parted`, including the 49,571-frame game.

A one-frame offset on the roll press changes the dice every time — the
table under "The RNG, and what advances it" is eight offsets and eight
different rolls. Determinism and that table together are what make a search
over press frames worth running: the same press frame always gives the same
roll, and a neighbouring frame gives another.

## Facts a plan rests on

- The dice are chosen by the frame Genesis A goes down, not by the pad's
  history: `0x84b8` shifts once a frame on its own (x -> 2x with a feedback
  bit), the dice at `0x867e`/`0x867f` are two consecutive samples of it,
  and eight consecutive press frames from one checkpoint give eight
  different rolls. Every total from 2 to 12 is inside a 36-frame window.
- A held button never repeats: the dice are latched at the press, a
  1,800-frame hold of A never resolves a turn, and every menu costs one
  press a step.
- The game is won only by bankruptcy. `players_left` at `0x84dd` goes 2 ->
  1, the loser's seat byte goes to 0, "AMANDA IS BANKRUPT!" (perceptual
  `497072db0b360dcd`) comes 548 frames after the press and the winner's
  plate (`47b938c18c1e71dc`) 227 frames after that.
- SHORT GAME's clock ends a game without a winner: a five-minute game stops
  20,278 frames after the start press, into black and then the attract
  demo, with the board reset. It is not a shortcut to a win.
- The fewest opponents the game allows is one: NUMBER OF PLAYERS opens on 2
  and will not go lower.
- The tape reaches the human's first roll in 2,621 frames from power on and
  replays to frame 3,221, exact `4f6f34c4f3c1a48c`, with $1500 in the purse
  and `current_player` 0.
- The state a plan reads every frame: square, seat, cash and doubles on a
  28-byte stride from `0x84de`; the deed table, 4 bytes a square from
  `0x85dc`, with mortgaged, houses (5 is a hotel) and owner (255 the bank);
  `current_player` at `0x85dd`; `players_left` at `0x84dd`.
- There is no turn-phase word. "The game is waiting for a press" is
  `0x84dc..0x86ff` unchanged for 30 frames — `run.memory_stable` reads it —
  and it holds for every screen except one that is still arriving, where
  the block is still and a press is swallowed. A press that changes nothing
  must be followed by 120 frames, not 24.
- A turn costs about 900 frames for either player with every rest answered
  (mean 871 for the human and 966 for the opponent over 74 turns), and no
  press makes the opponent's turn shorter than about 540 frames. OPTIONS ->
  GENERAL -> GAME SPEED -> FAST costs nine presses and takes ten turns from
  7,550 frames to 5,640.
- One lap of the board is about 9,000 frames a player (4,350 to 13,831 over
  thirteen laps); the whole table laps in the same window.
- Buying everything landed on is not a way to win: over 74 turns both
  purses hover between $23 and $800 and neither trends to zero, and the
  opponent survived $23 twice by mortgaging. Houses are what decides a game
  — four on the brown group cost $200 and 510 frames through OPTIONS ->
  BUILDINGS.
- A player that presses only Genesis A wanders into the options menu and
  stops the game after 15 turns. C is not a safe default either: at the
  roll prompt it opens the options and at the vault it declares bankruptcy.
  What works is C only when a confirm has already failed and the options
  panel is up — the panel fills the middle of the screen with one grey,
  about 10,000 pixels of 12,288 against the board's 329.
- Restores are exact: the same burst from one checkpoint gives the same
  hash three times, and the restore probe reports 0 parted on every run.

## The dice called in advance

`dice.py` is the register at `0x84b8` reproduced from the cartridge, and
`rolls.py` is the scenario that holds it against the machine. Both seats'
rolls can be read off a single register read taken minutes earlier; the
pad chooses which roll arrives.

### The register, out of the ROM

One routine clocks it, at ROM `0x119ba`, and it is the whole generator:

    0119ba  move.w  $ff84b8.l, d0
    0119c0  asl.w   #$5, d0
    0119c2  move.w  $ff84b8.l, d1
    0119c8  eor.w   d1, d0
    0119ca  asl.w   #$1, d0
    0119cc  roxl.w  $ff84b8.l
    0119d2  move.w  $ff84b8.l, d0

`asl.w #5` then `eor` puts bit 10 xor bit 15 into bit 15 of d0; `asl.w #1`
pushes that into X, and `roxl.w` on memory shifts the word left and brings
X in at the bottom. So the step is

    s -> ((s << 1) | (bit10(s) ^ bit15(s))) & 0xffff

which is `dice.advance`. The seed is `0x1234`, written at `0x112d2` beside
the instruction that sets the frame counter `$ff81a0` to `0x1234` — the
counter and the register start together. The orbit is **16383** clocks
long and never reaches 0.

The whole ROM holds five references to `$ff84b8`: `0x112d4`, the seed, and
four inside `0x119ba`. **Nothing can read the register without clocking
it**, which turns "no extra clocks" into a proof that a screen did not
consult the generator, and several findings below rest on it.

### What clocks it

Exactly one site, in the vblank line:

    002922  addq.l  #$1, $ff81a0.l
    002928  jsr     $119ba.l

so a clock and a bump of the longword at `$ff81a0` are the same event.
Frames are *not* clocks — the line does not run its game half on every
frame, and the register pauses. The counter is the clock:

    clocks since a read = counter_of(now) - counter_of(at the read)

`dice.counter_of` reads it out of byte-swapped work RAM. Beyond the
counter, the only extra clocks in a whole game are the draws a roll makes:
two for a human roll, four for an opponent's turn (two pairs, 66 clocks
apart). Over the 20-roll run the model was still exact after 11,593
clocks — the word it carried equalled the machine's.

### The sample map

`rnd(n)` at ROM `0x119da` is one clock and a remainder:

    0119da  jsr     $119ba(pc)      the clock above, result in d0
    0119de  andi.l  #$ffff, d0
    0119e4  move.w  $6(a7), d1      n, from the stack
    0119e8  addq.w  #$1, d1
    0119ea  divu.w  d1, d0          remainder in the high word
    0119ee  lsr.l   #16, d0

so a sample is the clocked word modulo `n + 1`, which is `dice.draw`. A
die is `rnd(5) + 1`, twice, at two sites that are the same code —
`0x10f46` and `0x10fbc` — each guarded by `cmpi.b #$7, $ff867f`, the game
rolling only when the dice read 7:

| rom | writes | the watch |
|---|---|---|
| `0x10f4a` | `$ff867e` | `die_2` (region `0x867f`), drawn **first** |
| `0x10f5c` | `$ff867f` | `die_1` (region `0x867e`), drawn second |

That reversal is why `roll_at` reads `die_2` off the earlier clock, and
why the reconnaissance saw the second die of press k+1 equal the first die
of press k.

### 36 press frames, 36 rolls called from one read

`roll_at(register, offset)` answers the pair for a press `offset` frames
after a state whose register reads `register`: `offset + 65` clocks to the
first draw. From one checkpoint, 36 consecutive press frames, every roll
called from the **single** read at offset 0 (register 31524):

| offset | register | die_1, die_2 | total | called |
|---|---|---|---|---|
| 0 | 31524 | 2, 1 | 3 | 2, 1 |
| 1 | 63048 | 3, 2 | 5 | 3, 2 |
| 2 | 60560 | 6, 3 | 9 | 6, 3 |
| 3 | 55584 | 5, 6 | 11 | 5, 6 |
| 4 | 45633 | 4, 5 | 9 | 4, 5 |
| 5 | 25731 | 3, 4 | 7 | 3, 4 |
| 6 | 51463 | 5, 3 | 8 | 5, 3 |
| 7 | 37391 | 5, 5 | 10 | 5, 5 |
| 8 | 9247 | 3, 5 | 8 | 3, 5 |
| 9 | 18495 | 1, 3 | 4 | 1, 3 |
| 10 | 36990 | 4, 1 | 5 | 4, 1 |
| 11 | 8445 | 2, 4 | 6 | 2, 4 |
| 12 | 16890 | 6, 2 | 8 | 6, 2 |
| 13 | 33780 | 6, 6 | 12 | 6, 6 |
| 14 | 2025 | 2, 6 | 8 | 2, 6 |
| 15 | 4051 | 4, 2 | 6 | 4, 2 |
| 16 | 8103 | 4, 4 | 8 | 4, 4 |
| 17 | 16207 | 1, 4 | 5 | 1, 4 |
| 18 | 32415 | 4, 1 | 5 | 4, 1 |
| 19 | 64831 | 1, 4 | 5 | 1, 4 |
| 20 | 64126 | 4, 1 | 5 | 4, 1 |
| 21 | 62717 | 2, 4 | 6 | 2, 4 |
| 22 | 59898 | 4, 2 | 6 | 4, 2 |
| 23 | 54261 | 2, 4 | 6 | 2, 4 |
| 24 | 42987 | 4, 2 | 6 | 4, 2 |
| 25 | 20438 | 2, 4 | 6 | 2, 4 |
| 26 | 40877 | 5, 2 | 7 | 5, 2 |
| 27 | 16218 | 5, 5 | 10 | 5, 5 |
| 28 | 32437 | 6, 5 | 11 | 6, 5 |
| 29 | 64875 | 1, 6 | 7 | 1, 6 |
| 30 | 64214 | 4, 1 | 5 | 4, 1 |
| 31 | 62893 | 3, 4 | 7 | 3, 4 |
| 32 | 60250 | 1, 3 | 4 | 1, 3 |
| 33 | 54965 | 2, 1 | 3 | 2, 1 |
| 34 | 44394 | 5, 2 | 7 | 5, 2 |
| 35 | 23252 | 4, 5 | 9 | 4, 5 |

36 of 36. Hold length does not enter into it: holds of 1, 2, 12 and 40
frames from the same frame give the same pair.

### The opponent's roll

The opponent samples on the game's own timeline, and the sharp anchor is
its dice reset, not the turn switch:

| | anchor | clocks to `die_2` | frames of warning |
|---|---|---|---|
| human | the press | 66 | 65 |
| opponent | the dice going back to (7, 7) | 128 | 125 |

Against the turn switch the same sample jitters between 137 and 138
clocks, so a plan should arm on the reset. The opponent's turn *length*
shifts everything after it, but only through the counter and its own four
draws — its buys, rents and cards add no clocks at all, so a caller that
tracks the counter stays exact across turns of any length (the run below
includes a doubles re-roll).

Presses made *inside* the opponent's turn change nothing. What chooses its
roll is the last human press the game answers before it: idling that press
by k frames, k = 0..47, gives

| idle frames | die_1, die_2 | total | clock of the sample |
|---|---|---|---|
| 0 | 1, 3 | 4 | 674 |
| 1..7 | 2, 3 | 5 | 677 |
| 8..32 | 1, 1 | 2 | 671 |
| 33..47 | 3, 2 | 5 | 665 |

Every one of the 48 was called in advance from the one read: 48 of 48. The
map is a step, not a slope — the game absorbs a late press until its next
rest, so 48 idle frames select three distinct rolls here rather than 48.
The choice is real and it is cheap (totals 4, 5 and 2 within 48 frames of
waiting), but a plan that wants a particular opponent roll must sweep the
idle range and read the steps off it rather than solve for k.

### Twenty rolls, both seats, one read

`rolls.py` reads the register once at the first roll prompt and then only
presses: every roll below was printed before the machine drew it.

| frame | seat | dice | clock | called at frame | frames ahead |
|---|---|---|---|---|---|
| 95 | human | 4, 1 | 96 | 30 | 65 |
| 726 | opponent | 2, 3 | 677 | 601 | 125 |
| 1290 | human | 1, 6 | 1187 | 1225 | 65 |
| 1980 | opponent | 5, 6 | 1824 | 1855 | 125 |
| 2704 | human | 4, 1 | 2494 | 2639 | 65 |
| 3287 | opponent | 2, 4 | 3020 | 3162 | 125 |
| 4040 | human | 5, 3 | 3715 | 3975 | 65 |
| 4749 | opponent | 1, 1 | 4375 | 4624 | 125 |
| 5268 | opponent | 4, 1 | 4844 | 5143 | 125 |
| 5827 | human | 4, 5 | 5349 | 5762 | 65 |
| 6563 | opponent | 4, 1 | 6033 | 6438 | 125 |
| 7221 | human | 3, 2 | 6637 | 7156 | 65 |
| 7844 | opponent | 5, 3 | 7208 | 7719 | 125 |
| 8495 | human | 2, 4 | 7804 | 8430 | 65 |
| 9106 | opponent | 6, 5 | 8365 | 8981 | 125 |
| 9828 | human | 1, 6 | 9033 | 9763 | 65 |
| 10435 | opponent | 4, 5 | 9587 | 10310 | 125 |
| 11259 | human | 5, 2 | 10353 | 11194 | 65 |
| 11930 | opponent | 4, 2 | 10972 | 11805 | 125 |
| 12606 | human | 4, 1 | 11592 | 12541 | 65 |

20 of 20, called 65 to 125 frames ahead, with the model still holding the
machine's register at the end.

### Everything else the register decides

| what | read site | the map |
|---|---|---|
| turn order | `0x1af8` | `rnd(players_left - 1)` into `$ff85dc`, the seat |
| the Chance deck | `0xcefe` | 16 x `rnd(15)`, no repeats, to `$ff86c8` |
| the Chest deck | `0xcf60` | the same to `$ff86da`; cursors clear at `0xcfa4` |
| drawing a card | `0xe800`, `0xe76e` | deck at the cursor, cursor + 1 |
| buy, build, mortgage | none | no clocks across 13 audited turns |

The cards are decided once, when the decks are shuffled at setup, and
never again at the card screen: four press timings that all totalled 7 and
landed on the same Chance square drew the identical card (back to square
4, -$150), and the counter showed zero extra clocks while the card was up.
The opponent's decisions are not a gamble either — nothing it does between
rolls touches the generator. Boot and setup spend 114 draws in all, the
two shuffles and the turn-order roll among them.

That is the whole of this game's randomness: a 16-bit register the frame
clocks, read at a press for the dice and at setup for the decks and the
seat order.

## Every prompt, answered from the state (v7 step 2)

`engine.py` has one function a prompt: it reads the ram to know the prompt
is up and presses what answers it. Each row was measured from a named
checkpoint, `_runs/_checkpoints/0066202883d8a59a/`, with the pad released
after every restore and every press placed on a settled state block.

| prompt | presses | frames | it is up when | it is answered when |
|---|---|---|---|---|
| roll | 1 | 66 | `watch current_player equal 0`, `watch die_1 equal 7`, panel < 3,000 | the block settles again |
| buy the deed | 1 | 66 | the square is in `state.BUYABLE`, owner 255, panel 3,000..9,000 | the deed's owner is the seat, cash fell by the price |
| decline, into the auction | 1 | 69 | the same | `$ff8862` (the asking price) is above zero |
| the auction, bidding to a ceiling of $400 | 30 | 2,149 | `$ff8862` above zero | the deed's owner is the seat; won at $145 on a $100 deed |
| the auction, no press at all | 0 | 768 | the same | the opponent takes it at $14 |
| one house on a monopoly | 7 | 462 | panel >= 9,000 after OPTIONS | `houses_on` rose, cash fell $50 |
| four houses | 10 | 660 | the same | cash fell $200 |
| eight houses | 14 | 924 | the same | cash fell $400 |
| two hotels, from four houses each | 9 | 612 | the same | houses read 5, cash fell $100 |
| mortgage one deed | 10 | 660 | the same | the deed's mortgage flag set, cash rose $50 |
| un-mortgage it | 9 | 594 | the same | the flag clear, cash fell $55 |
| rent paid | 1 | 66 | panel 3,000..9,000 and the square's owner is the other seat | cash fell by `board.rent` |
| rent received | 1 | inside the opponent's turn | the other seat's square is one of ours | cash rose by `board.rent` |
| a card drawn | 1 | 69 | the square is one of the six card squares and the panel is a card | the deck cursor advanced by one |
| jail, serving the three turns | 38 | 2,063 | record byte +16 above zero | record +16 zero and cash fell the $50 the game takes itself |
| jail, rolling a double | not forced | — | the same | record +16 zero without the $50 |
| jail, the pardon card | never drawn | — | OPTIONS gains GET OUT OF JAIL and a holder byte names the seat | the holder byte is 0xff again |
| the trade screen, nine offers | 4 to 29 | 294 to 1,944 | panel about 1,400, then 6,400 | nothing ever moved |
| the opponent's whole turn, tapped | 14 | 698 | `not watch current_player equal 0` | `watch current_player equal 0` |
| the opponent's whole turn, one held confirm | 1 hold | 952 | the same | the same |

### Holding, measured before any tap-and-wait loop

- **A held direction does not repeat.** 240 frames of `right` held on the
  deed browser moves it exactly one step, so every browser walk is taps.
- **A held confirm does carry the opponent's turn**, on its own, with no
  further press: 952 frames from the `brown` checkpoint against 698 for
  fourteen placed taps. Holding costs 254 frames more, and it is the only
  way to answer the turn in one call.
- **A held confirm also freezes the opponent's dice.** From `brown`, 96
  held sweeps at one-frame offsets and 48 tapped sweeps all landed the
  opponent on square 7: the opponent's roll is latched when its turn opens
  and no press of ours moves it. Step 1's "step function of idle frames"
  is the delay *before* the turn opens, not inside it.

### The options tree, and the cursor behind it

C opens WHOM AM I TALKING TO (`+` on PLAYER 1 or AMANDA, the cursor at
region `0x86c4`), A takes the seat, and the list is BUILDINGS, MORTGAGE,
TRADE, DEEDS, GENERAL — the same five strings the ROM holds at `0x12f66`,
`0x12f70`, `0x12f79`, `0x12f80`, `0x12f86`, with GET OUT OF JAIL
(`0x12f8e`) added while a pardon is held. The same byte `0x86c4` is the
cursor everywhere: 0..4 in the list, 0..3 in BUILDINGS (BUY HOUSES, BUY
HOTELS, SELL HOUSES, SELL HOTELS), 0..1 in MORTGAGE (MORTGAGE,
UN-MORTGAGE), the board square in the deed browser, and the bidder in an
auction.

**The deed table's mortgage flag was mis-mapped.** Mortgaging Vermont
(square 8) moves region byte `0x8601`, which is `0x85e1 + 4 * square`; the
houses byte is `0x85de + 4 * square` and the owner `0x85df + 4 * square`.
`state.py` now reads the record from `0x85de` with houses at +0, owner at
+1 and the mortgage flag at +3.

### The auction, in ram

Four words at region `0x8860` hold it, and they are all zero when no
auction is running: the square, the asking price, the standing bid, and
the seat that made it (`0xffff` for nobody). The bidder cursor is the
usual `0x86c4`; `up` puts it on our seat and A bids the asking price, two
presses a bid. Left alone the auction closes itself in 768 frames and
hands the deed to the opponent for $14; bidding to a ceiling of $400 won
Oriental Avenue for $145 in 2,149 frames.

### The cards, known before the square

Two decks of sixteen ids, dealt at setup, are read straight out of work
ram — byte-swapped, so id *i* sits at region index *i xor 1*:

| deck | ids | 68000 deck | cursor | holder of the pardon |
|---|---|---|---|---|
| Chance | 16..31 | `$ff86c8` | `$ff86d8` | `$ff86ec` |
| Community Chest | 0..15 | `$ff86da` | `$ff86ea` | `$ff86eb` |

The drawn id is written to `$ff86ee`; the ROM's draw routine at `0xe800`
reads the cursor, indexes the deck, adds `#$10` for Chance and wraps the
cursor at 15. `engine.next_card(run, "chance")` answers the id the next
draw will turn up. Checked: from `brown-roll` the Chance cursor stood at
3 and the deck's fourth id was 17; the token was driven onto square 7 and
`$ff86ee` read 17, the cursor 4.

Every card's text is inline code behind a jump table — Community Chest at
`0xcfe0`, Chance at `0xd530`, each sixteen word offsets from its own base:

| id | Community Chest | id | Chance |
|---|---|---|---|
| 0 | GET OUT OF JAIL, FREE | 16 | GET OUT OF JAIL FREE |
| 1 | PAY HOSPITAL $100 | 17 | TAKE A RIDE ON THE READING |
| 2 | STREET REPAIRS $40/$115 | 18 | MAKE GENERAL REPAIRS $25/$100 |
| 3 | BEAUTY CONTEST, COLLECT $10 | 19 | ADVANCE TO GO |
| 4 | RECEIVE FOR SERVICES $25 | 20 | GO DIRECTLY TO JAIL |
| 5 | YOU INHERIT $100 | 21 | ADVANCE TO ST. CHARLES PLACE |
| 6 | LIFE INSURANCE MATURES $100 | 22 | BUILDING AND LOAN, COLLECT $150 |
| 7 | FROM SALE OF STOCK $45 | 23 | BANK DIVIDEND OF $50 |
| 8 | XMAS FUND MATURES $100 | 24 | ADVANCE TO ILLINOIS AVE. |
| 9 | INCOME TAX REFUND $20 | 25 | GO BACK 3 SPACES |
| 10 | GRAND OPERA, COLLECT $50 EACH | 26 | TAKE A WALK ON THE BOARD WALK |
| 11 | BANK ERROR, COLLECT $200 | 27 | ADVANCE TO NEAREST UTILITY |
| 12 | ADVANCE TO GO | 28 | POOR TAX OF $15 |
| 13 | GO TO JAIL | 29 | CHAIRMAN OF THE BOARD, PAY $50 EACH |
| 14 | SCHOOL TAX $150 | 30 | ADVANCE TO NEAREST RAILROAD |
| 15 | DOCTOR'S FEE $50 | 31 | ADVANCE TO NEAREST RAILROAD |

### Jail

The jail counter is byte +16 of the player record: 3 the moment the token
arrives, 0 when it is free. Every button rolls at the jail prompt — A, B,
C and start all produce the same roll, and C does not open OPTIONS there.

- **Serving the three turns** costs 38 presses and 2,063 frames from the
  `jail-prompt` checkpoint, and on the third the game takes the $50 itself
  (cash 1,110 to 1,060) and leaves the token on square 10.
- **Rolling a double could not be forced from that checkpoint.** 150
  press frames one apart, and another 90 with two presses each, produced
  two outcomes only — (4,1) and the idle (7,7). The jail roll, like the
  opponent's, is latched before the prompt, so the press frame does not
  choose it and the dice model of "36 press frames, 36 rolls" does not
  apply in jail. A plan that wants the double has to vary the state before
  the turn opens.
- **The pardon card was never drawn.** The ROM adds GET OUT OF JAIL to
  OPTIONS while a holder byte names a seat. Across 80 turns driven at the
  card squares and another 400 plain presses the Chance cursor moved from
  3 to 5 and the Chest cursor not at all; the two pardons sit at Chance
  index 10 and Chest index 9 of the decks this tape deals, so neither came
  up.

### The trade screen: nine offers, nothing moved

Nine offers were put from the `deal-panel` checkpoint — none, one and two
deeds on the pedestal, crossed with no cash, four raises and twenty — and
after every one the forty owner bytes and both purses were unchanged. No
ram word moves with the DEAL panel's own cursor either: the six cells
(PROPERTY, CASH, ACCEPT on each side) leave `0x86c0..0x86cb` alone, unlike
every other list in the game. **The computer accepted nothing, and there
is no word behind the answer.** Step 3 should not plan on trading.

### The ROM's money tables

`board.py` reads both tables through `run.memory("cartridge", ...)`; the
cartridge region holds the rom file as it lies, so the words are big-endian
and carry no work-ram byte swap.

| table | offset | shape |
|---|---|---|
| the deed rows | `0x14ec6` | 20 bytes a board square, 40 squares |
| rents | row + 0, 2, 4, 6, 8, 10 | bare, one to four houses, hotel |
| mortgage value | row + 12 | word |
| house price | row + 14 | word |
| price | row + 16 | word |
| the group of each square | `0x14c92` | one byte a square: 0..7 colours, 8 railroad, 9 utility, 10 nothing |

Found by looking for Mediterranean's known $60 price: the word 60 at
`0x14ef6` sits 16 bytes into a row whose first words are 2, 10, 30, 90,
160, 250 — the board's brown rents. The rules on top of the table are the
68000's: `0x6636` doubles a bare rent when the owner holds the group
whole, reading the per-group word at `$ff87be`; `0x66ec` charges a
railroad 25, 50, 100 or 200 by how many its owner holds; `0x6728` charges
a utility four times the dice, ten times with both; and `0x67e6` doubles
whatever came out when `$ff86f1` is set, which is what the "pay twice the
rental" Chance card sets.

Six checks against the emulator:

| check | board.py | the game |
|---|---|---|
| Oriental Avenue bought | price 100 | cash 1,500 to 1,400 |
| a house on brown | 50 | cash 1,032 to 982 |
| eight houses on brown | 400 | cash 1,032 to 632 |
| St. Charles Place, bare, no monopoly | rent 10 | cash fell 10 |
| Tennessee Avenue, bare, no monopoly | rent 14 | cash fell 14 |
| Indiana Avenue, bare, no monopoly | rent 18 | cash fell 18 |

Vermont Avenue's mortgage reads 50 and the game paid 50 for it; the
un-mortgage cost 55, the ten per cent the rules add and the table does not
hold.

### Twenty turns, every prompt answered from state

`turns.py` plays the tape, then twenty turns in which `Turns.prompt()`
names the screen from the ram alone and `Turns.answer()` presses for it.
`_runs/2026-09-14T18-04-40Z-monopoly`, 17,502 frames, 1 checkpoint probed,
0 parted:

| turn | seat | frames | cash, seat 0 / seat 1 | prompts raised |
|---|---|---|---|---|
| 0 | 0 | 843 | 1,350 / 1,360 | roll, then six notices |
| 1 | 1 | 529 | 1,350 / 1,180 | four opponent screens |
| 2 | 0 | 1,321 | 970 / 1,180 | roll, notices, deed, roll, notices, deed |
| 3 | 1 | 523 | 970 / 960 | three opponent screens |
| 4 | 0 | 677 | 690 / 960 | roll, notices, deed, notice |
| 5 | 1 | 492 | 690 / 810 | three opponent screens |
| 6 | 0 | 763 | 615 / 810 | roll and five notices |
| 7 | 1 | 1,126 | 615 / 860 | eight opponent screens |
| 8 | 0 | 658 | 800 / 860 | five notices |
| 9 | 1 | 651 | 800 / 1,000 | five opponent screens |
| 10 | 0 | 1,317 | 620 / 1,000 | roll, notices, roll, notices, deed |
| 11 | 1 | 491 | 620 / 900 | three opponent screens |
| 12 | 0 | 631 | 620 / 900 | roll, notices, then jail |
| 13 | 1 | 558 | 620 / 700 | four opponent screens |
| 14 | 0 | 446 | 620 / 700 | five jail prompts |
| 15 | 1 | 488 | 620 / 700 | three opponent screens |
| 16 | 0 | 446 | 620 / 700 | five jail prompts |
| 17 | 1 | 445 | 620 / 700 | three opponent screens |
| 18 | 0 | 789 | 556 / 714 | jail, then the $50 and four notices |
| 19 | 1 | 547 | 556 / 394 | four opponent screens |

Seat 0: ten turns, 7,891 frames, 446 to 1,321, mean 789. Seat 1: ten
turns, 5,850 frames, 445 to 1,126, mean 585. No press was swallowed —
every turn ended because the seat changed, not because the answer budget
ran out — and both seats were still in the game at the end.

Turn 16 is the rent-received check from the other side: seat 0 fell 440 to
422 on Indiana Avenue and seat 1 rose 640 to 658, the $18 `board.rent`
says.

### FAST on the tape

The setup order allows it, so the existing tape grew two segments rather
than a second tape: `game-speed-fast` walks C, A, four downs to GENERAL,
A, one down to GAME SPEED, A, one down to FAST, A, and two Cs back out,
and `fast` refuses to end on a slow game by waiting on the delay word the
menu writes (`watch game_speed equal 9`; SLOW writes 14, and the two
stores are ROM `0x96e0` and `0x96fa`). The tape is 12 segments and 3,521
frames, 76 transitions, 0 retries, and two plays from power on end on the
same frame — sha256 `2914f8ee99ee88bd000a1aaa10d9aa19b0be4c145d6758f9d5d9a5525225e26e`
on both shots.

    tape    title-to-first-roll (12 segments)
    core    genesis_plus_gx
    profile examples/monopoly/profile.yaml
      title            none                                     timeout 60     2 transitions
      players          perceptual_hash 7e7280d589c1ccae within 4 timeout 900    2 transitions
      player-1-is      perceptual_hash 7e5680d0c9c5ce2e within 2 timeout 600    2 transitions
      name             perceptual_hash 7e70c0d48d85ccae within 2 timeout 600    10 transitions
      token            perceptual_hash 7e76c0d18c85c8ae within 2 timeout 600    2 transitions
      player-2-is      perceptual_hash 7e5680d089c5ceae within 2 timeout 600    4 transitions
      opponent         perceptual_hash 4402ab7cfdd9d80c within 4 timeout 600    2 transitions
      ready            perceptual_hash 7e76c0d18d84cc8e within 2 timeout 600    2 transitions
      opponent-first-turn watch current_player equal 1             timeout 1800   24 transitions
      first-roll       watch current_player equal 0             timeout 1800   0 transitions
      game-speed-fast  watch current_player equal 0             timeout 600    26 transitions
      fast             watch game_speed equal 9                 timeout 600    0 transitions

## The opponent's roll, settled (v7 step 3)

Step 1 read the opponent's roll as chosen by the idle before the last
human press, in three coarse steps over 48 frames; step 2's revision read
it as latched when the opponent's turn opens. `sweep.py` settles it at
three checkpoints taken at our own roll prompt (`prompt-0`, `prompt-1`,
`prompt-2`, all on disk), sweeping the idle 0..200 one frame apart and
reading the register and the counter at the press each time —
`_runs/2026-09-14T18-32-23Z-monopoly-sweep`, 574,214 frames, 603 turn
replays.

**Both were measuring the same thing from different seats.** The roll is
chosen by the idle before *the last press of our turn that the game
answers* — which is not the last press we make. The sweep finds it by
scanning backwards for the press whose idle moves the dice:

| checkpoint | presses in our turn | the answering press | idle 0 | idle 5 |
|---|---|---|---|---|
| prompt-0 | 11 | 8 (the deed) | (5,6) | (4,2) |
| prompt-1 | 9 | 8 | (6,3) | (4,4) |
| prompt-2 | 23 | 20 | (2,4) | (4,4) |

Idling before a later press changes nothing: the game has already drawn.
That is step 2's measurement — its checkpoint sat after the latch. Step
1's three-rolls-in-48-frames was measured at a press the game absorbs, so
only the frames where its own screens happened to land showed a change.

### It steps every frame, not in blocks

| checkpoint | distinct rolls / idle frames | totals reached | called from the register |
|---|---|---|---|
| prompt-0 | 194 / 201 | 2..12, all eleven | offset 163, 201 of 201 |
| prompt-1 | 193 / 201 | 2..12, all eleven | offset 81, 201 of 201 |
| prompt-2 | 193 / 201 | 2..12, all eleven | offset 163, 201 of 201 |

The seven or eight repeats are the frames where the counter at `0x81a0`
skips a clock (`idle 181..182` at prompt-2 is one: two frames, one clock,
one roll). Every one of the 603 rolls was called in advance from the
register read at the press: `dice.roll_at(register, D)` matched 201 of
201 at each checkpoint, with D constant within a turn and different
between turns (163, 81, 163). D is the clocks between the press and the
opponent's draw, so it moves with what the screen does after the press; it
is measured once per turn, not assumed.

The first fourteen idle frames at each checkpoint, which is all a player
needs — every total is inside them:

| idle | prompt-0 | prompt-1 | prompt-2 |
|---|---|---|---|
| 0 | (5,6) 11 | (6,3) 9 | (2,4) 6 |
| 1 | (5,5) 10 | (2,6) 8 | (6,2) 8 |
| 2 | (4,5) 9 | (4,2) 6 | (5,6) 11 |
| 3 | (1,4) 5 | (1,4) 5 | (5,5) 10 |
| 4 | (2,1) 3 | (4,1) 5 | (4,5) 9 |
| 5 | (4,2) 6 | (4,4) 8 | (4,4) 8 |
| 6 | (3,4) 7 | (1,4) 5 | (2,4) 6 |
| 7 | (1,3) 4 | (4,1) 5 | (4,2) 6 |
| 8 | (1,1) 2 | (2,4) 6 | (4,4) 8 |
| 9 | (4,1) 5 | (4,2) 6 | (3,4) 7 |
| 10 | (3,4) 7 | (1,4) 5 | (6,3) 9 |
| 11 | (2,3) 5 | (2,1) 3 | (1,6) 7 |
| 12 | (6,2) 8 | (4,2) 6 | (3,1) 4 |
| 13 | (6,6) 12 | (3,4) 7 | (5,3) 8 |

So the opponent's roll is a **choice**, at one-frame resolution, for the
price of up to thirteen idle frames a turn. The search treats it as one
everywhere our turn ends on a press the game answers, and as environment
only when our turn's last answered press is our own roll press (a jail
turn that fails to double), where the two cannot be chosen apart.

## The model (v7 step 3)

`model.py` carries both purses, both squares, jail and doubles counts,
the pardon cards, every deed's owner, houses and mortgage flag, both deck
orders with their cursors, and the register and counter. `Model.turn`
applies a seat's rolls with no emulator: movement, GO, rent from
`board.rent`, taxes, the card the deck's cursor is about to turn up, the
jail rules, the machine's buy rule and the auction floor. `Model.read`
takes the whole thing out of ram, so the player re-reads every turn and
never drifts.

### Checked against twenty turns

`turns.py` now runs the model beside the machine: every turn it applies
the rolls the machine drew and compares purses and squares, then reads
the machine back. `_runs/2026-09-14T18-46-13Z-monopoly-turns`, 17,262
frames — **20 of 20 turns agreed**, cash and square, both seats.

Two mismatches were found and fixed on the way, and both are worth
keeping written down:

| gap | what the model said | what the machine did | the fix |
|---|---|---|---|
| income tax, square 4 | $200 | $150 | the bill is `min(200, worth // 10)`; the sample is one seat worth $1,500 |
| turn 7/8, a roll in the wrong turn | the rival rolled twice | our roll had been drawn by the last press of the rival's turn | a roll belongs to the seat `current_player` names on the frame it appears, so a late roll is handed to the next turn |

The second is a fact about the game, not about the ledger: **the turn
boundary is fuzzy by one press.** The press that acknowledges the last of
the opponent's screens can be the press that rolls our dice, and then our
turn opens with the dice already on the table and no roll prompt at all
(turn 8 of the run raised five notices and no roll). A player that wants
to choose that roll has to choose it a press early.

The model's limits, none of which the twenty turns exercised: the income
tax rule cannot be told from a flat $150 on one sample; trades are not
modelled (the machine never offered one); and a three-house-deep
liquidation order is the model's guess at the machine's, since no seat
came near bankruptcy in the run.

## The line (v7 step 3)

`player.py` is the line: it reads the state every frame it answers, and
it never places a press by hand. Each turn pair it

1. takes a named checkpoint (`turn-N`, on disk beside the bundles),
2. reads the board into `model.Model` and runs `search.Search` — a beam
   of width 40, up to 14 turn pairs deep, over turn pairs of the model,
   our roll and the rival's both choices, ranked by the frames spent
   plus the model's estimate to the fold,
3. turns the beam's first halves into candidates — the top 3 distinct
   first halves, each crossed with the 14 idles the sweep above shows
   cover every total the rival can be sent,
4. plays all 42 in the core with `tash.search`, which restores the
   turn's checkpoint before each one, and keeps the candidate whose
   score is highest, and
5. re-applies the winner and checks the model against what the machine
   did with the rolls the machine actually drew (`gap ...` on the run's
   output, one line per disagreement).

The score is the rival's liquid worth against our own and the frames
spent: `4 * liquid(us) - 20 * liquid(rival) - frames`, a win worth
10^7 and our own bankruptcy -10^9. It is the rival's *liquid* worth —
cash plus what mortgaging and selling houses would raise — because that
is what the game counts before it folds a player.

### What it chose, turn by turn

`_runs/2026-09-14T21-23-19Z-monopoly-line`, 16 turn pairs from power on.
"idle" is the frames held back before the last press of our half, which
is what picks the rival's roll; "build" is the group and the houses
asked for; the last column is the model checked against the machine on
the rolls the machine drew.

| pair | our total | buy | build | idle | our cash | rival cash | rival liquid | trial frames | trials | model |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 3 | True | - | 3 | 1440 | 880 | 1190 | 53315 | 42 | ok |
| 1 | 5 | True | - | 5 | 1340 | 430 | 965 | 53186 | 42 | ok |
| 2 | 5 | True | - | 13 | 1200 | 30 | 765 | 79045 | 42 | ok |
| 3 | 3 | True | - | 3 | 1020 | 60 | 795 | 81937 | 42 | ok |
| 4 | 3 | True | - | 0 | 820 | 87 | 642 | 70875 | 42 | ok |
| 5 | 4 | True | - | 9 | 590 | 77 | 632 | 91805 | 42 | gap |
| 6 | 34 | True | - | 7 | 410 | 57 | 612 | 73399 | 42 | ok |
| 7 | 11 | True | - | 5 | 535 | 32 | 587 | 86417 | 42 | ok |
| 8 | 3 | True | - | 4 | 415 | 87 | 512 | 66873 | 42 | ok |
| 9 | 32 | True | ('brown', 10) | 1 | 415 | 112 | 537 | 126259 | 42 | gap |
| 10 | 9 | True | ('light-blue', 8) | 8 | 481 | 96 | 521 | 128386 | 42 | gap |
| 11 | 4 | True | ('light-blue', 8) | 10 | 495 | 82 | 507 | 144326 | 42 | gap |
| 12 | 9 | True | ('light-blue', 8) | 1 | 515 | 62 | 487 | 136636 | 42 | gap |
| 13 | 10 | False | ('light-blue', 13) | 10 | 740 | 37 | 462 | 147644 | 42 | gap |
| 14 | 3 | True | ('light-blue', 13) | 7 | 540 | 62 | 262 | 157331 | 42 | gap |
| 15 | 10 | True | - | 6 | 452 | 62 | 62 | 71236 | 42 | gap |

The first eight pairs are the same move: take the cheap end of a group
(`deed_gain` prices a deed by the monopoly it brings, not by what it
costs), and spend the idle on the roll that costs the rival most. The
browns complete at pair 9 and the light blues at pair 10, and from
there every pair builds. The browns never take a house — the machine
refuses that build and the purse shows it — but by pair 14 the light
blues carry four each, where a landing is 400, 400 or 450 off the rom's
own table. The rival's liquid worth falls 1500 -> 1190 -> 965 -> 765 ->
... -> 462 -> 262 -> 62, and at pair 15 it lands and cannot pay.

### The size of the search

| what | number |
|---|---|
| turn pairs played | 16 |
| candidates tried in the core | 688 (42 a pair, 3 halves x 14 idles) |
| frames spent trying them | 1,568,670 |
| model nodes expanded | 775,035 in 2,712 expansions |
| frames the whole run took | 1,576,061 in 919.55 s at 1714.0 fps |
| frames the line itself keeps | 35,623 |

The beam is 40 wide and up to 14 pairs deep; it ends early when a child
folds the rival. The emulator never trusts it: every candidate is
played from the turn's own checkpoint and scored on what the machine
says afterwards, and only then is the winner re-applied.

### The frames to the two plates

| plate | frame of the line | frame of the run |
|---|---|---|
| "AMANDA IS BANKRUPT!" | 35,507 | 1,575,945 |
| the winner's plate | 35,623 | 1,576,061 |

116 frames separate them, and the tape cuts a segment at each: the
loser's plate is anchored on exact hash `c52a7882a064f66f` and the end
of the line on `d1c07d95a1d0a9ba`.

    tash tape replay --bundle _runs/2026-09-14T21-23-19Z-monopoly-line \
        --record _runs --name monopoly-line-replay --video-stride 30
    replayed 35623 frames of the 35623 the tape keeps,
    hash d1c07d95a1d0a9ba, watches match

### Where the model still disagrees

Eight of the sixteen pairs printed a `gap`, and every one is one of two
things:

- **A build buys fewer houses than it was asked for.** Pairs 9 to 14
  differ by exactly 400, 500 or 50 — eight, ten or one house at 50 —
  with the squares agreeing. `engine.build` browses the group's deeds
  and confirms; the model charges the whole count the plan asked for.
  The machine put four houses on each light blue and none at all on
  the browns; the model charged for both.
- **A roll that lands on the turn boundary is not seen.** Pairs 5 and
  15 have `made: []` while the machine moved us 19 -> 24 and 8 -> 12.
  That is the press that clears the rival's last screen also rolling
  our dice: the player never sees a roll prompt, so it records no roll
  and the model applies nothing. It costs the check, not the line —
  the search scores the machine's state either way.

Neither is fixed here. They are the model's two known limits.

### Four things that stall a player, and what answers them

- **The auction words outlive the auction.** `AUCTION_ASKING` at
  `0x8862` still reads the last auction's price on a plain card screen,
  so `engine.auction_prompt` alone says "auction" where the game is
  showing FREE PARKING. A live auction fills the panel: the player
  only calls it an auction when `engine.panel` is above 7,000.
- **A menu left open answers a confirm with another menu.** After a
  build the OPTIONS list can still be up; confirm toggles panel 10,035
  against 10,396 for ever, and one or two cancels put the roll prompt
  back. `engine.in_menu` is tested before every other prompt.
- **A deed we cannot pay for is a decline.** The deed prompt does not
  go away for a confirm we cannot afford; the player reads the price
  off the rom table and declines when the purse is short.
- **A named checkpoint taken with no frame after a restore refuses the
  run's tape.** `run.restore` of a checkpoint from disk seeds the line
  with no stretch behind it, so the place a `checkpoint` records right
  after it is harness frame 0, and the next restore of that name is
  judged "taken on a line this run left" — `tape.yaml` is then never
  written, however the run ends. One settle after the restore is
  enough; the player does it in `begin`.
