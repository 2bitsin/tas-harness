# examples/dune/

`profile.yaml` runs *Dune: The Battle for Arrakis* (USA) on Genesis Plus GX
from the cartridge library in `roms/`. The goal this reconnaissance serves is
"complete the first two missions with one house as fast as the game allows",
so the hunt went after credits, the building count and the cursor. **Both
missions are won from power on by `player.py`**, and the sections from "The
engine" down are the steps that built it; everything above them is the
reconnaissance, corrected where a later step found it wrong. The
reconnaissance's surprise was where the first mission's arithmetic lands:
**the quota is 1,000 credits, the opening purse is 990, a windtrap and a
refinery cost 700 between them, and a full harvester load is about 706 — so
the mission as the mentat describes it needs two deliveries, not one.** The
second surprise killed the play: **a building placed on bare sand decays and
explodes**, and the refinery took the banked spice with it at 981 of 1,000.
Everything below is from that play or from the session that found it.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `credits` | `0x4d6a` | 2 | little | the credit figure the game works from |
| `credits_shown` | `0xc05a` | 2 | little | the number drawn top right, which chases `credits` |
| `buildings` | `0xdbee` | 2 | little | the player's standing buildings |
| `cursor_x` / `cursor_y` | `0xbf12` / `0xbf14` | 2 | little | the map cursor, in screen pixels |
| `radar_x` / `radar_y` | `0xbf4c` / `0xbf4e` | 2 | little | the cursor's blip inside the radar box, in screen pixels |
| `panel_row` | `0xbf8c` | 2 | little | the build list's row, 0 on the EXIT / FIX / STOP strip |
| `panel_column` | `0xbf8a` | 2 | little | the build list's column, 0 leftmost |
| `panel_price` | `0xfe8a` | 2 | little | the price the panel draws for the highlighted item |
| `picked_x` / `picked_y` | `0xbf5c` / `0xbf5a` | 2 | little | the selected object's position, tile in the high byte |
| `mission` | `0xc04c` | 2 | little | the mission number, 1 to 9 |
| `house` | `0xc274` | 2 | little | the house the player is, 0 Harkonnen, 1 Atreides, 2 Ordos |
| `harvester_load` | `0x1e0a` | 2 | little | mission 1's harvester's load, `100 << 8` when full |

Little-endian on a 68000 is Genesis Plus GX keeping work RAM byte-swapped:
region offset R is 68000 address R xor 1, so a 68000 word at an even address
reads as a little-endian 16-bit word at the same offset. Nothing here is a
longword; credits never left 16 bits in the first two missions.

## The hunt

### Credits

Mission 1 opens on 990 credits. A `hunt` over `system` at width 2 for the
value 990 (seeded once, then filtered on a second `step`, because the first
`step` only seeds the region) left three addresses: `0x4d6a`, `0xc05a` and
`0xd6c2`. Spending separates them. Buying the windtrap drops `0x4d6a` in one
motion while `0xc05a` and `0xd6c2` walk down behind it in the step the
counter on screen uses, so `0x4d6a` is the figure and the other two are the
display chasing it. All three agree once the game is still.

The drain is not instant: the press that starts a windtrap is followed by 96
frames of nothing, then 990 falls to 690 over the next 360 frames, 50 at a
time every 60 frames.

### The building count

Three checkpoints with known counts — `m1-start` (the construction yard
alone), `m1-ref-ready` (the windtrap up), `m1-refinery-placed` (the refinery
down) — filtered a width-1 hunt from 74 candidates at value 3, to 3 at value
2, to exactly one at value 1: `0xdbee`, holding 1, 2 and 3. It is the
player's count and it fell to 2 the moment the refinery exploded.

No enemy count came out of this. Atreides mission 1 has no enemy buildings,
so the side that would prove a per-side table reads zero throughout and is
indistinguishable from the zeroed words around `0xdbee`. Unit counts are in
the same position: nothing in the session has two states whose unit counts
are both known, so the filter has nothing to bite on. The method above is
what will find both as soon as mission 2, which has an Ordos base, is
reachable.

### Winning and losing

Mission 1 was not won, so the differential is the one the brief allows
instead: a building's destruction. A full 64 KB dump at `m1-alive`
(credits 981, three buildings), a second 300 frames later to drop everything
volatile, and a third after the refinery blew up, leaves 1,034 bytes changed
of 65,162 that had been stable, 168 of them holding a small number on both
sides. `0xdbee` is in that list, 3 to 2. Nothing in it looks like a
mission-won or mission-lost flag, and nothing should: the construction yard
survived, the mission kept running, and the game never drew an end screen.
The mission number was in the same state, and step 2 settled it by
reaching mission 2 to difference against: "Winning, and the mission number"
below.

### The cursor

`0xbf12` and `0xbf14` are the map cursor in screen pixels, the centre of the
24-pixel bracket. Holding `right` for 16 frames moved `0xbf12` from 153 to
177; holding `down` for 16 moved `0xbf14` from 132 to 142 (it was against
nothing on that axis). They clamp at x 0..232 and y 24..200, which is the
play area minus the radar panel.

`0xbf4c` and `0xbf4e` are the same cursor drawn as a blip in the radar box,
and they are the reading that survives the camera scrolling, because screen
pixels stop naming a tile as soon as the map moves under them. **The
reconnaissance's formula was wrong by a factor of two.** A one-frame tap
under a placement ghost moves the cursor exactly one tile and moves the blip
exactly two, on either axis, from every tile tried; the blip's corner is
(240,144), not (240,143); and the blip clamps at x 240..298, y 144..202. So
one tile is two blip pixels and 32 screen pixels, the cursor addresses 30
tiles on each axis, and

    tile = ((radar_x - 240) // 2, (radar_y - 144) // 2)

`0xbf8a` is the build list's **column**, not its row: the row is `0xbf8c`
beside it. The reconnaissance read 0xbf8a as 0 with the panel open and took
that for a failure; 0 is the EXIT tab, which is where the list opens. Both
came out of the same 256-byte window diffed across one press of `down` and
one of `right`, beside two more words that hold the highlight's position in
screen pixels.

## The play

### The menus

Power on runs the SEGA, Virgin and Westwood screens and a planet flight, and
settles on the title menu — DUNE, START GAME / OPTIONS / TUTORIAL — by frame
1,980 with no input at all. The starfield twinkles on a four-frame cycle, so
the exact hash never rests; the perceptual hash sits at `19936665cc6c9993`
or `1933666ccccc9393` depending on the twinkle.

OPTIONS holds MUSIC IS, SOUNDS ARE, RADAR IS, MUSIC TEST, SOUND TEST and
ENTER PASSWORD, and exits on START. **There is no speed setting.** ENTER
PASSWORD is the one door to a later mission that does not go through winning
the one before it; the code table is not plain ASCII in the ROM, so this
reconnaissance could not use it.

START on the title opens the house plates: ATREIDES, ORDOS, HARKONNEN,
perceptual `41cb3eb6c169d0b4`, with **ATREIDES already selected** (its name
plate is drawn bright; two presses of `right` move the highlight to
HARKONNEN). Genesis A takes the house; START does nothing there.

From the plate to the mission is 17 presses of Genesis A and one stretch of
animation nothing shortens: a black screen, four screens of house text, DO
YOU WISH TO JOIN HOUSE ATREIDES? with YES already chosen, another black
screen, seven lines of the mentat's briefing, PROCEED / ADVICE with PROCEED
chosen, and then a 600-frame zoom into the region map that ignores every
press. A press every 66 frames is comfortable; the tape spends 19 to cover
17 and the spares land on the zoom.

### Choosing the house

Atreides, and not only because it is the default. The three houses' briefing
text is in the ROM in plain ASCII and asks for the same thing:

- Atreides, mission 1, `0x1b51d`: *"Greetings, I am your Mentat Cyril.
  Together we will purge this planet of the foulness of the other Houses.
  The High Command wishes you to produce 1000 credits. You may earn credits
  by building a refinery and harvesting spice."*
- Atreides, mission 1, ADVICE, `0x1b6de`: *"First build concrete slabs, and
  then build a Windtrap and place it upon the concrete. Do the same for a
  refinery so that you can reach your quota of 1000 credits."*
- Atreides, mission 2, `0x1b780`: *"Greetings, I am honored to see you
  again. You are now required to either produce 2700 credits in a new
  harvesting area or remove the Ordos base in this region. Good luck!"*
- Ordos, mission 1 `0x1caa1` and mission 2 `0x1cd4b`: Mentat Ammon, 1000
  credits, then 2700 credits or the Harkonnen base.
- Harkonnen, mission 1 `0x1e035` and mission 2 `0x1e26b`: Mentat Radnor,
  1000 credits, then 2700 credits or the Atreides base.

Same quota, same alternative, same two missions. No house is shorter, so the
one that costs zero presses to pick wins.

### The cursor and the orders

The map scrolls at 32 pixels a tile. A held direction accelerates rather
than stepping, measured from a checkpoint with a restore between each:

| frames held | pixels moved |
|---|---|
| 1 | 1 |
| 2 | 2 |
| 4 | 4 |
| 8 | 10 |
| 16 | 24 |
| 32 | 65 |

**It does not glide.** Measured a frame at a time from a checkpoint, for
every hold up to 32 frames the cursor stops dead on the release; what the
reconnaissance saw was the camera recentring under a cursor that had already
stopped. What does happen, and only under a placement ghost, is that the
*tile* gains exactly one more in the direction of travel after the release,
because the ghost snaps to the grid. `engine.push` therefore aims one tile
short under a ghost and straight at the target on the bare map, and it reads
the blip every frame while the direction is held rather than counting
frames. The cumulative pixels for 1 to 32 frames held, measured with a
restore between each, are 1, 2, 3, 4, 5, 7, 8, 10, 11, 13, 15, 16, 18, 20,
22, 24, 26, 28, 30, 33, 35, 37, 40, 42, 45, 47, 50, 53, 56, 59, 62, 65.

Genesis A on a building selects it — a white outline, and a portrait with a
green health bar in the sidebar. Genesis A again opens that building's
panel: EXIT / FIX / STOP tabs, the item list on the left, a picture and the
name on the right, and three figures — cost in credits, power as a lightning
bolt, and a gear number that is the game's own build time. Genesis B backs
out of a panel or out of placement mode. `left` and `right` move the list.

The construction yard's list at mission 1 is CONCRETE ($15, power 0, gear
40) and WINDTRAP ($300, power 100, gear 400). REFINERY ($400, power -30,
gear 900) appears once a windtrap stands.

Genesis A on the selected row starts the build, which is paid for gradually
— the windtrap's 300 over the 360 frames from frame 96 to frame 456 after
the press — and the yard shows an OK badge when it is done. Genesis A on the
OK yard enters placement: a footprint ghost follows the cursor, 2 by 2 tiles
for a windtrap, 3 by 2 for a refinery.

**The ghost's hatching is red where the footprint is refused and green where
it is allowed, and Genesis A only places on green.** That was found by
writing `run.look()` to a PNG and counting pixels with r > 140, g < 80,
b < 80 outside the session — about 2,350 to 2,575 red pixels with a refused
ghost on screen, 82 to 320 with an allowed one or none at all — and
sweeping the ghost along a row of tiles and reading that count is what
finally put the refinery down. The harness counts the same pixels itself
now: "The ghost, counted" below.

### Mission 1, played by hand

At rate 0, in the session, from the mission's first frame:

| what | credits after |
|---|---|
| mission starts | 990 |
| windtrap built and placed | 690 |
| one concrete slab and the refinery built | 275 |
| refinery placed, on bare sand | 275 |

690 to 275 is 415, which is $400 for the refinery and $15 for a single
concrete slab. 990 to 690 is the windtrap's $300 exactly, so nothing was
under it: **neither of the two buildings this play put up stood on
concrete.**

Then, counting from the frame the refinery went down:

| frames | what |
|---|---|
| +6,600 | the harvester's first delivery starts; credits begin to climb |
| +6,600 to +11,100 | credits climb 282 to 981, about 49 every 300 frames |
| +15,000 | still 981, three buildings, nothing near the base |
| +15,000 to +16,200 | the refinery explodes; `buildings` 3 to 2, credits 981 to 7 |
| +21,000 to +23,400 | the windtrap goes the same way; `buildings` 2 to 1 |
| +44,400 | the construction yard still stands, alone, on 7 credits |

**What ended it was the refinery, not an enemy.** The frame before the
explosion has no hostile unit anywhere on screen and no shot in flight; the
refinery simply blows up, in fireballs, on ground it was never entitled to.
Between 21,000 and 23,400 frames after the placement the windtrap goes the
same way, and from there to 44,400 the only thing standing is the
construction yard the mission started with, in a field of craters and
wreckage. The mentat's ADVICE text says to put
both buildings on concrete first, and it is not advice, it is the mechanic:
a building on bare sand decays until it explodes. The banked spice goes with
the refinery, because the refinery is where it was stored: 981 to 7.

The quota was never met. The arithmetic says it could not have been, not on
one delivery: 990 - 300 - 400 = 290, and a full harvester load is about 706,
so the first delivery tops out at 996. Mission 1 wants two deliveries — or a
cheaper opening. Skipping the windtrap leaves 590 and one load reaches
1,296, but a refinery draws 30 power it would not have; that is the first
thing a plan should measure.

Mission 2 was not reached, so its play is unmeasured. Its briefing is
quoted above.

### Determinism

- The restore probe fires on the session's first `checkpoint` and passed.
  The bundle's `verdicts.jsonl` holds one line, at frame 2376: *a restored
  state continues as the straight line*, passed, *120 frames compared, hash
  for hash*.
- Restored against restored, in the middle of mission 1 with a harvester
  moving and enemy units walking: from `m1-refinery-placed`, 120, 600 and
  1,800 frames played twice from the same restore agree hash for hash, and
  the credit figure agrees too.
- Straight against restored, at a later checkpoint (900 frames into the
  mission from that one): 120 and 1,800 frames, no difference.
- The same inputs twice from one checkpoint: one Genesis A press 600 frames
  before the reading, `20e4e50d59941d73` both times.
- **A one-frame delay changes the outcome.** The same press one frame later
  gives `a043fee596de0224`, two frames later `8670e81ebd4b5756`. The
  credits and the cursor were identical in all three, so the difference is
  the units and the sand under them; unlike Theme Park's even-frame cursor,
  this game has a search space.

### The clock

The core runs at 59.9227 fps. There is no clock on screen, nothing in RAM
that counts game minutes, and no speed setting in OPTIONS, so a game minute
is not a unit this game has. What it has instead is the gear figure on the
build panel: a windtrap of gear 400 was paid off 456 frames after the press,
which is the only measured point on that scale. Concrete is gear 40 and a
refinery gear 900.

What can be skipped: START skips the intro (the title menu's picture is
already matchable at frame 1,416 and resting by 1,980); Genesis A advances
every house-text, join-prompt, briefing and PROCEED screen, at any cadence
down to 66 frames; the 600-frame zoom into the region map after PROCEED
answers to nothing. No end-of-mission screen was reached, so whether that
one skips is unknown.

## The ghost, counted

The hatching is two exact colours, **red `168,0,0` where the footprint is
refused and green `0,168,0` where it is allowed**, and the crop that holds
it is the 64 by 64 box centred on the cursor,
`cursor_x - 32, cursor_y - 32, 64, 64`. `run.colours(crop, rgb, 0)` counts
them without a PNG and without leaving the session.

From `tapes/title-to-mission-1.yaml`, a windtrap started on the construction
yard with Genesis A, A, `down`, `right`, A, and then
`run_until("colour 168,0,0 within 0 over 89,57,64,64 at least 300", 3600)`,
which came back after 234 frames:

| the ghost | crop | red `168,0,0` | green `0,168,0` |
|---|---|---|---|
| refused, on the yard, tile (16,39) | `89,57,64,64` | 384 | 0 |
| allowed, two taps of `down`, tile (16,43) | `89,78,64,64` | 0 | 384 |

384 of the crop's 4,096 pixels either way — the hatch is a regular dither,
3 pixels in 32, and it is one colour or the other, never both, so `at least
300` separates them with room to spare. Genesis A on the green ghost put the
windtrap down, `buildings` 1 to 2; Genesis A on the red one placed nothing
and opened the yard's panel instead. The four numbers repeat run for run.

**Placement is entered by the game, not by a press.** The build being paid
for is the whole of it: the panel closes on its own and the ghost is on the
map about 420 frames after the press that started the windtrap, with the
cursor where it was left.

## Running it

The tape:

    _install/tash/tash tape check examples/dune/tapes/title-to-mission-1.yaml
    _install/tash/tash run --profile examples/dune/profile.yaml \
      --tape examples/dune/tapes/title-to-mission-1.yaml \
      --frames 600 --bundle _runs

It anchors on the title menu's perceptual hash, mashes START across the
window because the anchor matches while the screen is still fading in,
anchors on the house plate, spends 19 presses of Genesis A, and finishes on
a watch rather than a picture — `credits equal 990` — because the mission's
first frames have a scrolling map and walking units and no still picture to
hash.

## Facts a plan rests on (checked 2026-09-14 on dev)

- Dune boots to Atreides mission 1 with the player in control in **3,866
  frames**: START on the title menu, then 17 presses of Genesis A, which is
  retro `y`. `tapes/title-to-mission-1.yaml` plays it, and plays twice to
  the same frame — exact hash `eadc5346ecebd4f5` at frame 4,466 after 600
  settling frames, byte-identical PNGs.
- Genesis A is retro `y`, Genesis B is retro `b`, Genesis C is retro `a`.
  The retropad has no `c`.
- ATREIDES is the plate the game starts on, and all three houses ask for
  the same two missions (1000 credits, then 2700 credits or the enemy
  base), so Atreides costs the fewest presses and no extra frames.
- Credits are a 16-bit little-endian word at `0x4d6a` in byte-swapped work
  RAM. `0xc05a` and `0xd6c2` are the drawn counter chasing it.
- The player's standing buildings are the word at `0xdbee`. No enemy or
  unit count could be isolated: mission 1 gives no second state with a
  known count to filter against.
- There is no mission-won or mission-lost word: a building's destruction
  does not set one, and the differential around the refinery's explosion
  changes 1,034 of 65,162 stable bytes and holds nothing that reads as a
  flag. Step 2 won the mission and found the mission *number* instead -
  `0x4ec1`, "Winning, and the mission number" below.
- The map cursor is `0xbf12` / `0xbf14` in screen pixels; the same cursor as
  a radar blip is `0xbf4c` / `0xbf4e`. Step 2 corrected the tile it names to
  `((radar_x - 240) // 2, (radar_y - 144) // 2)` over a 32 by 32 playable
  map, and the build list's row to `0xbf8c` - `0xbf8a` is its column.
- A held direction accelerates: 1, 2, 4, 10, 24 and 65 pixels for 1, 2, 4,
  8, 16 and 32 frames held. A tile is 32 pixels. Step 2 measured every hold
  to 32 frames and found **no glide** - see "The cursor and the orders".
- Mission 1's opening purse is 990 and its quota is 1,000. A windtrap is
  $300, a refinery $400, a concrete slab $15. A full harvester load is
  about 706 credits, so the quota needs two deliveries after a windtrap and
  a refinery, or an opening that skips the windtrap.
- The harvester's first delivery starts 6,600 frames after the refinery is
  placed and pays in at about 49 credits per 300 frames for 4,500 frames.
- **A building off concrete decays and explodes**, bare rock included -
  step 2 lost a refinery on rock the same way. Neither building this
  play put up was on concrete. The refinery blew up about 15,400 frames
  after placement, with no enemy near it, and the credits stored in it went
  from 981 to 7; the windtrap followed, and the construction yard was the
  only thing left standing. The mentat's ADVICE text — concrete first, then
  the building on it — is the mechanic, not a hint.
- A build is paid for over time: the windtrap's 300 credits left the purse
  between frame 96 and frame 456 after the press, 50 every 60 frames.
- Placement mode draws a footprint ghost whose hatching is **red where the
  footprint is refused and green where it is allowed**; Genesis A places
  only on green. The two colours are exact — `168,0,0` and `0,168,0` —
  and `run.colours()` counts them in the session: "The ghost, counted".
- There is no speed setting and no game clock. The core runs at 59.9227
  fps; the build panel's gear number is the game's own time unit and gear
  400 measured 456 frames.
- The title's OPTIONS page offers ENTER PASSWORD, so missions are reachable
  out of order if the codes can be found; they are not plain ASCII in the
  ROM.
- The run is deterministic from a checkpoint — restored against restored and
  straight against restored both agree for 1,800 frames in the middle of a
  live mission — but **a one-frame delay on one press changes the frame 600
  frames later**. There is a search space here.

## The engine

`engine.py` is the cursor, the yard's build list and the placement ghost;
`state.py` is the map array and the entity table; `player.py` is the line
that wins both missions. Every number below was measured in a
session on this ROM, from a checkpoint, with a restore between attempts.

| constant | value | how it was measured |
|---|---|---|
| tile from the blip | `((radar_x - 240) // 2, (radar_y - 144) // 2)` | a one-frame tap moves the tile by one and the blip by two, on both axes, from every tile tried |
| a tile | 32 screen pixels, 2 blip pixels | the acceleration table against the tile the blip names |
| the cursor's reach | 30 by 30 tiles | the blip clamps at x 240..298, y 144..202 |
| the ghost crop | `cursor - 32, 64, 64`, clamped inside 320 by 224 | the reconnaissance's, re-measured: 384 pixels of 4,096 either way |
| the ghost's verdict | red `168,0,0` refused, green `0,168,0` allowed, threshold 300 | never both colours, never between |
| the list is up | 4,859 pixels of `32,32,64` over `0,190,320,34` | 0 to 15 on the map at every checkpoint tried, so 1,000 separates them |
| opening the list | Genesis A twice on the yard | the first press writes the yard's position to `picked_x`/`picked_y` and draws the bracket; the second draws the list |
| closing the list | EXIT, row 0 column 0, then A | Genesis B does not close it: two B taps left `panel_row` at 1 |
| the list's shape | row 0 is EXIT / FIX / STOP, items three to a row under it | walking `down` stops at row 1, walking `right` stops at column 2 with three items on it |
| the items | concrete (1,0), windtrap (1,1), refinery (1,2) | `panel_price` reads 15, 300 and 400 on them; the refinery's seat is empty until a windtrap stands |
| order to placement | concrete 133-167, windtrap 403-406, refinery 643-651 frames | the frames from the A press that starts the build to the first frame the ghost answers, over seven orders |

The brief's "about 420 frames after a paid-off build" is the windtrap's
number. It is not one constant: it tracks the item, and the three gear
figures the panel draws (40, 400, 900) order the three the same way.

`move_to(tile)` holds a direction, reads the blip every frame, releases when
the tile it wants is reached (one short under a ghost), waits for the blip
to hold still for 20 frames, and repeats for the other axis, up to six
rounds; the press happens in the same call. `place(building, tiles)` orders
the item off the list, waits for the ghost to appear, and walks the
candidate tiles until `run.colours` reads green on one, stepping 180 frames
and trying again when none of them is — because a unit standing on a
footprint turns its ghost red, and units wander off.

## The state model

| what | address | width | meaning | how it was proved |
|---|---|---|---|---|
| credits | `0x4d6a` | 2 | the purse | the reconnaissance's hunt |
| buildings | `0xdbee` | 2 | the player's standing buildings | the reconnaissance's hunt |
| mission | `0x4ec1` | 1 | the mission number | below |
| map array | `0x8ddc` | 4 a tile, row stride `0x100` | the 32 by 32 playable map | below |
| map `+0` | | 1 | terrain: 126 concrete, 127 sand, 143 rock, 128-142 sand and rock edges, 177-207 spice, 243-246 the yard's own four tiles | below |
| map `+1` | | 1 | shroud: 246 hidden, 0 revealed, 218 to 244 the partial edges | reads 246 everywhere the screen is black and 0 everywhere it is not |
| map `+2` | | 1 | the entity standing on the tile, 0 for none | the indices it holds are exactly the entity table's live records |
| map `+3` | | 1 | flags: the low three bits the ground's owner, 8 revealed, 16 a unit, 32 a building | below, and corrected in step 2 |
| unit table | `0x0f44`, stride 140 | 140 a record | every unit, indexed by the number map `+2` holds where map `+3` carries bit 4 | below |
| building table | `0x4e26`, stride 98 | 98 a record | every building, indexed by the number map `+2` holds where map `+3` carries bit 5 | below |
| record `+2` | | 2 | a load word, but not the harvester's alone: step 4 read it climbing past 25,000 on a unit of 130 health that never moved | below, and "The Ordos attack" |
| record `+57` | | 1 | the owner: 1 Atreides, 2 Ordos, in both tables | below |
| record `+58` / `+60` | | 2 | position, `(tile + 16) << 8` plus a sub-tile byte, 0x80 centred | below |
| record `+66` | | 2 | health, in both tables; zero is destroyed, and the undamaged figure is the ROM's for the type, which is how step 4 names the harvester by its 150 | below |
| enemy buildings | `0xdbec` | 2 | the Ordos' standing buildings, beside `0xdbee` for mine | below |
| panel row / column | `0xbf8c` / `0xbf8a` | 2 | where the build list's highlight sits | a 256-byte window diffed across one press of `down` and one of `right` |
| panel price | `0xfe8a` | 2 | the highlighted item's cost | a width-2 hunt stepped on the values 15, 300, 400, 15, 300 as the highlight walked: 32,768 candidates to 3, all three moving together, `0xfe8a` the lowest |
| picked | `0xbf5c` / `0xbf5a` | 2 | the selected object's position, same encoding as a record's | pressing A on the yard wrote (24,35) to them, which is the yard's tile plus the map's 16-tile inset |

### The map array

Reading four bytes at `0x8ddc + y * 0x100 + x * 4` gives the tile the cursor
calls `(x, y)`. Data is present for x 0..31 and zero for x 32..63, so the
playable map is 32 by 32 inside a 64-wide row, and the game's own tile
numbering is 16 higher on each axis than the cursor's: a record's position
words and the `picked` words both read the cursor's tile plus 16.

The encoding was read off the screen. With the cursor parked on a known tile
its screen pixel is known, every other tile is 32 pixels away per tile, and
sampling nine pixels of each revealed tile against its `+0` byte gives 143 →
`96,68,0` (163 samples of 189), 61, 132 and 134 → `168,136,64`, and 77 to 79
→ `168,168,136`. 126 is concrete because placing one slab at (8,21) turned
`+0` of (8,21), (9,21), (8,22) and (9,22) — and only those — to 126, which
also fixes the slab at 2 by 2 anchored on the cursor's tile. 243 to 246 are
the construction yard's four tiles.

`+3`'s bits were read the same way: the yard's tiles carry 41 (32 + 8 + 1),
a fresh slab carries 9 (8 + 1), a tile with a unit on it carries 24
(16 + 8), and bare revealed ground carries 8. **A unit's presence replaces
the concrete bit rather than joining it**: a tile that reads 9 reads 24
while a unit stands there, and 9 again when it leaves. That matters because
the game will not lay concrete under a unit at all, and no later slab may
overlap concrete already down, so a slab placed over a unit leaves a hole in
the pad that nothing can fill. **The "concrete bit" is not one**: step 2
reads the low three bits as the side that owns the ground, and it is only
because the Atreides are side 1 that a slab of theirs reads 9. Concrete is
terrain — `+0` equal to 126 — and that is what `state.is_concrete` tests.

### The entity table

Sixteen units were alive with their tiles known from map `+2`. For each, the
set of RAM offsets holding its game-tile y with its game-tile x two bytes
on was collected; then every stride from 16 to 256 was tried against every
seat of the lowest-numbered unit, counting how many of the sixteen landed on
`first + (index - 26) * stride`. **Exactly one candidate hit eight or more,
and it hit all sixteen**: stride 140, entity 26's y-tile byte at 7,607. That
makes a record `0x0f44 + 140 * index` with y at `+58` and x at `+60`, which
is the base the reconnaissance's pairwise-spacing histogram had suggested.

Step 2 read "buildings share the table" off the fact that the yard's tiles
carry a small index; **they do not**. Buildings keep a table of their own at
`0x4e26` with stride 98, found the same way: every offset holding a known
yard's game-tile y with its x two bytes on was collected and every stride
from 16 to 256 fitted against it. The map's `+2` byte is an index into
whichever table `+3` names — bit 4 the unit table, bit 5 the building table
— which is why one small number appeared to mean both. The two tables share
their offsets: `+2` load, `+57` owner, `+58`/`+60` position, `+66` health.

The load is `+2`. Polling all 64 records every 250 frames across a delivery,
it is the only word that climbs to `0x6400` — 100 in its high byte — and
drains to 0 exactly as credits go 216 to 915. A second load carried the
purse from 915 to 1,005.

`+25` is neither the owner nor a class. Mission 2 has both houses on one
map: across twenty live records `+25` reads 0, 1 and 2 with no relation to
the house — a unit of mine reading 2 sits beside an Ordos unit reading 1 —
and it changes on a unit that never dies, unit 37 reading 1 at one poll and
2 at the next with its tile and its health unmoved. Step 2's guess is
withdrawn.

**`+57` is the owner**: 1 for every record the Atreides own and 2 for every
Ordos one, in both tables, over twenty units and seven buildings. It
partitions the map the way the game's own counters do — `0xdbec` counts the
records reading 2 and `0xdbee` the ones reading 1 — and `0xdbec` reads 0
through the whole of mission 1, which fields no enemy.

**`+66` is health, and it is a word, not a byte.** Read as a byte the
construction yard under fire goes 32, 20, 8, 252, 220 — a low byte wrapping
under a high byte that steps down with it. Read as the little-endian word
the byte-swapped ram spells there it goes 600, 588, 576, 564, 552, and the
undamaged values are exactly the ROM's own `[CONSTRUCT]` table at `0x5318d`
**doubled for a building** and taken straight for a unit: windtrap 200 in
the table and 400 in the record, refinery 450 and 900, silo 150 and 300,
construction yard 400 and a highest reading of 712 on a yard that was
already being shot at; and infantry 50, trooper 45, raider trike 80, trike
100, quad 130, harvester 150, every one of them a record's health to the
credit. Step 2's refinery poll missed it because it polled a byte every 600
frames, and a decaying building's low byte is back where it started by the
next poll.

A unit shot by a trike changes its `+66` word and nothing else that holds
still: at zero the slot's position words go to (255,255), the map tile it
stood on loses bit 4 of `+3` and its `+2` index, and the slot is reused by
the next unit the game delivers. A building at zero leaves its record in
place with health 0, loses bit 5 on every tile of its footprint, and the
terrain under it turns to rubble — 91 and 97 where rock had been. That is
why `state.standing` asks for health above zero as well as a tile on the
map.

**No class byte is in the record.** The ROM's `[CONSTRUCT]` table ends each
line with the type index doubled — soldier 2, infantry 4, trooper 6,
troopers 8, trike 10, raider trike 12, quad 14, carryall 16, harvester 18 —
and no offset in the 140 bytes carries that number for the units whose type
their health gives away. `+63` and `+121` do split the records two ways, 4
and 6 for everything on foot against 8 and 8 for everything on wheels, which
is a reach or a speed and not a class. The one field that names a harvester
is `+2`: it is the only record whose load word is ever non-zero, and in
mission 2 the Ordos' three harvesters and mine are exactly the four records
that carried.

### Winning, and the mission number

A width-1 hunt seeded on the value 1 three times over 900 frames of mission
1 kept 167 candidates; stepped on the value 2 once mission 2 had loaded it
kept 17. Eight of those are inside the entity table and four read 0 before
the first building went up, which leaves five that read 1 from the mission's
first frame — `0x4ec1`, `0x4f04`, `0xc04c`, `0xc066`, `0xc8d2` — moving
together and holding 2 through 6,000 frames of mission 2. `0x4ec1` is the
lowest and is what `profile.yaml` names.

**There is no won flag.** A second width-1 hunt kept the 60,089 bytes that
held still across 900 frames of mission 1 and then asked which of them had
changed by the victory plate: 47, every one of them in `0xe0..`, which is
the plate's own sprite memory. The mission number is the state model's
answer, and the plate itself is the anchor: perceptual `30b14b4e6e34b8cb`,
30 bits from the map's `50ffc04a8b5bce0a`, so `within 4` separates them with
room to spare. The plate is drawn **519 frames** after credits cross 1,000,
and its first four frames all hash `569943f9f108d9df` exactly.

**No lost flag, and the mission byte is the loss.** Mission 2 fields an
enemy, so the provocation that mission 1 could not make was made twice from
the same checkpoint. Doing nothing from control, the last of my buildings
fell at **+31,200 frames** and the run stayed in mission 2 for 148,800
frames more with six units alive: an empty building table is not a loss.
Sending the army across the map instead (route B below) killed the last
unit as well, and then the mission ended — `0x4ec1` reads **0**, the screen
is the Mentat re-briefing ("THE SERIOUSNESS OF YOUR MISSION CANNOT BE
OVEREMPHASIZED!", perceptual `509f2788f81f620f`, an animating plate) and
every watch goes to nonsense: credits 63,897, both tables empty,
`enemy_buildings` frozen on the 6 it held. So the loss is nothing new to
read — it is the mission number leaving 2 — and no watch means anything
while it reads 0.

## Into mission 2

`tapes/mission-1-to-mission-2.yaml` carries the run from mission 1's
victory plate to the first frame of mission 2 that answers the pad: two
segments, **2,856 frames**, 755 of them waited, 32 transitions, no retries.
The first segment anchors on the plate's own perceptual hash and spends
sixteen presses of Genesis A at a 126-frame cadence on the plate and the
Mentat's briefing; the second waits on `credits_shown equal 1200`.

It has to be `credits_shown` and not `credits`. The purse already holds
1,200 while the loading screen is still black and the mission byte still
reads 1, so a segment anchored on `credits` ends on a black screen four
hundred frames early. `credits_shown` is the counter the mission draws, and
it only exists once the mission is on screen.

### The opening, measured

| what | mission 2 opens with |
|---|---|
| purse | **1,200** |
| the quota | **2,700 credits, or the Ordos base removed** |
| my construction yard | building record 7, tiles (16,21) (17,21) (16,22) (17,22) |
| the cursor | tile (16,21) |
| my units | unit records 36 to 42, seven of them |
| the Ordos base | building records 1 to 6 at (2,1) (4,1) (2,3) (4,3) (6,3) (5,5), six buildings; record 3 is their construction yard, terrain 243-246 |
| the Ordos units | records 26 to 35, ten of them, three of which carry spice |
| the counters | `0xdbee` 1 for mine, `0xdbec` 6 for theirs |
| spice | 133 tiles; the nearest field is (20..23, 22..24), five tiles east of the yard |
| shroud | 937 of the 1,024 tiles hidden, and the map array and both tables read true underneath every one of them |

The game states both conditions itself, in the briefing the tape plays
through, and the sentence is in the ROM at `0x1b780`:

> You are now required to either produce 2700 credits in a new harvesting
> area or remove the Ordos base in this region.

### The yard's list in mission 2

Six items, three to a row under the tab strip, with the prices the panel's
own word at `0xfe8a` reads as the highlight walks them:

| row, column | item | price |
|---|---|---|
| 1,0 | concrete | 15 |
| 1,1 | windtrap | 300 |
| 1,2 | refinery | 400 |
| 2,0 | OUTPOST | 400 |
| 2,1 | SPICE SILO | 150 |
| 2,2 | VEHICLE | 400 |

`engine.ITEMS` indexes them and `engine.PRICES` holds the prices, which is
what `player.afford` waits on: an order the purse cannot cover is refused
silently, and the refusal costs the 1,800 frames `wait_for_ghost` spends
before it gives up.

### The purse has a ceiling

A refinery holds a thousand credits of spice and there is nowhere else to
put it. Measured: with a yard, a windtrap and one refinery standing, the
purse climbed to **1,005** and then held there for 25,000 frames while the
harvester went on shuttling — four round trips that banked nothing. Mission
1's quota is 1,000 and never met it. Mission 2's is 2,700, so the quota
cannot be reached without storage: each silo holds another thousand and two
of them carry the ceiling to 3,000. That is the single fact the whole
harvest route turns on, and nothing on screen says it.

### The Ordos attack, and what answers it

Left alone, the base does not survive the quota. From the checkpoint with
the base up, doing nothing but stepping: the construction yard fell at
+23,700 frames, the refinery at +32,100 — which also cost the 1,226 credits
it was holding, the purse dropping from 2,269 to 4 in one poll — and the
windtrap at +35,400. The seven units of the opening army die one at a time
without ever being ordered anywhere.

The harvest needs about 33,000 frames and the base lives about 33,000
frames, so the two routes are within a poll of each other and the run that
merely steps loses by four hundred credits. The frames the harvest spends
are frames the cursor has nothing to do in, so `player.harvest` spends
them on orders instead of on `step`: every poll it reads the Ordos records,
takes the nearest one inside ten tiles of the yard, and sends the whole
opening army at that tile. The army still dies — all seven of it — but the
base it bought lives past the quota. The trial that fixed the loop spent
171 orders on it and at the quota the construction yard still read 336 of
its 800 health with the refinery and both silos untouched at 900, 300 and
300; the recorded run spends 81.

The harvester is never sent at a raider, and step 4 had to find a better
way to know which record it is. Two earlier guesses are withdrawn here. The
roster at control does not name it: mission 1 has no unit at all at control
and five foot soldiers join during the base, so "anything that joins later
is a harvester" sends infantry onto the spice. The load word `+2` does not
name it either: in mission 2 it climbs, by thousands, on a unit of 130
health that never moves, while the harvester's own reads 0 between loads.
What does name it is the health word: `+66` carries the ROM's own figure
for the type, and **150 belongs to the harvester alone** in both missions —
the opening armies read 20, 50, 90, 100 and 130 and nothing else reaches
it. `player.reapers` remembers a slot the first poll it reads 150, so a
harvester stays a harvester once it takes a hit, and `player.harvest`
orders those onto the spice and every other record at the raider.

`+25` is not a class byte: the harvester reads 0 in mission 1 and 2 in
mission 2. Step 3 withdrew it once already and this step withdraws it
again.

### Route B, and why it is not the line

The other win condition is the Ordos base, and it was costed from the same
`m2-control` checkpoint by the same kind of program: every volley reads the
building table, takes the Ordos record with the least health, orders every
unit I own onto its tile, and waits up to 3,600 frames for that record or
my army to go.

| volley | frame | target | health | Ordos buildings | my army | my buildings |
|---|---|---|---|---|---|---|
| 0 | 0 | record 1 at (2,1) | 300 | 6 | 7 | 1 |
| 1 | 9,700 | record 1 at (2,1) | 300 | 6 | 1 | 0 |
| — | 21,386 | — | — | 6 | 0 | 0 |

**Nothing ever hit anything.** The target's health word reads 300 at both
volleys and `0xdbec` reads 6 at the end: in 21,386 frames the army did not
take a single hit point off the Ordos base. Their base is in the map's
top-left corner, 34 tiles from my yard, and the seven units of the opening
army cross open sand into it one at a time — six of them were dead by frame
9,700, and my construction yard with them, because the units that killed it
were the ones I had just sent away.

So route B costs **21,386 frames and the mission**, against **44,251
frames from control to the quota** on route A as the line stood then, and
41,382 on the line below. Route A is what `player.py` plays. Route B is the
only loss this example has ever provoked, and is what the mission byte's
zero above was measured on.

## Both missions, won by one program

`tash run --profile examples/dune/profile.yaml --scenario
examples/dune/player.py --bundle _runs --name dune-player`. One program
from power on: the tape into mission 1, a base planned off the map array,
the harvest, the plate, the briefing tape, then mission 2 the same way.
**72,580 frames from power on** against the 77,722 the two scenarios it
replaces cost, and nothing in it looks at a frame except the ghost's two
colours, the panel's chrome and the victory plate's hash.

| mark | frame | what it beat | gain |
|---|---|---|---|
| the tape ends and mission 1 answers | 3,927 | 3,927 | — |
| mission 1's base stands | 11,749 | 15,498 | 3,749 |
| mission 1's quota, 1,005 credits | 27,986 | 29,998 | 2,012 |
| mission 1's victory plate | 28,065 | 30,617 | 2,552 |
| the tape ends and mission 2 answers | 30,918 | 33,471 | 2,553 |
| mission 2's base and both silos stand | 43,849 | 43,537 | −312 |
| mission 2's quota, 2,700 credits | 72,300 | 77,260 | 4,960 |
| mission 2's victory plate | 72,580 | 77,722 | 5,142 |

The frame column is the run's **kept** frames, which is what a replay
plays back. `run.frames()` reads 143,771 at the plate, because it does not
fold back when a search restores; the two differ by exactly the 71,191
frames the four searches ran and threw away. The second column is what the
line this one replaces measured: the base row and the quota row are
`mission1.py`'s own, because the combined line never marked them, and the
rest are `mission2.py`'s.

### What the planner chose

Every decision reads the map array again. `costs_of` turns the 32 by 32 map
into a cost grid — sand, its rock edges and spice cost 1, rock and anything
standing are `tash.plan.IMPASSABLE` — and `spice_of` hands every spice tile
to `tash.plan.distances` as a source. A candidate pad is rock, so it is a
wall in that grid and its own cell answers `UNREACHED`; what ranks it
instead is **the sand it has to cross to reach the spice**, the cheapest
`spread[cell] + |middle - cell|` over every crossable cell. The windtrap
takes the pad that ranks worst and the refinery the best, which leaves the
ground nearest the spice to the building that uses it.

| | building | pad | candidates | ranked, cheapest first | chosen | trials |
|---|---|---|---|---|---|---|
| mission 1 | windtrap | 2 by 2 | 10 | (8, 21) 8 | (8, 21) | — |
| mission 1 | refinery | 4 by 2 | 5 | (10, 22) 5, (10, 21) 6, (10, 18) 7 | (10, 22) | 3 |
| mission 2 | windtrap | 2 by 2 | 7 | (14, 20) 7 | (14, 20) | — |
| mission 2 | refinery | 4 by 2 | 2 | (15, 23) 4, (14, 23) 5 | (15, 23) | 2 |
| mission 2 | first silo | 2 by 2 | 4 | (17, 25) 5, (16, 25) 6, (15, 25) 7 | (17, 25) | 3 |
| mission 2 | second silo | 2 by 2 | 4 | (15, 25) 7, (14, 25) 8, (15, 26) 8 | (15, 25) | 3 |

Only the three cheapest are listed, and only they are judged. A windtrap
row carries one seat because the windtrap is not searched at all: it takes
the ranking's last place outright.

### What the search judged

`tash.search` judges a pad in the emulator: it checkpoints, restores for
each candidate, builds there and scores `-run.frames()`, so the earliest
finisher wins. A refinery trial waits for the purse to climb — the
harvester's first load — and a silo trial for the building to stand; a
trial that fails still spends its cap, so it ranks last with no channel
between `apply` and `score`.

| search | trials | rejected | chosen | frames |
|---|---|---|---|---|
| mission 1 refinery | 3 | (10, 21), (10, 18) | (10, 22) | 36,723 |
| mission 2 refinery | 2 | (14, 23) | (15, 23) | 23,235 |
| mission 2 first silo | 3 | (16, 25), (15, 25) | (17, 25) | 5,529 |
| mission 2 second silo | 3 | (14, 25), (15, 26) | (15, 25) | 5,704 |

The search confirmed the planner's first choice every time, which is the
useful answer: the distance ranking is worth trusting, and the four
searches bought certainty rather than a different pad. The refinery trials
all spent their 12,000-frame cap — no candidate banks a load that fast — so
those two searches ranked by how quickly the pad could be paved and the
building raised, not by the round trip. The silo trials finished well inside
4,000.

Their frames are free to the line: every one is restored away rather than
kept, so `_runs` records 72,580 kept frames of the 143,771 the core ran,
and the bundle's `tape.yaml` is eight segments and exactly those 72,580 —
the tape folds back at every restore even though the film does not. On the
build this step ends on the search's checkpoints are `unprobed` in
`run.yaml` — a search checkpoint is scratch now — which is why this bundle
carries no "a restored state continues as the straight line" verdict: the
line takes no named checkpoint of its own.

### The orders

| mission | order | seat | frames | paid | ghost after |
|---|---|---|---|---|---|
| 1 | concrete | (8, 21) | 1,062 | 15 | 134 |
| 1 | windtrap | (8, 21) | 958 | 300 | 377 |
| 1 | concrete | (10, 22) | 775 | 18 | 164 |
| 1 | concrete | (12, 22) | 852 | 18 | 162 |
| 1 | concrete | (13, 23) | 897 | 18 | 163 |
| 1 | refinery | (11, 22) | 1,478 | 408 | 651 |
| 2 | concrete | (14, 20) | 995 | 15 | 136 |
| 2 | windtrap | (14, 20) | 982 | 300 | 377 |
| 2 | concrete | (16, 23) | 804 | 15 | 135 |
| 2 | concrete | (14, 23) | 740 | 15 | 137 |
| 2 | concrete | (18, 23) | 788 | 15 | 140 |
| 2 | refinery | (15, 23) | 1,329 | 400 | 615 |
| 2 | concrete | (17, 25) | 759 | 9 | 136 |
| 2 | silo | (17, 25) | 1,107 | 150 | 382 |
| 2 | concrete | (15, 25) | 772 | 15 | 137 |
| 2 | silo | (15, 25) | 1,108 | 150 | 382 |

A building's seat is where the cursor stood when the game took the
placement, which for the 4 by 2 refinery is whichever tile of the pad's top
row it accepted first, not the pad's corner.

The paid column is the purse's fall over the order, which laps the previous
order's instalments — a build is paid 50 credits every 60 frames from 96
frames after the press — so the concrete slabs read 9, 15 or 18 rather than
15 flat. The seats are what the planner chose on this run; none is written
into the program.

The harvest that follows is a poll loop over the tables and not a fixed
roster: 28 defence orders and 1 harvester order in mission 1, 60 and 1 in
mission 2.

### The replay

`tape replay --bundle _runs/2026-09-14T16-35-34Z-dune-player --record
_runs` plays the recorded inputs back into a fresh core: **72,580 of the
72,580 frames the tape keeps, hash `569943f9f108d9df`, watches match**,
into `_runs/2026-09-14T16-38-13Z-dune-player-replay`. That is the same
plate hash `mission2.py` ended on, 5,142 frames earlier.

### What the two lines it replaces cost

`mission1.py` won mission 1 alone in 29,998 frames and `mission2.py` played
both for 77,722. Their measured tables, kept here because they are the
numbers the table above is read against:

| mission 1 by `mission1.py` | frame |
|---|---|
| the tape ends | 3,866 |
| the cursor answers, and control starts | 3,927 |
| three buildings stand | 15,498 |
| credits reach 1,005, and the victory plate | 29,998 |

| order | seat | frames | credits | ghost after |
|---|---|---|---|---|
| concrete | (10,19) | 718 | 15 | 135 |
| concrete | (12,19) | 726 | 18 | 165 |
| concrete | (8,21) | 723 | 18 | 163 |
| concrete | (10,21) | 761 | 18 | 163 |
| concrete | (12,17) | 794 | 18 | 165 |
| windtrap | (12,17) | 1,115 | 319 | 403 |
| refinery | (8,21) | 1,334 | 406 | 643 |

Mission 1's own section claimed 29,998 frames while the combined line
measured the same leg at 30,617; step 3 left that open and it is answered
here — the difference was `open_panel` polling the build list across its
fade, and neither number is the line's any more.

| mission 2 by `mission2.py` | frame |
|---|---|
| mission 1's victory plate | 30,617 |
| the tape ends and mission 2 answers | 33,471 |
| the base, the refinery and both silos stand | 43,537 |
| the quota, 2,700 credits | 77,260 |
| the victory plate | 77,722 |

| order | seat | frames | paid | ghost after |
|---|---|---|---|---|
| concrete | (15,23) | 1,281 | 15 | 137 |
| concrete | (17,23) | 747 | 15 | 134 |
| concrete | (17,25) | 747 | 15 | 134 |
| windtrap | (17,25) | 1,054 | 300 | 377 |
| refinery | (15,23) | 1,369 | 400 | 619 |
| concrete | (14,21) | 717 | 16 | 138 |
| SPICE SILO | (14,21) | 1,025 | 150 | 378 |
| concrete | (15,25) | 767 | 15 | 138 |
| SPICE SILO | (15,25) | 1,132 | 150 | 379 |

That line issued 81 defence orders across the two harvests and never
touched the harvester.

### What step 4 had to fix in the line it inherited

- **The panel opened on a poll.** `open_panel` waited about 600 frames for
  the list to fade in. The two presses are measured now: the pick lands on
  the release of the first, and the list answers **39 frames** after the
  release of the second, with any gap down to 10 frames between them. 61
  frames instead of about 440, on every order. The measurement needed
  `run.step(60)` after the restore: a restored core drops the first input,
  which is what made the second press look like a cancelled order.
- **A slab laid over a unit leaves a hole**, and the old line abandoned a
  holed pad for the next candidate. `player.slabs` answers a seat for every
  tile still bare, widest cover first, and `lay` keeps going until none is:
  the hole at (13,23) on mission 1's pad was filled from a seat half
  outside it, for 15 credits and about 900 frames, where abandoning the pad
  cost the whole pad again.
- **A pad is not mine because a building touches it.** Mission 2's Ordos
  base offers pads of its own and the planner ranked one at (0,0) as
  farthest from the spice, so the windtrap went there and nothing else
  could be built. `pads` now keeps only seats within six tiles of my yard.
- **An order can land in the build list.** A raider standing on my own
  building turns `send`'s press into a press on the yard: the list opens,
  every later order walks the highlight instead of the map, and the harvest
  stands still — 120,000 frames of it on one run. `raider` skips a tile
  carrying a building, and every harvest poll closes the list if one is up
  with `engine.close_panel`, which walks to EXIT, because no button on the
  pad closes it.
- **The harvester is not in the roster at control, and it leaves the
  table.** Mission 1's harvester is delivered after control, and any
  harvester inside the refinery vanishes from `state.units` — the map's
  entity and unit flag both drop — so a set of known harvesters has to be
  sticky or the line loses it for good. `+66` reading 150 is the mark: `+2`
  is not a load byte of its own and `+25` is not a class.
- **A harvester that stops is re-ordered.** The purse not moving for sixty
  polls empties the working set, so the next poll routes every known
  harvester to the spice again.

## One frame later

Same checkpoint, same orders, one press moved by one frame, compared 600
frames on:

| | straight | one frame later |
|---|---|---|
| exact hash | `52a6d8155728ba0e` | `52a6d8155728ba0e` |
| perceptual hash | `40335b3e2d2c6ce6` | `40335b3e2d2c6ce6` |
| credits, cursor | 990, (121,89) | 990, (121,89) |
| work ram bytes differing | | 207 of 65,536 |

The screen is identical and every watch is identical, but the two runs have
already parted inside the machine: 207 bytes differ, 17 of them in the
entity table and **none at all in the map**. Run the same pair 6,000 frames
instead and the frame parts too — `942c2421160aa92b` against
`3cbd67b3e1c177f3`, the perceptual hash still equal at `40a14a3e3d3c6577`,
the watches still equal, 645 bytes differing with 81 in the entity table and
none in the map. So the one-frame offset costs nothing a placement cares
about and everything a unit's path does, which is the shape a step 4 search
has to plan around: the map is stable under a shifted press, the units are
not.

## v6b: the last level

Mission 9 is the last of the nine a house plays, and every house's ninth is
the same brief in three voices: Atreides "You are instructed to subdue
Emperor Frederick`s forces", Ordos "We expect you to destroy all of what
remains of his forces", Harkonnen "Destroy all of the Emperor`s forces"
(ROM `0x1c800`, `0x1dd80`, `0x1f100`). The enemy is house 4, Sardaukar, and
the map is the whole 64 by 64 board, not mission 1's 32 by 32 window.

### The password door

The title's OPTIONS page holds ENTER PASSWORD, and the entry screen is a
grid of 29 cells: `A`..`J`, `K`..`T`, then `U V W X Y Z ◄ ► END`.

| thing | where | what it reads |
|---|---|---|
| options row | `0xdbfc` | ENTER PASSWORD is row 5 |
| grid cursor x | `0xdbf0` | `84 + 16 * column`, ten columns |
| grid cursor y | `0xdbf2` | `52 + 16 * row` |
| typed letters | `0xffaf` | ten characters, ASCII |

The cell number is `(y - 52) // 16 * 10 + (x - 84) // 16`, and the cells
wrap as one ring of 29: `right` from END is `A` again, so the short way
round is `min(ahead, 29 - ahead)`. A **tap costs two frames a cell** — one
frame held, one frame let go — and that is the cheap way: a *held*
direction walked 11 cells in 120 frames, about 10.9 frames a cell, so the
hold is five times slower than tapping and the line taps. Genesis A (retro
`y`) types the cell under the cursor, and the **tenth letter parks the
cursor on END by itself**, so a ten-letter password never walks to END —
the eleventh A submits. A whole word costs 230 to 268 frames.

The codes are **plain ASCII in the ROM** at `0x20c14`, eleven bytes a code,
which corrects the v6 fact "they are not plain ASCII in the ROM". Twenty
four codes run Harkonnen, Atreides, Ordos for missions 2 to 9, then five
cheat words at `0x20d1c`:

| mission | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| 2 | DEMOLITION | DIPLOMATIC | DOMINATION |
| 3 | SPICESATYR | SPICEDANCE | SPICESABRE |
| 4 | BURNINGSUN | ETERNALSUN | ARRAKISSUN |
| 5 | DARKHUNTER | DEFTHUNTER | COLDHUNTER |
| 6 | EVILMENTAT | FAIRMENTAT | WILYMENTAT |
| 7 | ITSJOEBWAN | ASHLIKENNY | SLYMELANIE |
| 8 | DEVASTATOR | SONICBLAST | STEALTHWAR |
| **9** | **DEATHRULER** | **DUNERUNNER** | **POWERCRUSH** |

The five after them — DUNEFINALE, LOOKAROUND, SPLURGEOLA, PLAYTESTER,
VERSIONNUM — are not passwords: typed and submitted, DUNEFINALE leaves the
entry screen up (perceptual hash still `3ec1338c738b1cc3` 2,400 frames on),
and the `On  Off` strings that follow the table read like toggles.

**The password chooses the house.** No house plate appears: the entry
screen goes straight to the mentat briefing for that house's mission 9.
Sixteen A presses at a 66-frame cadence carry Atreides and Ordos to
control and nineteen carry Harkonnen; control is `0xbf12` going non-zero,
and the drawn purse then counts up to the real one over about 720 frames.

Proved on the emulator, after each password:

| | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| mission word `0xc04c` | 9 | 9 | 9 |
| purse | 2,500 | 2,500 | 2,000 |
| my units' owner byte | 0 | 1 | 2 |
| entry frames | 230 | 238 | 268 |

### The mission number, and what `0x4ec1` really is

`profile.yaml` named `0x4ec1` `mission`. It is **building record 1's owner
byte** — `0x4e26 + 1 * 98 + 57` — which read 1 and 2 in missions 1 and 2 by
coincidence and reads 4, Sardaukar, in every mission 9. The mission number
is the word at **`0xc04c`**, found by walking an eight-password Atreides
ladder and keeping the one byte that read exactly 2 to 9. Step 1 took the
`0x4ec1` entry out of the profile and pointed `mission` at `0xc04c`; the
three tapes below were re-recorded under the corrected profile.

### The state model on the whole map

`state.py`'s `MAP_BASE = 0x8ddc` with `MAP_INSET = 16` is mission 1's
window into a bigger board. The map really starts at **`0x7d9c`** and is
**64 by 64** tiles of 4 bytes with a `0x100` row stride: `0x8ddc = 0x7d9c +
16 * 0x100 + 16 * 4`. With the true base the inset disappears — a record's
position word carries the game's own tile number.

The radar blip's scale is mission-dependent: **2 pixels a tile in missions
1 and 2, 1 pixel a tile in mission 9**, blip x clamped 240..301 and y
144..205 for tiles 0..61, so `engine.BLIP_A_TILE = 2` is wrong here.

Both tables carry a **type byte at `+51`**, which no v6 code knew: unit 16
is the harvester (`state.HARVESTER_HEALTH = 150` was standing in for it),
25 the sandworm, 7 launcher, 9 tank, 10 siege tank, 11 devastator, 12 sonic
tank; building 2 is the palace, 8 the construction yard, 9 the windtrap, 12
the refinery, 17 the silo, 18 the outpost. Names are the Dune II order
read off the ROM's `[CONSTRUCT]` hitpoints at `0x53180` (buildings read
double the table's number, units read it exactly); the ones a line builds
itself are the ones a step can pin.

A record's `+48` word reads its own index and the map's entity byte reads
that **plus one**, so the map's index times the stride lands one record
late and self-consistently — v6's tables are read that way throughout.

The purse is a per-house array at **`0x4d24`, stride 70**: Harkonnen
`0x4d24`, Atreides `0x4d6a` (v6's `credits`), Ordos `0x4db0`, ending where
`BUILD_BASE = 0x4e26` begins.

### The opening state, by house

Read from the tables at control, 720 frames after the last briefing press.

| | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| purse | 2,500 | 2,500 | 2,000 |
| my yard | (13,29) | (43,43) | (22,42) |
| my units | tank, 2 launchers, | 2 siege tanks, | 3 siege tanks, |
| | 2 devastators, siege | 2 launchers, 2 sonic | 2 deviators, tank, quad |
| my buildings | the yard alone | the yard alone | the yard alone |
| spice tiles | 854 | 733 | 733 |
| sandworms | 2 (owner 3, fremen, 1,000 hp) | 2 | 2 |

The Emperor's base is the top of the map in all three, 30 tiles or more
from the yard:

| Sardaukar | Harkonnen map | Atreides map | Ordos map |
|---|---|---|---|
| buildings | 32 | 33 | 32 |
| rocket turrets | 10 | 11 | 10 |
| windtraps | 8 | 7 | 8 |
| heavy factories | 3 | 3 | 3 |
| refineries | 2 | 2 | 2 |
| outposts | 2 | 2 | 2 |
| palace | 1 | 1 | 1 |
| units | 22 | 22 | 21 |
| siege tanks | 9 | 9 | 7 |
| launchers | 6 | 6 | 10 |
| devastators | 0 | 3 | 2 |
| harvesters | 2 | 2 | 2 |

### The won word and the lost word

**Defeat is a full-screen plate**, a vulture over a wrecked gun, perceptual
hash `74e14b0e1e12f2ec` (it animates between that and `74e14b0e1c1af2ec`,
so `within 6` catches it). It comes up **when the last building falls**,
not when the last unit does: an Atreides mission 9 left to itself lost its
construction yard between frames 31,200 and 32,400 and the plate followed
at once, with six units of mine still alive on the map. `buildings`
(`0xdbee`) reading 0 is the game's own word for it.

The **won** word for mission 9 is not proved here. v6's victory plate
`30b14b4e6e34b8cb` is a mission-complete plate for missions 1 and 2, and
the last mission may go to the ending sequence instead; no line has won
mission 9 yet, and DUNEFINALE is not a way in.

### What differs from missions 1 and 2

| verb | how | cost, measured on Atreides mission 9 |
|---|---|---|
| cursor onto a tile | hold the direction | 12 frames a tile |
| cursor diagonally | hold two directions | 9 tiles on both axes in 120 frames |
| select a unit | A on its tile | 46 frames, `picked` reads the tile |
| move order | select, cursor to the target, A | 131 frames over 6 tiles |
| a unit walking | — | about 128 frames a tile |
| attack order | select, cursor on the enemy, A | 1,447 frames, two orders |
| open the yard's list | A, A on the yard | part of the 1,700 below |
| one build order placed | list walk, ghost, A | 1,686 to 1,976 frames |

- **The build list starts at two items.** With only a construction yard
  standing, mission 9 offers concrete ($15, gear 40) and the windtrap
  ($300, gear 400) and nothing else, for all three houses — the same two
  mission 1 offers. Every heavier thing, the palace and its house weapon
  included, is behind the chain of prerequisites, so the last level is a
  full build-up and not a fight the opening force can start.
- **The enemy attacks.** Nothing in `player.py` expects to be attacked: the
  Sardaukar reached and killed an idle construction yard in about 32,000
  frames.
- **Attacking works but arriving does not.** Two heavy units ordered onto
  the nearest enemy building 32 tiles away both died on the way and the
  target never lost a hit point; the v6 finding "route B never took a hit
  point off them" is about arrival, not about the verb.
- **Slot reuse.** A dead unit's record slot is taken by the next unit
  delivered, so a working set keyed by slot silently changes unit.
- **The order cost dominates.** One placed build is about 1,700 frames,
  almost all of it the cursor walking one axis at a time and the list
  being tapped through; a diagonal hold and a list walk that knows the
  seats would cut it.
- **Repair is not measured.** Row 0 of the list is the EXIT / FIX / STOP
  tab strip, and FIX wants a damaged building to act on; no recon run had
  one standing to fix.

### The first estimate

One straight line, Atreides, from control: six concrete slabs and a
windtrap placed, then the refinery ordered and **paid for** (credits
2,110 to 1,750) but never placed, because the placement ghost does not
appear within 600 frames of the order — a refinery is slower to build than
that. The line then waited.

| | frames from control |
|---|---|
| six slabs and a windtrap placed | 12,516 |
| refinery ordered, no ghost | 13,879 |
| **Defeat** | **42,679** |
| the same map with no input at all, Defeat | about 32,400 |

So the number to beat is a loss at 42,679 frames, and the first thing any
v6b line must do is survive past 32,400.

### What humans do, and what is known outside

speedrun.com runs a Genesis board for this game with an individual-level
category covering all 27 missions. The mission 9 records, all emulated,
all by 0zymandias, timed first game-cursor frame to last:

| mission 9 | record | frames at 59.9227 fps |
|---|---|---|
| Atreides | 11:29 | about 41,300 |
| Harkonnen | 12:21 | about 44,400 |
| Ordos | 16:54 | about 60,800 |

So a human wins Atreides mission 9 in about the same frames the straight
line above takes to lose it. The full-game records are 1:29:06 Atreides,
2:40:32 Harkonnen, 3:07:58 Ordos.

TASVideos has a game page and **no publication and no submission**, so
there is no tool-assisted run of this game to beat. Its resource page is
the outside cross-check for two of the facts above — the mission 9
passwords, and `C04C` as the current level, pokeable — and it carries one
mechanic no run of ours has measured yet: **off-screen units get half the
updates**, with carryalls, harvesters, 'thopters, the Ordos saboteur,
Fremen, Shai-Hulud and the frigate exempt. Keeping our units on screen and
the enemy's off it is a lever a planner can pull.

The passwords cross-check two ways off the web — the TASVideos resource
page and Ledmeister's reference, which agree with each other and with the
ROM table byte for byte — and the cheat words are entered from a paused
game, not from the title, which is why DUNEFINALE did nothing here.

### The tapes

Three tapes play from power on through the password to control and 720
frames past it, one a house. Nothing wrote them by hand: `door.py` drove a
recorded run and the harness turned the pad changes into the tape, cutting
a segment at each of the door's three marks, so each tape is exactly what
the verb did. `tash tape replay --bundle` plays each back frame for frame
and says `watches match`.

| tape | frames | replay hash |
|---|---|---|
| `tapes/harkonnen-to-mission-9.yaml` | 4,330 | `de60a6ae6096dafd` |
| `tapes/atreides-to-mission-9.yaml` | 4,282 | `38684964f93ae565` |
| `tapes/ordos-to-mission-9.yaml` | 4,316 | `3208cff27233cf38` |

The hand-written tapes they replace cost 5,763, 5,635 and 5,606 frames to
the same place, so waiting on the game's words instead of on frame counts
is worth 1,290 to 1,433 frames a house. Each segment's anchor is the
recorder's own exact hash of the frame the mark fell on; the first, a
one-frame `start` segment, is the power-on frame before the door's first
mark.

### The words step 1 pinned, and how each was proved

Every watch `profile.yaml` carries, with the line that settles it. The
three mission-9 openings below are the ones `door.py` reaches; "the
openings" means all three of them, 720 frames after control.

| watch | address | what it is | proof |
|---|---|---|---|
| `credits` | `0x4d6a` | the **Atreides** purse | v6's hunt on 990 credits; step 1 places it as slot 1 of the purse array `0x4d24 + 70 * house`, and at the openings the six slots read 2500/0/0/0/4004/0, 0/2500/0/0/4008/0 and 0/0/2000/0/3998/0 — so this watch reads 0 for Harkonnen and Ordos and a line that reads a purse must read the array |
| `credits_shown` | `0xc05a` | the drawn counter | reads 2,500 / 2,500 / 2,000 at the openings, the figure each house's briefing promises |
| `buildings` | `0xdbee` | my standing buildings | v6's width-1 hunt, 74 candidates to 1; reads 1 at every opening, which is the construction yard alone, and 0 on the frame the Defeat plate comes up |
| `enemy_buildings` | `0xdbec` | the enemy's buildings, **turrets not counted** | reads 22 / 21 / 22 at the openings while the building table holds 32 Sardaukar buildings of which 10 / 11 / 10 are rocket turrets: 32 − 10 = 22, 32 − 11 = 21, 32 − 10 = 22 in all three |
| `cursor_x` / `cursor_y` | `0xbf12` / `0xbf14` | the map cursor in screen pixels, and the sign the player has control | 0 through the title, the password screen and the whole briefing, 121 / 89 on the frame control arrives, in all three houses |
| `radar_x` / `radar_y` | `0xbf4c` / `0xbf4e` | the cursor's blip in the radar box | v6's hold-and-watch; the blip's scale is 1 pixel a tile here, not 2 |
| `mission` | `0xc04c` | the mission number | an eight-password Atreides ladder left one byte reading exactly 2 to 9; it reads 9 for all three houses' last mission, 1 and 2 in the v6 line's two, and the ROM's own pokeable "current level" in the outside references is `C04C` |
| `house` | `0xc274` | the house the player is | 0 / 1 / 2 for Harkonnen / Atreides / Ordos at control, unchanged over 4,800 frames of play and reading 1 in mission 1 through the v6 tape. `0xd304` and `0xd300`, which the reconnaissance named, read **2 for both Atreides and Ordos** at control and fall to 0 for Ordos 1,200 frames later, so they are not the house |
| `panel_row` / `panel_column` | `0xbf8c` / `0xbf8a` | the build list's highlight | v6's 256-byte window diffed across one `down` and one `right` |
| `panel_price` | `0xfe8a` | the highlighted item's price | v6's hunt stepped on 15, 300, 400, 15, 300 |
| `picked_x` / `picked_y` | `0xbf5c` / `0xbf5a` | the selected object's position | pressing A on the yard writes its tile; reads 0 at the openings, which is how a briefing press is known not to have clicked the map |
| `harvester_load` | `0x1e0a` | mission 1's harvester's load | the entity table's base plus 27 strides plus 2; `state.py` now reads a `load` off any harvester record instead |

`0x4ec1` is gone from the profile: it was building record 1's owner byte.

### The state model, as one class

`state.py` is one class, `Snapshot`, and one memory call: the whole 64 KB
of work RAM, sliced afterwards. `Snapshot.of(run)` takes the full board,
`Snapshot.of(run, state.MAP_WINDOW)` the 32 by 32 window missions 1 and 2
play in — the window is the same array inset 16 tiles, so one model serves
every mission and `player.py` reads it with `side=32`.

| field | what it holds |
|---|---|
| `seen` | the map, `seen[y][x]` = terrain, shroud, entity index, flags |
| `side`, `inset` | the window: 64 and 0 in mission 9, 32 and 16 in missions 1 and 2 |
| `mission`, `house`, `control` | `0xc04c`, `0xc274`, `0xbf12` |
| `purses` | all six sides' purses, `0x4d24` stride 70 |
| `units`, `buildings` | slot to record: `slot`, `kind`, `name`, `owner`, `tile`, `health`, `tiles`, and `load` where the kind is a harvester |
| `build_list` | the list's `row`, `column`, `price` and `gear` |
| `owners()` | the map again as owner bytes, 255 where nothing stands |
| `mine(table, owner)`, `tally(table)` | that house's records, and a count by owner and name |

`player.py` reads the class instead of the loose functions it replaces,
and the v6 line is unchanged by it: the same run, `tash run --profile
examples/dune/profile.yaml --scenario examples/dune/player.py`, keeps
**72,580 frames** and passes all eleven of its verdicts, mission 2's plate
at 143,771 as before — and "mission 2 is up" now tests the mission word
rather than a building's owner byte.

A record is live when the map itself points at it: the entity byte under a
tile names the slot, the record's own position word names a tile the map
flags, and its health is above zero. That is what keeps the reserve out —
each house carries a dozen records at tile (255,255) with full health,
waiting to be delivered, and a table scan would count them as an army.

Read from the three tapes, 720 frames after control:

| | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| mission / house / control | 9 / 0 / 121 | 9 / 1 / 121 | 9 / 2 / 121 |
| purse | 2,500 | 2,500 | 2,000 |
| my buildings | 1 const yard | 1 const yard | 1 const yard |
| my units | 6: 2 devastators, 2 launchers, siege tank, tank | 6: 2 launchers, 2 siege tanks, 2 sonic tanks | 7: 2 deviators, 3 siege tanks, tank, quad |
| Sardaukar buildings | 32 | 32 | 32 |
| Sardaukar units | 19 | 20 | 20 |
| worms (Fremen) | 2 | 2 | 2 |
| spice tiles | 854 | 733 | 733 |

The reconnaissance's "33 buildings" for Atreides counted my yard with the
Sardaukar's 32; the Sardaukar hold 32 buildings on all three maps. Its
"21/22 units" counted the two harvesters a Sardaukar refinery had swallowed
by the time this snapshot is taken — a harvester inside a refinery is on no
tile, so the map cannot point at it and the model does not count it.

### The door, and what it costs

`door.enter_password(run, house)` is the whole way in: power on, START,
OPTIONS, five downs to ENTER PASSWORD, the ten letters, END, and the
briefing pressed through to control. Every wait is on a word the game
writes — the menu arrow `0xd70e`, the options row `0xdbfc`, the grid cursor
`0xdbf0`/`0xdbf2`, the typed buffer `0xffaf`, `mission`, then `cursor_x` —
and not one on a frame count. The codes are read out of the cartridge
region — `run.string("cartridge", 0x20c14 + 11 * index, 10)`, the ROM file
mapped read-only beside `system` and `save` — so no password is spelled in
the source. The table's first code is DEMOLITION, Harkonnen's mission 2;
the last mission's three sit at `0x20cfb`, `0x20d06` and `0x20d11`.

| | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| the grid is open | 2,281 | 2,281 | 2,281 |
| the word is typed and taken | 2,574 | 2,582 | 2,612 |
| the word itself | 293 | 301 | 331 |
| control | 3,610 | 3,562 | 3,596 |
| the briefing | 1,036 | 980 | 984 |
| A presses through the briefing | 55 | 52 | 52 |

The briefing cadence is measured, not assumed: a press every 6 frames
never reaches control at all (60 presses, no cursor), every 12 frames takes
52 presses and 980 frames, every 30 takes 29 presses and 1,067, every 66
takes 17 and 1,209. The line presses every 12, and `picked_x` stays 0
throughout, so no press fell through to the map.

### Step 2: the hand's verbs, and what each one waits on

`engine.py` is the hand: seven verbs a planner can cost, each one holding
a direction or a button and letting go on a word the game writes. None of
them waits a frame count, and every one answers what it measured — the
frames it took, the tile it reached, the words it waited on — so the cost
of a plan is the sum of what the verbs already report.

| verb | what it does | the word it waits on |
|---|---|---|
| `move_to(run, tile)` | holds both axes at once and releases each as its own word lands | `radar_x` / `radar_y` |
| `select(run, tile)` | steers there and presses until the game names what it holds | `picked_x` / `picked_y` |
| `order_move(run, tile)` | presses on a tile with a unit held | the unit record's order byte, `+111` |
| `order_attack(run, tile)` | the same press on a target | the same byte |
| `arrived(run, slot)` | waits an order out | the same byte, back to 0 |
| `open_build_list(run)` | selects the yard and presses again | `mode`, `0xf702` |
| `pick(run, row, column)` | walks the highlight onto a seat and buys | `panel_row`, `panel_column`, then `mode` |
| `place(run, item, tile)` | waits the build out, steers the ghost, sets it down | the ghost's two colours, then the map's own cell |
| `let_go(run)`, `close_build_list(run)` | drops what is held; leaves the list by EXIT | `mode` |

#### The cursor under a hold, measured

Held from a standstill on `atreides-9-open`, one direction at a time, the
blip crossing tiles:

| frames held | left / up | right / down |
|---|---|---|
| 1 | 1 tile | 0 |
| 2 | 1 | 0 |
| 4 | 1 | 0 |
| 8 | 1 | 0 |
| 16 | 1 | 0 |
| 32 | 3 | 2 |
| 64 | 6 | 5 |
| 128 | 12 | 11 |
| 256 | 24 | 19 |

Two facts fall out. The steady rate is **32 frames every 3 tiles**, 10.67
frames a tile, the same on either axis and the same with both axes held
for a diagonal — a diagonal tile is free. And the first crossing is an
artefact of where the cursor already stands: left and up cross on the
first held frame, right and down take about 21, which is the same
boundary seen from its two sides. The release costs no tile on the bare
map, so `cost_of(here, there)` is `round(32 / 3 * steps)` over the longer
axis and nothing else. At 256 frames right the cursor has run into the
map's edge, which is why that row falls short.

Measured against that cost, the four corners and back, on all three
houses (`out` is the outward hold, `cost` what the formula predicts):

| from | corner | steps | out | cost | back |
|---|---|---|---|---|---|
| Harkonnen (13,29) | (1,1) | 28 | 299 | 299 | 289 |
| | (62,1) | 49 | 523 | 523 | 513 |
| | (62,62) | 49 | 513 | 523 | 513 |
| | (1,62) | 33 | 342 | 352 | 342 |
| Atreides (43,43) | (1,1) | 42 | 448 | 448 | 438 |
| | (62,1) | 42 | 438 | 448 | 438 |
| | (62,62) | 19 | 203 | 203 | 193 |
| | (1,62) | 42 | 448 | 448 | 438 |
| Ordos (22,42) | (1,1) | 41 | 437 | 437 | 427 |
| | (62,1) | 41 | 427 | 437 | 427 |
| | (62,62) | 40 | 417 | 427 | 417 |
| | (1,62) | 21 | 224 | 224 | 214 |

The cost is exact or one tile over, never under, and every one of those
moves landed on its tile and came home to the yard's.

#### The rest of the verbs, on all three houses

From each house's `-9-open` checkpoint, 720 frames after control:

| | Harkonnen | Atreides | Ordos |
|---|---|---|---|
| yard selected | (13,29), 20 frames, answered 0 | (43,43), 20, answered 0 | (22,42), 20, answered 0 |
| a unit six tiles | slot 36, (11,27)→(17,27), order taken in 77, arrived in 516 | slot 46, (41,41)→(47,41), 76, 390 | slot 56, (21,40)→(27,40), 74, 390 |
| the list opened | 870 frames, the list up 37 after the second press | 869, 36 | 881, 37 |
| a slab picked | row 1 column 0, price 15 | the same | the same |
| the slab laid | (15,29), ghost 77 frames after the order, placed | (43,41), 77 | (22,40), 78 |

The list costs 870 frames rather than the 36 the word itself takes
because the unit the previous verb ordered is still walking past the
yard, and **a press takes the nearest unit before the building under
it** — pressing anywhere over tiles 5, 6 and 7 of a row picks the unit
standing on 6. `select` therefore lets go and presses again, up to three
times; where nothing is in the way the same call costs 124 frames.

Run again from a checkpoint one frame later (`<house>-9-open-1`, made
with `restore`, `step(1)`, `checkpoint`), every corner figure above is
identical to the frame, the yard answers in 20 frames again (24 for
Ordos, whose press landed 4 frames late), the unit arrives 379 to 484
frames on, and the slab lands on the same tile for the same price. The
verbs do not depend on the frame they start on.

#### The words step 2 hunted

| watch | address | what it is | proof |
|---|---|---|---|
| `mode` | `0xf702` | the high byte says what is on screen: 1 the bare map, 2 something held, 4 the yard's list; the low byte blinks with the cursor | it reads 4 in the high byte on exactly the frames the list's chrome fills the bottom strip (4,859 pixels of `32,32,64` against at most 15 on the map), on all three houses and in mission 1 |
| unit `+55` | record | the record's state: 160 where the game holds it, 0 where it is under orders | reads 160 for every idle unit at the openings and 0 from the press to the arrival |
| unit `+111` | record | the order byte: 1 from the press that orders a unit to the frame it stops | the six-tile runs above; it goes to 1 within 74 to 77 frames of the press and back to 0 on the tile ordered, 379 to 516 frames on. Buildings have stride 98 and no such byte |
| map `+3` low three bits | map array | the side whose ground it is, not a concrete bit | the three houses' yards read 40, 41 and 42 — 32 plus the owner — Sardaukar concrete reads 4 and fresh Ordos concrete 10. Testing bit 0 only ever answered for the Atreides, who are side 1 |

`0xbf36` was tried as a placement flag and dropped: it takes 0, 1, 2 and 3
while the ghost is up. `0xffde` was tried as a screen word and dropped: it
reads 4 on about nine frames in ten over the bare map and 0 on the rest.
Both live in the 68000's stack region above `0xfe80`, as `panel_price`
does — good for a value read on the frame it is drawn, no good for a wait.

#### The radar, on a short map

The radar box is 64 pixels square and draws the playable map, which is
inset in the 64-tile board the records number by: mission 9's map is 62
tiles inset one, mission 1's is 32 inset sixteen, drawn two pixels a
tile. So the tile a press acts on is `(blip - left) // scale + origin`,
`scale` = `64 // side` and `origin` the inset the snapshot has not taken
out already — which is where mission 9's "the press acts one tile down
and right" really comes from. Proved on mission 1 with a slab ghost up,
blip pixel by blip pixel: x 252 lays the 2 by 2 slab on (6,21), 254 on
(7,21), 256 on (8,21), 258 on (9,21). Under the ghost the blip only ever
reads even pixels there, so placement is exact; over the bare map it
reads half tiles too.

A held direction bites in 11 frames once the game is running and in 51 on
the first frame the player has control, which is the only place a move
needs a stall guard wider than one tile's worth of frames.

#### The seat words hold the last order's, and what that cost

The yard's list answers `mode` about nine frames before it has drawn
anything: at the frame the word reads 4 the screen is still fading, and
`panel_row` and `panel_column` still hold the seat the **previous** order
walked to. Nineteen frames later the game resets the row to its own EXIT,
and eight frames after that `panel_price` answers 65535 for it.

So a pick that walks from where those words claim to be taps nothing, and
the press that should buy an item lands on EXIT: the list closes, no
credit is spent and no ghost ever comes. Measured from a checkpoint taken
just before the order that first hit it, `m1-pre-4-concrete`: the list
opened reading row 1, column 0 with the price word at 0, a press there
bought nothing, and **24,000 frames — ten times `GHOST_FRAMES` — passed
with no ghost**. Walking the highlight off the seat and back made the
same press buy the slab at its price of 15.

`pick` now walks from EXIT every time, so every step of the walk is a tap
a word has to answer, and it will not press a seat whose price word reads
65535. That is what `seat_walk` is for.

#### When each ghost really arms

Placement arms when the build is paid off, not when the order is taken,
so the wait is as long as the item is dear. Measured from the press, with
the cursor parked on bare sand so the ghost that appears is the refused
one and cannot be confused with the map's own colours:

| item | price | the ghost arms |
|---|---|---|
| concrete | 15 | 186 frames after the press |
| windtrap | 300 | 420 |
| refinery | 400 | 688 |

`GHOST_FRAMES` is 2,400, comfortably over the dearest of them, so the
refinery's build time was never what stopped the line.

#### The whole line, on the step 2 verbs

`tash run --profile examples/dune/profile.yaml --scenario
examples/dune/player.py`: **both missions won, 73,430 kept frames**
(143,767 with the four searches that are restored away), plate
`569943f9f108d9df` — the same plate the v6 line ended on. All sixteen
orders are placed on the same sixteen seats the v6 line chose, and they
cost 12,833 frames against that line's 15,406.

| mark | kept frame | the v6 line | gain |
|---|---|---|---|
| mission 1 answers | 3,927 | 3,927 | — |
| mission 1's base stands | 10,950 | 11,749 | 799 |
| mission 1's quota | 27,842 | 27,986 | 144 |
| mission 1's plate | 28,178 | 28,065 | −113 |
| mission 2 answers | 31,800 | 30,918 | −882 |
| mission 2's base and silos | 41,884 | 43,849 | 1,965 |
| mission 2's quota | 72,977 | 72,300 | −677 |
| mission 2's plate | 73,430 | 72,580 | −850 |

The one row that is squarely worse is the briefing. The v6 line played a
tape between the two missions and `run.play` now refuses a machine past
power on, so **the v6 line as it stands on main cannot reach mission 2 at
all** — run from power on it raises `a tape plays from power on and this
run stands at frame 64,788` at the same call. The briefing is pressed
through instead: Genesis A, 23 presses, ending on the purse the mission
draws for itself, 3,622 frames against the tape's 2,740. Nothing in it
waits on a frame count except the pace between presses, which is the
tape's own.

### Step 3: mission 9's base, and how long it stands

`examples/dune/base.py` is the mission-9 line for any house. It is a
separate file from `player.py` on purpose: the missions 1 and 2 line is a
delivered result and nothing in step 3 touches it. One class, `Base`,
holds the whole program — the opening, the turrets and the hold — and
`play_house(run, house)` runs it from `<house>-9-open`, the checkpoint the
password door left behind. No house is named anywhere in it.

#### The game's own frame counter, and why `run.frames()` cannot answer

`0xc590` is a long the game steps once a frame while a mission is being
played, and a restore rewinds it with the rest of the state. That is the
clock every measure below is in.

`run.frames()` is the harness's counter and never goes back, so on a line
that searches it reads the search's bill rather than the base's: the
Atreides refinery stood at game frame 4,460 from the opening and at
`run.frames()` 66,403 from the same opening. Twenty-four trials were
played out and thrown away in between.

#### The opening, measured

Every frame is counted from the opening — the frame `<house>-9-open`
restores to — and every "stands" is the building's own record in the
table at `0x4e26`, not a picture.

| measure | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| the yard | (43, 43) | (13, 29) | (22, 42) |
| the windtrap stands | 1,992 | 2,745 | 1,786 |
| **the refinery stands** | **4,460** | **11,622** | **4,381** |
| **the first delivery** | **15,860** | **18,822** | **13,081** |
| the first silo stands | 19,217 | 20,595 | 15,707 |
| the outpost stands | 21,365 | 22,881 | 19,974 |
| four silos, the ceiling word at 5,005 | 27,084 | 28,003 | 26,996 |
| the turrets stand | 38,212 | — | 39,468 |
| **5,000 credits** | **never** | **never** | **never** |

The first delivery is the credits word climbing on its own: the harvester
comes with the refinery and harvests without an order, and the purse's
first rise is 11,400, 7,200 and 8,700 frames after the refinery stood.

**5,000 credits is not a harvesting problem, it is a ceiling problem and
then a harvester problem.** `0x4d24 + 70 * house + 4` is the purse's
ceiling: 1,005 with a refinery, 2,005 with a refinery and a silo, 5,005
with four. The opening now reads that word and raises silos until it can
hold the target, and all three houses reach 5,005 before 28,003 frames.
None of them then reaches 5,000 credits, because the harvester is the
first thing the Sardaukar kill: Harkonnen lost four harvesters between
44,697 and 108,862 frames and never held more than about 2,000 credits.

#### The repair verb

The list's row 0 is the tab strip: EXIT at column 0, FIX at column 1,
STOP at column 2. `engine.fix` walks `panel_column` to the FIX seat and
presses A, then waits on `mode` for the list to close. Two words, no
pixels.

Measured from `atreides-9-hurt`, an Atreides base the Sardaukar are
already shooting at:

| question | the answer, measured |
|---|---|
| must the building be selected first? | **yes** |
| the yard's own list, FIX pressed | the hurt outpost stayed at 849, the list stayed open, 618 frames |
| no list open at all | refused, `no list`, 0 frames |
| the building's own panel, FIX pressed | the list closed 19–33 frames after the press; the verb answered in 37–82 frames |
| what it heals | the record's health word at `+66`, 14 points a 30 frames |
| what it costs | 25 credits a 30 frames — 1.8 credits a hit point |
| one whole wound | the outpost 849 → 1,000 in 480 frames for 273 credits |

`engine.open_panel` is what selects it — `select` on the tile, then A
again, and `mode` says the list is up — and `open_build_list` is that
same call on the yard, so the two are one verb with two seats.

A second measurement on a windtrap at 58 of 400 gives the same rate with
the damage running against it: 58 → 100 in 120 frames for 76 credits,
then down again under fire, and the building fell 1,350 frames after the
press. Repair does not outrun a devastator.

#### The placement search's shape

Nothing is placed by looking at a frame. A seat is chosen in four steps
and only the last costs emulator time:

1. **The map says where a building may stand.** `state.seats` walks the
   64 by 64 map array for every rectangle of plain rock (terrain 143)
   that touches concrete or a building of mine with no unit on it;
   `Base.pads` keeps the ones within 8 tiles of the yard. That is 9 to 31
   pads a decision on mission 9.
2. **A plan ranks them, over no frame at all.** Everything but the
   refinery is ranked by the step count to the yard. The refinery is
   ranked by `tash.plan.distances` over a cost grid built from the map
   array — every spice tile a source, sand and spice passable, buildings
   walls — so the first-ranked seat is the one whose harvester has the
   shortest trip.
3. **The top three are played out.** `tash.search` restores the same
   checkpoint for each, and the apply is the whole order: walk my own
   units off the pad, lay the 2 by 2 slabs that pave it, wait on the
   purse for the price, buy it off the list and set the ghost down.
4. **The score is the game's own clock.** A seat that stands is worth
   1,000,000 minus the game frames it took, a seat that never stands is
   worth minus those frames, and the refinery's trial is carried on to
   its first delivery so its score holds the harvester's trip too.

Thirteen decisions a house, three trials each, 39 trials — and the search
is not decoration: it picked the third-ranked windtrap seat for Atreides
(45, 43) over the first (43, 41), and the third-ranked refinery seat
(47, 43) over the nearest one. A score is the difference between 998,008
and 986,132 — eleven thousand game frames between two seats eight tiles
apart.

The slab rule is measured, not guessed. A 2 by 2 slab is refused unless
every tile it covers is plain rock, the slab itself touches concrete or a
building already standing, and no unit stands on it; a slab laid over a
unit leaves a tile no later 2 by 2 can reach, so `lay` waits the pad
quiet and retries rather than failing. 38 slabs went down for the
Atreides base and one was refused.

The build list is read, never counted: `engine.survey` walks every seat
and reads the two words the panel draws — the price and the gear —
and `seat_for` names an item by that pair. On mission 9: concrete
(15, 40), windtrap (300, 400), refinery (400, 900), outpost (400, 1000),
silo (150, 300), wall (50, 140), turret (125, 250), rocket turret
(250, 500). Turrets appear only once the outpost stands, which is why
the opening raises it before the defence.

#### Turrets where the Sardaukar come from

`Base.approach` reads the enemy unit records, not the map and not a
picture: every record whose owner byte is the Sardaukar's, averaged to
one tile, and the approach is the tile six steps from the yard along that
line. `Base.watch_attack` keeps one row a Sardaukar slot — the kind, the
tile it was first seen on, its step count to the yard, and the frame that
step count first fell — and those rows are the record of the first
attack.

The same Atreides base, held twice from the two checkpoints the line
leaves behind: `atreides-9-base` at game frame 27,084, the five buildings
with no turret on them, and `atreides-9-defended` at 38,212, the same
base with three rocket turrets at (40, 40), (39, 45) and (45, 47). Both
holds repair and both send idle units at what comes inside twelve tiles.
Frames are from the opening.

| | before the turrets | after the turrets |
|---|---|---|
| the first Sardaukar moves at | 35,597 | 39,087 |
| what moves | siege tank (27, 12), soldier (22, 12), tank (30, 16) | siege tank (38, 33), soldier (35, 26), trooper (26, 14) |
| the first building falls | 35,597, **the construction yard** | 51,826, the windtrap, the refinery and a silo |
| the harvester falls | 37,398 | 51,826 |
| the base is gone | 46,383 | still standing at 78,274 |
| what is left at the end | nothing | three rocket turrets and two silos |

The turrets do not stop the attack, they buy time: the wave that wiped
the base 19,299 frames after it stood had not finished 40,062 frames
later with three rocket turrets on its approach, and what it kills first
changes — without them the construction yard goes first.

#### The hold, and where each house's base stood

The hold is one loop over the records: add any Sardaukar slot not seen
before, note the frame a slot's step count to the yard first falls, count
what is gone since the last poll, repair the building furthest under the
health its own kind stands at when the purse holds more than 600, and
send every unit that is not a harvester at the nearest Sardaukar inside
twelve tiles. It checkpoints `<house>-9-standing` at the frame the base
held most buildings.

| | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| turrets placed | 3 | 3 | 4 |
| **the first attack moves at** | **39,087** | **42,319** | **44,168** |
| what moves first | siege tank (38, 33), soldier (35, 26), trooper (26, 14) | infantry (25, 22), (29, 18), (35, 12), soldier (27, 36) | harvester (57, 17), devastator (19, 9) at 44,768 |
| the first loss | 41,291, a soldier of mine | 44,697, the harvester | 44,168, the refinery, two silos, the windtrap |
| the first building lost | 51,826, the windtrap | 46,540, the windtrap | 44,168 |
| buildings lost in the hold | 9 | 5 | 10 |
| repairs made | 0 | 47, 4,150 points healed | 0 |
| **at 120,000 frames** | **fell at 102,397** | **stands, four buildings** | **fell at 86,676** |
| what was left | nothing | a rocket turret and three silos | nothing |
| `<house>-9-standing` taken at | 38,487 | 41,570 | 39,468 |

Harkonnen is the one base that is still there at 120,000 frames, and it
is the one house whose purse stayed over 600 long enough to repair: 47
repairs, 4,150 points put back. Atreides and Ordos spent everything on
the fourth silo and the third turret and then had nothing to repair with,
which is the clearest result of the step — **the ceiling word and the
repair verb are the same decision.** Neither base keeps its construction
yard: Harkonnen's is gone by 120,000 and the mission cannot be won from
what is left. What answers a devastator is not a turret, and that is what
step 4 starts from.

#### What the harness made hard

- **A detached `python` job carries a 360,000-frame budget by default.**
  `python_file --detach true` with no `frame_budget` raises
  `tash.BudgetExceeded` on the first `step` past it; a three-house run is
  about 850,000 frames. Nothing in the answer of `python_file`,
  `python_status` or `python_output` names the budget or how much is
  left, so a run that dies there looks like the scenario's own fault.
  Every long job here passes `--frame_budget 0`.
- **Nothing a detached job writes to stderr comes back.**
  `python_output` carries the job's stdout, and its traceback when the
  job itself failed; a `traceback.print_exc()` inside the script goes to
  stderr and is lost. A script that catches its own exceptions has to
  `print(traceback.format_exc())` for the reason to survive.
- **A long `python_file` without `--detach` times out over HTTP** and the
  server answers nothing for as long as the job runs, so the output is
  gone even though the run finished. Everything over a few thousand
  frames is detached and polled with `python_status`.
- **A detached job that raises keeps its globals**, which is the only
  reason the first 184,872-frame Atreides run was not lost: the next
  `python` call read `told` out of the interpreter it left behind.
- **`restore_or_play` is MCP-only**, as step 1 found: a scenario writes
  the try/except around `restore` itself.

### Step 4: the Sardaukar read, and the base that holds longer

`examples/dune/enemy.py` is one class, `Army`, and it is the only thing
in the line that knows what the enemy is. `base.Base` owns one of them
and hands it a snapshot every poll; nothing else reads the Sardaukar's
records. Every frame below is the game's own clock at `0xc590`, counted
from the frame `<house>-9-open` restores to.

#### What one enemy record says, and what it does not

Step 2 pinned the order byte at `+111` of a unit record. Step 4 went
looking for the target words it points at and there are none: no byte
pair anywhere in work ram holds a record's goal as a tile, as a tile
shifted, or as a pixel pair, for any record, on any frame of a walk.
What a record does carry is the next step of the walk, and the game
keeps the goal itself in one global:

| word | what it is, and how it was proved |
|---|---|
| `+126`, `+128` | the tile the record is stepping onto, y then x, centred the way the position words are (`>> 8` gives the tile). Both read 0 on the frames between two steps. Found by stepping one walking trooper a frame at a time and diffing its own 140 bytes: these two are the only ones that change one tile ahead of `+2`/`+4`. |
| `+112` | the distance still to run at the order when it is given, tiles `<< 8`, on the octagonal metric `max + min / 2`; it reads 10 for the rest of the walk. |
| `0xc188` | the goal tile of the last order issued, as `y * 64 + x`, one global for the whole game. `0xc240`, `0xc244` and `0xc248` mirror it. It is not per record: it is overwritten by whoever ordered last, which is why it cannot answer "where is that record going". |

So "where it is going" is answered one step at a time, and `Army` stores
exactly that: `tile` is where the record stands, `step` is the tile it is
stepping onto, and `heading` is their difference. The order byte is kept
beside them; over the three holds it takes 0, 1, 2, 3, 4, 5, 9, 18, 64
and 128, with 0 and 1 (about 60% of records) the two the walk sits in.

#### The army over the three holds

Counted off the records, from the turrets standing to 120,000 frames.

| | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| records opened | 38 | 45 | 42 |
| what they are | siege tank 11, launcher 8, trooper 6, devastator 4, harvester 3, tank 3, troopers 2, soldier 1 | siege tank 13, launcher 7, trooper 5, soldier 5, infantry 5, troopers 4, harvester 3, sonic tank 1, quad 1, tank 1 | launcher 14, siege tank 11, devastator 8, harvester 3, trooper 3, soldier 2, troopers 1 |
| made by | wor 5, heavy factory 3 | barracks 11, wor 2, refinery 1 | refinery 3, wor 2, heavy factory 1 |
| already there at the opening | 30 | 31 | 36 |

"Made by" is the Sardaukar building nearest the tile a record was first
seen on, within six steps, and only buildings that make units are
allowed to be one — a rocket turret two tiles from a spawn is not a
maker, which is what the first pass got wrong. Every record the hold did
not watch appear is charged to "the opening": the Sardaukar start
mission 9 with most of their army already on the map.

#### The waves, and what they cost

A wave is a group of first moves toward my yard: the frame a record's
step count to the yard first falls, grouped when two are within 3,000
frames of each other.

| | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| waves | 5 | 7 | 8 |
| units in them | 11 | 23 | 15 |
| the first leaves at | 40,280 | 44,583 | 57,946 |
| period, mean | 20,247 | 11,541 | 8,549 |
| period, shortest / longest | 6,160 / 48,556 | 6,461 / 18,085 | 3,413 / 27,875 |
| the largest wave | 4, at 121,269 | 8, at 113,830 | 4, at 114,373 |
| where it comes from | wor 4 and heavy factory 10 | barracks 7, heavy factory 13, wor 24 | refinery 4, wor 21, heavy factory 3 and 11 |

The period is not a period. Harkonnen's gaps run 6,461, 18,085, 7,610,
9,395, 17,298, 10,398; Ordos' run 3,837, 27,875, 3,899, 7,863, 9,533,
3,420, 3,413. The waves grow instead: Harkonnen's last is eight units
and it is the one that took the construction yard.

**The measure has a gap and the Ordos row shows it.** A record only
counts as having moved when its step count to my yard falls below what
it was on the poll that first saw it, so a record that spawns already at
its closest never joins a wave. Ordos' first wave is timed at 57,946
and its construction yard was killed at 46,781 — by three records that
were in the table before the hold began and never came closer than they
started. The wave table under-counts the first attack on every map.

#### Which records killed what

`Army` keeps the tile of every Sardaukar record from the previous poll,
so when a building of mine leaves the table it can name what stood
nearest it 600 frames before. The three construction yards:

| house | the yard | fell at | what stood over it (steps away) |
|---|---|---|---|
| Atreides | (43, 43) | 44,462 | launcher 10, trooper 16, siege tank 17 |
| Harkonnen | (13, 29) | 113,830 | troopers 4, infantry 8, launcher 11 |
| Ordos | (22, 42) | 46,781 | tank 3, launcher 10, launcher 11 |

Harkonnen's is the one killed by a wave the table has: wave 7, eight
units, 113,830, out of barracks 7 and heavy factory 13. Atreides' and
Ordos' yards were killed by records the wave table does not hold, for
the reason above.

#### The sandworm

The worm is nobody's unit: kind 25, owner 3, in the same table as every
other record. There are two on the Harkonnen map and two on the Ordos
map, and none at all on the Atreides one.

| | slot 23 | slot 24 |
|---|---|---|
| Harkonnen | (34, 22), in the table from 33,911 to 124,708 | (30, 47), the same |
| Ordos | (21, 29), moved once, to (25, 30) | (40, 30), never moved |

Three of the four never changed tile across a 90,000-frame hold, and the
fourth moved four tiles. On mission 9 the worm is a fixed hazard on the
map, not a thing that hunts: it is a position to keep units off, and the
line treats it as one.

#### What the view is worth: nothing

The claim tested is the off-screen half-update — that a fight the view is
not on resolves at a different rate. The test is the same fight twice
from the same checkpoint, `<house>-9-defended`, with no hand at all: the
cursor is parked on the yard for one run and on the map corner furthest
from the yard for the other, the game's clock is started after the
cursor has finished walking, and the enemy is left to it. 80,000 frames
a side, 224,307 emulator frames for all six.

| | first building lost | | last building lost | | the first enemy step toward the yard | |
|---|---|---|---|---|---|---|
| | view on | view away | view on | view away | view on | view away |
| Atreides | 4,200 | 3,600 | 60,767 | 59,962 | 600 | 1,800 |
| Harkonnen | 7,800 | 8,400 | 22,059 | 35,823 | 6,600 | 6,600 |
| Ordos | 8,400 | 3,600 | 24,588 | 13,290 | 5,400 | 5,400 |

The hold polls every 600 frames, so 600 is the measurement's own
resolution. Two of the three first-loss differences are exactly one
poll, in opposite directions, and two of the three first enemy steps are
the same frame to the poll with the view 40 tiles away. The view is
worth nothing: there is no consistent sign, and there is certainly no
half-rate — if anything Ordos' base died sooner with the view away, the
opposite of what the claim predicts. The number to quote is **0 frames,
± one 600-frame poll**, and the line never moves the view to fight.

The view's own origin is `0xe3be` and `0xe3c0`, in pixels, 32 to a tile,
so the tile in the top left corner is the word over 32 (`0xe3ec` and
`0xe3ee` mirror them). That is how "away" was checked rather than
assumed: with the cursor on the far corner the origin reads (52, 55) for
Harkonnen against (10, 25) with it on the yard.

#### Spending against the ceiling word and the repair verb, as one search

Step 3 ended on the finding that the purse's ceiling word and the FIX
verb are the same decision. Step 4 makes that one candidate: **how many
silos, how many turrets, and the purse the hold will not repair below**,
searched together over checkpoints, scored on the game's clock and on
buildings standing.

    POLICIES = ((0, 2, 300), (1, 3, 300), (2, 4, 300), (2, 4, 600),
                (3, 5, 900), (4, 6, 300), (4, 6, 600), (4, 6, 1200))

Each candidate is applied in full from `<house>-9-base` — the silos
raised, the turrets placed on the approach, then the hold played out with
repair and guard running — and scored `buildings standing × 1,000,000 +
the game frames it lasted`. The trial horizon is 60,000 frames, not
42,000: the first horizon ended before the wave that takes a base down
and the search could not tell a survivor from a corpse.

| candidate | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| (0, 2, 300) | 5 stand | 5 stand | 2 stand |
| (1, 3, 300) | 9 stand | **9 stand** | **3 stand** |
| (2, 4, 300) | 8 stand | 7 stand | wiped at 59,290 |
| (2, 4, 600) | **9 stand** | 7 stand | wiped at 60,057 |
| (3, 5, 900) | 6 stand | 4 stand | wiped at 77,314 |
| (4, 6, 300) | 4 stand | wiped at 69,948 | 1 stands |
| (4, 6, 600) | wiped at 78,476 | wiped at 69,948 | 1 stands |
| (4, 6, 1200) | 5 stand | wiped at 71,955 | 1 stands |

Six turrets is worse than three on every map, and on two of them it is
fatal: the turrets are paid for out of the purse the hold repairs from,
and a base with six turrets and no credits dies faster than a base with
three and a reserve. That is the step's answer to step 3's carry, and it
is a measured one — 24 whole holds played out, 1.4 million emulator
frames of search.

Two defects in the line were exposed by widening the search, and both are
the same kind of mistake — state of the scenario's own that a restore
does not rewind:

- **The yard's build list was cached across a restore.** `survey()` kept
  the list it read and re-read it only when the building count changed, so
  a trial that happened to end on the same count left its list behind;
  the real placement then read "concrete: not on the list" and Harkonnen
  raised no silo and no turret at all while its own trial had scored nine
  buildings standing. `forget_list()` now runs at the head of every trial
  and in `fresh()`.
- **Repair chose by health fraction alone**, so the construction yard —
  the thing the Sardaukar shoot first — waited behind a scratched silo.
  The yard goes first now while it is hurt at all, which is the plan's
  "defence before offence" written into the verb.

#### The hold, measured again

The same program for all three houses, no house named in it, from
`<house>-9-open` to 120,000 game frames.

| | Atreides | Harkonnen | Ordos |
|---|---|---|---|
| the policy the search chose | 2 silos, 4 turrets, 600 floor | 1 silo, 3 turrets, 300 floor | 1 silo, 3 turrets, 300 floor |
| silos raised, ceiling word | 2, 4,005 | 1, 3,005 | 1, 3,005 |
| turrets standing, where | 4: (42,41) (41,39) (41,43) (39,45) | 2: (15,29) (17,28) | 3: (22,40) (24,41) (20,37) |
| the base is defended at | 36,851 | 33,911 | 31,151 |
| **the first attack moves at** | **40,280** | **44,583** | **57,946** (see the gap above) |
| the first loss of anything | 44,462, the yard | 44,583, the harvester | 31,751, the harvester |
| **the first building lost** | **44,462, the construction yard** | **113,830, the construction yard** | **46,781, the construction yard** |
| buildings lost in the hold | 2 | 2 | 9 |
| repairs made, points healed | 66, 6,533 | 59, 4,323 | 65, 5,243 |
| **at 120,000 frames** | **stands, 8 buildings** | **stands, 5 buildings** | **fell at 120,032** |
| with its construction yard | no | no | no |
| `<house>-9-standing` taken at | 39,680, 10 buildings | 33,911, 7 buildings | 31,151, 9 buildings |

Against step 3, where Atreides fell at 102,397, Ordos at 86,676 and
Harkonnen stood on four buildings:

| | step 3 | step 4 |
|---|---|---|
| Atreides | fell at 102,397 | stands, 8 buildings |
| Harkonnen | stands, 4 buildings | stands, 5 buildings |
| Ordos | fell at 86,676 | fell at 120,032 |
| repairs, Atreides / Ordos | 0 / 0 | 66 / 65 |

Every house is better and no house keeps its construction yard. Ordos
still falls: the yard goes at 46,781 to a tank three tiles away and two
launchers, the outpost at 49,619, the windtrap at 56,105, and the two
silos and the last rocket turret between 110,240 and 120,032. What
finishes it is the devastator — eight of them on that map against four
on the Atreides one — and step 3 already measured that repair does not
outrun a devastator. Spending cannot answer it; an answer has to take
the devastator off the map before it arrives, which is offence, and this
step is the defence one.

The search horizon and the repair order decide which house keeps its
yard, and neither is free: with five candidates over a 42,000-frame
horizon and repair by fraction alone, Harkonnen kept its yard to 120,000
and Ordos was wiped at 54,372; with eight candidates over 60,000 and the
yard repaired first, Ordos lasts to 120,032 and Harkonnen's yard goes at
113,830. Both are the same program scored the same way — the game is
chaotic enough that a 600-frame difference in when a repair is pressed
changes which record walks where 60,000 frames later.

#### What the harness made hard

- **`tash.search` rewinds the run but not the scenario.** The run is put
  back where it started and the trials' checkpoints are dropped, but
  every python object the apply touched keeps whatever the trials did to
  it. A player has to cut the trials off its own records (`kept()` and
  `fresh()` here) and, less obviously, invalidate every cache whose key
  is a game fact — the build list cache above cost two whole runs to
  find. A `search` that could call the scenario back on each restore, or
  a documented "your state is yours" note beside the tool, would have
  saved both.
- **While a python job runs, every other tool on the session refuses.**
  `peek` answers `python-12 is running`, and so does `observe`, `look`
  and `memory`. A detached job is therefore write-only: there is no way
  to see what a 40-minute run is doing except the frames it has run, and
  no way to sample the state it is in. The only lever is `python_cancel`,
  which ends it. What is missing is a read-only tool, or a second
  session on the same core, that answers while a job holds the run.
- **A scenario cannot take an unprobed checkpoint.** `run.checkpoint`
  always probes — 2 × 120 frames of restore proof — which is right for a
  checkpoint a reviewer will copy out and wrong for the dozens a search
  takes; `tash.search`'s own are unprobed and there is no way to ask for
  one. The three-house run pays 720 frames for each of its 12 named
  checkpoints, which is small, but a scenario that wanted a scratch state
  of its own per poll could not have one.
- **Nothing a detached job writes to stderr came back** until the harness
  fix on 2026-09-15, and a long `python_file` without `--detach` still
  has to be detached rather than waited on. Both are recorded in step 3's
  section; the first is now fixed.
- **A run that has called `tash.search` cannot be filmed, and neither can
  anything it checkpointed.** `run.line()` answers `None` from the first
  search onward and `run.tape()` refuses with `this run cannot be written
  as one line: 'tash.search/0' was taken on a line this run left`.
  Seeding a fresh run from one of the line's own checkpoints does not
  help: `restore("atreides-9-standing")` leaves the line adrift too
  (`came from outside this run`), because that checkpoint was saved by an
  adrift run. `atreides-9-open`, saved by the door run, which never
  searches, seeds cleanly and answers `line() == 4162`. So the exact gap
  is: **a checkpoint carries a line only if the run that took it had one,
  and a search costs the run its line permanently.** The step's film had
  to be made by replaying the searches' answers — `Base.choose` patched
  to pop the seats the search chose — from `atreides-9-open`, and that is
  a different run from the measured one: without the trials the purse and
  the clock differ, the base is defended at 59,456 instead of 36,851 and
  only one turret stands. A `search` that folded its trials out of the
  line the way a restore does would have filmed the real thing.

### Step 5: the ladder read from one byte, and what the purse refuses

Step 4 left the base standing and the plan's next question open: the
list has more on it than a base, and nothing in the line could say what.
A badge — the two words `panel_price` and `panel_gear` — does not name an
item: on the Harkonnen list `(400, 800)` is drawn twice and `(500,
1000)` is drawn twice more. Step 4's answer would have been to buy a
seat under a restore and read the record it leaves, which costs about
2,000 frames a seat and, when `pick` misfires, names the wrong thing.

#### The byte the cursor carries

The list's cursor carries the item's own id. It was found with a
differential read, not a guess: the fourteen seats of the Harkonnen list
were walked with the cursor settling on each, all 65,536 bytes of work
ram were taken at every seat, and the addresses were kept whose byte
equalled the record kind of every seat step 3 had already named from the
map — windtrap 9, refinery 12, outpost 18, silo 17, wall 14, turret 15,
rocket turret 16. Exactly one address survived:

| | |
|---|---|
| the byte | storage offset `0xbfa0`, which the 68000 reads at `0xbfa1` |
| what it holds | the id of the item the list's cursor sits on |
| its numbering | the same one a building record's kind byte uses |
| on the tabs | EXIT 255, STOP 253, FIX 252 (Harkonnen) or 254 (Atreides, Ordos) |
| on a factory's panel | the unit's id, in the units' own numbering |

`engine.item(run)` is the whole reader. Genesis Plus GX hands over
byte-swapped work ram, so the byte at address R sits at offset R xor 1
and `run.memory("system", 0xbfa0, 1)` is the 68000's `0xbfa1`.

With it a seat is named without buying anything: `Ladder.catalog` opens
the list, walks every seat with the same two-word walk step 3 wrote, and
answers `(price, gear, id)` per seat. One list is 3,300 frames and names
everything on it.

#### The yard's list, at `<house>-9-standing`

Read as words alone, no purchase, no pixels. Rows are the list's own
`panel_row`/`panel_column`.

| seat | price | gear | id | what it is | Harkonnen | Atreides | Ordos |
|---|---|---|---|---|---|---|---|
| (0,0) | 65,535 | 0 | 255 | EXIT | yes | yes | yes |
| (0,1) | 0 | 255 | 252/254 | FIX | 252 | 254 | 254 |
| (0,2) | 0 | 255 | 253 | STOP | yes | yes | yes |
| (1,0) | 15 | 40 | 1 | concrete | yes | yes | yes |
| (1,1) | 300 | 400 | 9 | windtrap | yes | yes | yes |
| (1,2) | 400 | 900 | 12 | refinery | yes | yes | yes |
| (2,0) | 400 | 1,000 | 18 | outpost | yes | yes | yes |
| (2,1) | 150 | 300 | 17 | silo | yes | yes | yes |
| (2,2) | 400 | 800 | 3 | factory | yes | yes | yes |
| (3,0) | 400 | 800 | 7 | WOR | yes | — | — |
| (3,0) | 300 | 600 | 10 | barracks | — | yes | yes |
| (3,1) | 50 | 140 | 14 | wall | yes | yes | yes |
| (3,2) | 125 | 250 | 15 | turret | yes | yes | yes |
| (4,0) | 250 | 500 | 16 | rocket turret | yes | yes | yes |
| (4,1) | 500 | 1,000 | 11 | starport | yes | yes | yes |

Fourteen seats, the same twelve items on every house but one: Harkonnen
is offered WOR where Atreides and Ordos are offered the barracks, at a
different price. Nothing else differs, and the light factory the plan
asked after is not a separate seat: seat (2,2), id 3, leaves a record of
kind 4 on the map (see below).

Once the factory stands the list is sixteen seats, and the three new
ones are the rungs above it:

| seat | price | gear | id | what it is |
|---|---|---|---|---|
| (4,1) | 700 | 1,800 | 13 | repair |
| (4,2) | 500 | 1,000 | 5 | hi-tech |
| (5,0) | 500 | 1,000 | 11 | starport, moved down one seat |

Raising the hi-tech adds nothing to the yard's list: the tech tree's next
rungs (IX id 6, palace id 2) never drew on any list this step reached.

The second word is the item's own strength, not its price doubled: the
refinery's 900 and the silo's 300 are the health those records read on
the map, and the factory's 800 is the 802 its record read the frame it
stood. For units it is the hit points the ROM's `[CONSTRUCT]` block
spells at 0x53180 — quad 130, harvester 150, MCV 150, tank 200, launcher
100, siege tank 300, carryall 100 — and the price is the credits the
purse really loses.

#### The rungs raised, on Harkonnen, from `harkonnen-9-standing`

The climb is a program: read the list, take the first id of `WANTED =
(3, 4, 5, 6, 7, 11, 2)` the list offers and has not been raised, hold the
base repairing and guarding until the purse holds the price, then let the
placement ghost judge the ten best anchors my base owns.

| rung | seat | words | placed at | paid | record kind | health | frames | purse after |
|---|---|---|---|---|---|---|---|---|
| factory | (2,2) | 400 / 800 / id 3 | (14,31) | 391 | 4 | 802 | 5,313 | 934 |
| hi-tech | (4,2) | 500 / 1,000 / id 5 | (17,29) | 516 | 5 | 625 | 7,900 | 64 |

The list's id and the record's kind agree on every item step 3 had
already named, and disagree by one on the factory: the seat whose id is 3
leaves a record of kind 4, and that record's own panel offers the heavy
list — MCV, siege tank, launcher — so what the yard calls 3 and the map
calls 4 is one factory, not two.

#### What each factory offers, read the same way

The factory at (14,31), record kind 4, six seats:

| seat | price | gear | id | unit |
|---|---|---|---|---|
| (1,0) | 200 | 130 | 15 | quad |
| (1,1) | 300 | 150 | 16 | harvester |
| (1,2) | 900 | 150 | 17 | MCV |
| (2,0) | 300 | 200 | 9 | tank |
| (2,1) | 450 | 100 | 7 | launcher |
| (2,2) | 600 | 300 | 10 | siege tank |

The hi-tech at (17,29), record kind 5, one seat: (1,0), 800 credits, 100
hit points, id 0 — the carryall. The Ordos barracks, record kind 10,
two seats: (1,0) 60 / 20 and (1,1) 150 / 0, the trooper and the pair.

Buying off a factory needs no ghost: `Ladder.hire` opens the building's
own panel and presses the seat, and the unit walks out. One harvester,
300 credits, was hired 65 frames after the factory stood.

#### What the ladder could not reach, and the exact refusal

The palace is the top of the list the plan asked for, and the climb never
drew it. The wall is money, and it is measured:

| | |
|---|---|
| purse at `harkonnen-9-standing` | 1,345 credits, ceiling 2,005 with one silo |
| spent on the two rungs | 907 credits |
| income, one harvester under fire | +125 credits per 12,000 game frames |
| still to buy for a palace | WOR 400, starport 500, IX 500, then the palace |
| frames that income needs for 1,400 | about 134,000 game frames |
| what the base does in that time | step 4: Harkonnen loses its construction yard at 113,830 |

So the refusal is not a missing verb: **the ladder above the hi-tech
costs more credits than mission 9's spice pays before the Sardaukar take
the yard that sells them.** With no yard there is no list, and the climb
ends. That is what the last rung's row says in the line's own words —
`{'seat': (3, 0), 'words': (400, 800, 7), 'placed': False, 'why': 'the
list never drew', 'frames': 15530}` — and the run ended at game frame
84,251 with 0 credits and two buildings standing.

Which makes the palace, the house weapon and the won word step 6's
questions, and puts a number on what step 6 has to fix first: mission 9
needs a second refinery and more harvesters before it needs a palace,
because at 125 credits per 12,000 frames nothing above the hi-tech is
buyable at all.

#### Three more seats bought under a restore, to name what they make

Each of these is one `tash.search` candidate: the seat is bought, the
record it leaves is read, its own panel is walked, and the restore throws
the purchase away.

| house | seat | words | record kind | health | paid | its panel |
|---|---|---|---|---|---|---|
| Harkonnen | (3,0) | 400 / 800 / id 7 | 7, WOR | 600 | 390 | trooper 100/45 id 5, troopers 200/0 id 27 |
| Harkonnen | (4,1) | 500 / 1,000 / id 11 | 11, starport | 612 | 489 | nothing the list words can read |
| Ordos | (3,0) | 300 / 600 / id 10 | 10, barracks | 442 | −205, the harvester outearned it | soldier 60/20 id 4, infantry 150/0 id 27 |

The starport is a finding for step 6: its panel opens — the mode word
says a list is up — but `panel_row` and `panel_column` will not move off
(4,1), the id byte keeps reading 11, the starport itself, and the price
word reads garbage that changes every time it is asked (65,535, then
1,016, then 2,897). Whatever the starport draws, it is not drawn with the
words the yard's list uses, and the hand cannot walk it.

#### The devastator before it arrives

Read from the records alone, hands off, from `ordos-9-standing` at
31,151 with the yard at (22,42) and nothing done at all (the base is
gone at 52,391):

| slot | first in the records | where | steps to the yard | first step toward it | what it crossed |
|---|---|---|---|---|---|
| 41 | 31,151 | (17,8) | 39 | never | — |
| 50 | 31,151 | (8,16) | 40 | never | — |
| 29 | 49,751 | (23,40) | 3 | never, gone at 51,791 | — |
| 28 | 49,871 | (20,44) | 4 | never | — |
| 27 | 50,591 | (19,40) | 2 | **51,551** | (19,40) → (20,41) at 51,551 → (20,42) at 52,271 |
| 69 | 50,591 | (20,40) | 4 | — | (20,40) → (19,41) at 52,271 |

Which is the answer, and it is not the one the plan expected: **there is
no approach to read.** The two devastators the map starts with stand 39
and 40 tiles away and never move in 21,000 frames. The ones that reach
the base are not in the records until they are two to four tiles from
it, and the one that walks in crosses a tile every 840 frames — about
2,000 frames of warning, less than one repair cycle. On the Harkonnen
map, the same read over 60,000 frames from `harkonnen-9-standing` holds
no devastator record at all.

With the hand running — repairs and guard orders, which is what step 4's
hold does — the same map put one at (21,38), five tiles out, at 43,686;
that frame is the checkpoint `ordos-9-devastator`, which is what the kill
was measured from.

#### What kills a devastator, and what it costs

Five candidates, each played from `ordos-9-devastator` under its own
restore (`tash.search`), target slot 70 at (21,38) with 370 health, purse
675, scored `1,000,000 if it died − frames − 10,000 a unit lost − 20,000
a building lost`:

| candidate | dead at | orders sent | units lost | buildings lost | frames | score |
|---|---|---|---|---|---|---|
| **nothing — the two rocket turrets alone** | **44,886** | 0 | one trike | none | **1,200** | **988,800** |
| launchers | 44,886 | 0, none exist | one trike | none | 1,200 | 988,800 |
| every unit ordered onto it | 46,770 | 9 | harvester, trike | none | 3,084 | 976,916 |
| every unit, twice | 46,770 | 9 | harvester, trike | none | 3,084 | 976,916 |
| repair only | 46,741 | 0 | harvester, tank, trike | **the construction yard** | 3,055 | 946,945 |

Two rocket turrets kill a devastator in 1,200 game frames for the price
of one trike, and every order the hand gives makes it worse: ordering
nine units onto it takes 2.5 times as long and costs the harvester too,
because the units walk into its gun to reach it. Repairing instead of
guarding loses the yard. The defensive answer step 4 already built is the
best one measured.

Two of the four the plan named were not measurable and the refusal is
exact: **launchers** need a factory, and no factory stands at
`ordos-9-devastator` — the purse there is 675, against 400 for the
factory and 450 for one launcher — and **the house weapon** needs a
palace, which the ladder above never reached.

#### The film

`run.tape()` no longer refuses a searched line. The step's film is the
step's own program run from power on — the password door, the opening
raised with its pad searches, the silos and turrets of step 4's policy,
then the ladder — because a film cannot start from a restore: seeding a
run with `restore("harkonnen-9-ladder")` answers `line() == None` and
`run.tape()` refuses with `this run cannot be written as one line:
'harkonnen-9-ladder' came from outside this run`.

| | |
|---|---|
| the run | 287,820 emulator frames, 74,258 game frames, 240 s |
| the line the tape kept | 74,385 frames |
| the replay | `tash tape replay --bundle … --record _runs --name dune-step5-line` |
| the film | 74,385 frames at 60 fps, 20.7 minutes, audio on, stride 1 |
| the replay's word | `replayed 74385 frames of the 74385 the tape keeps, hash 5e8605bc6c529119, watches match` |
| in the devlog | `_devlog/dune/2026-09-15-dune-step5-line.video.mkv`, `…-line-report.html`, `…-line-run.yaml` |

What it shows: the door typed, the opening raised pad by pad, three
rocket turrets, the factory bought at seat (2,2) and set down at (19,30),
a harvester hired off its panel 65 frames later, and then the hi-tech
refused — `'why': 'no ghost'` — because by 74,258 the construction yard
had gone and a yard is what draws the ghost. It stops there, with the
purse at its 2,005 ceiling and three buildings standing.

The run's own bundle is 287,820 frames because Dune's clock stops while a
panel is up: 23,167 game frames of opening cost 179,352 emulator ones.
The tape's line is folded back to the game's own pace, which is why a
five-minute run films as twenty minutes.

#### What step 5 fixed in the line it inherited

- **`engine.pick` pressed A when the cursor had not arrived.** It refused
  only when the price word read 65,535, so a seat the walk never reached
  bought whatever the cursor sat on: asking for WOR at (3,0) built a
  second factory, and an early trial "named" seat (2,2) a turret. It now
  refuses unless the walk seated, and answers the item id it pressed.
- **`engine.open_panel` presses A twice and the second press can close
  the list it just opened.** The mode word reads a list mode before the
  rows are drawn, so the open cannot be verified from the mode alone, and
  suppressing the second press breaks the opens that need it (tried: the
  list then never drew at all). The line opens until the list really drew
  its rows — four tries, each judged by the seats the walk can read — in
  `Ladder.open_list`. Every list this step read went through it.
- **`Ladder.buy` takes its "what stands" snapshot after the wait, not
  before.** The wait for credits is `Base.hold`, which builds turrets of
  its own; taking the snapshot first credited the purchase with whatever
  the hold had raised.

### Step 6: the income measured, and the palace the Sardaukar nearly took

Step 5 stopped at the hi-tech with a purse that could not reach the next
rung, and the plan's revision set this step's first question: income
before offence. Everything below was measured on mission 9 by the
program, under `tash.search` with a `restored` callback, from the step 4
and step 5 checkpoints.

#### Income, searched from `harkonnen-9-standing`

`harkonnen-9-standing` and not `harkonnen-9-ladder`: the two are the same
base at the same frame, but the ladder checkpoint was taken after step
5's purchases and its purse is 64 against standing's 1,345, so every
candidate would have started by waiting for money it did not have.

Eight candidates, each applied to the same seed and then left hands-off:
the score is the purse at game frame 74,000 plus 1,000,000 if the yard
still stands, so a base that falls can never outrank one that holds.
"Per 12,000" is a single hands-off probe of 12,000 game frames taken at
the frame the candidate finished, and it is marked invalid when the
ceiling word moved under it (a silo or the refinery lost clips the purse
and the sample with it).

| candidate | score | purse | harvesters | spent | ended | stood | yard | per 12,000 |
|---|---|---|---|---|---|---|---|---|
| the hold alone | 1,001,689 | 1,689 | 1 → 0 | 0 | 74,722 | 7 | stands | 660, invalid |
| idle in the list | 1,001,848 | 1,848 | 1 → 1 | 217 | 74,101 | 7 | stands | **655** |
| a second refinery | 576 | 576 | 1 → 1 | 446 | 74,247 | 4 | fell 53,721 | 1,290 |
| two more refineries | 520 | 520 | 1 → 1 | 856 | 74,180 | 3 | fell 69,091 | invalid |
| factory + harvester | 0 | 0 | 1 → 1 | 590 | 74,533 | 3 | fell 66,571 | invalid |
| factory + 2 harvesters | 1,000,947 | 947 | 1 → 2 | 821 | 74,890 | 7 | stands | invalid |
| refinery, factory, harvester | 1,796 | 1,796 | 1 → 1 | 446 | 74,377 | 4 | fell | invalid |
| factory, harvester, refinery | 1,230 | 1,230 | 1 → 2 | 590 | 74,299 | 4 | fell | invalid |

What the Sardaukar take is the same in every candidate and is why the
column on the right is mostly invalid: in the hands-off hold they killed
the harvester at 43,650 and again at 58,293, a siege tank reached the
yard at 61,517 and troopers at 63,023. A refinery ranked at the spice
does double the income — 1,290 credits per 12,000 game frames against
655 — and the base that bought it was down to four buildings by 74,000.

**The income the line can hold is 655 credits per 12,000 game frames,
with one harvester and no second refinery.** The second refinery is not
refused by the purse but by the defence: it is 400 credits that would
otherwise be four repairs, and the trial that bought it lost the yard
21,000 frames before the trial that did not.

Under that income WOR (400), the starport (500) and the palace (999) are
all bought inside one base's life — measured below, the last of them at
game frame 66,867. **IX is never buyable, because it never draws**: id 6
appeared on no list this step read, on any house, at any rung. In this
Genesis port the palace's own prerequisite is the starport.

#### The seat the starport opens

`Ladder.tree` searched four climb orders from `harkonnen-9-standing`
(176,469 frames, 130.7 s); the best, `(7, 11, 6, 2)`, scored 2,001,015 by
raising two rungs for real:

| rung | seat | price | gear | id | where it stood | health |
|---|---|---|---|---|---|---|
| WOR | (3,0) | 400 | 800 | 7 | on its own slabs | 800 of 800 |
| starport | (4,1) | 500 | 1,000 | 11 | (15,31) | 1,000 of 1,000 |

With the starport standing the yard's list is still fourteen seats, and
the starport's own seat is gone — replaced, at the same (4,1), by

| seat | price | gear | id | what it is |
|---|---|---|---|---|
| (4,1) | 999 | 2,000 | 2 | **palace** |

which is the first time the palace has drawn on any list in this plan.
`harkonnen-9-starport` is that state: game frame 48,771, purse 772,
ceiling 2,005, eight buildings.

#### The wall is not the purse, it is the yard

The palace is 999 and the purse at `harkonnen-9-starport` is 772, so the
line has to save 227 credits. Hands-off, from that checkpoint, it does —
and it is too late:

| game frame | purse | what stands |
|---|---|---|
| 48,771 | 772 | 8, yard included |
| 51,771 | 767 | 8 |
| **52,371** | 767 | **7 — the const yard falls** |
| 53,571 | 879 | 7 |
| 54,171 | **977** | 7 |
| 54,771 | 1,082 | 6 — a silo goes, the ceiling clips the purse |

The purse passes the price 1,800 game frames after the only building that
can spend it is gone: with no yard `Ladder.open_list` answers no seats at
all. Turning step 4's repair back on saves the yard and spends the
income instead — repair is 1.8 credits a hit point — and the purse never
passes 800.

What buys the palace is repairing **only** the yard: the FIX tab pressed
on the construction yard alone whenever its health falls under 600, and
nothing else repaired at all. Then

| game frame | what happened | purse | yard |
|---|---|---|---|
| 48,771 | the seed | 772 | 771 |
| 51,878 | the yard mended once | 626 | 288 → mending |
| 59,846 | the palace ordered at seat (4,1) | 1,001 | 410 |
| 62,094 | the build draws on the purse | 613 | 155 |
| 66,499 | the ghost comes up | 0 | 586 |
| **66,867** | **the palace stands at (19,30)** | 0 | 562 |

The palace is paid for as it builds, not at the press: the purse ran from
1,001 to 0 over the 6,653 game frames between the order and the ghost,
and the yard was mended four times inside that window out of the same
purse. The palace stood at 1,223 of its 2,000, because the tiles it took
were the concrete the base already owned plus bare rock — the pad search
of `Ladder.pave` cannot lay a 4×4 of slabs this late and the ghost allows
the placement anyway.

Checkpoints: `harkonnen-9-starport` (48,771), `harkonnen-9-palace`
(66,868), `harkonnen-9-fired` (69,124).

#### The house weapon: two presses and a cursor

The palace draws no list. `engine.open_panel` on its tile never sets the
list word and `Ladder.catalog` answers `[]`, so the weapon has no seat,
no price and no id: it is not bought, it is aimed.

The verb, in the words the line already has:

1. `engine.select(run, palace_tile)` — the cursor onto the palace, A
   held down until the picked word reads its tile.
2. `engine.tap(run, A)` again. The mode word `0xf702` goes to a high byte
   of 2 — the same "something is up" the placement ghost reads — and the
   sidebar draws the weapon's own icon beside the palace's.
3. `engine.move_to(run, target)` — the cursor is now the aim, and it
   crosses the map at the ordinary rate, over shroud and over the enemy
   alike.
4. `engine.tap(run, A)`. The mode's high byte reads 3 and the missile is
   away.

What it does, measured under restore from `harkonnen-9-palace`:

| aimed at | what it took |
|---|---|
| the Sardaukar palace (34,2), 2,000 of 2,000 | two windtraps beside it, 400 → 209 and 400 → 200, inside 756 game frames; the palace itself untouched |
| the nearest heavy unit, a tank at (18,32) | the tank, two soldiers and two infantry gone inside 1,628 frames, one infantry 50 → 20 |

The building numbers are the honest ones: the Sardaukar base is nineteen
tiles from anything of mine and nothing else could have hit it. The unit
numbers are not clean — that cluster was three tiles from my own rocket
turrets, which were firing at it — and they are quoted here only to say
the blast covers a group and not a tile.

**A devastator was never a target to measure.** As Harkonnen on mission
9 the Sardaukar field harvesters, infantry, launchers, siege tanks,
soldiers, a sonic tank, tanks and troopers, and not one devastator at any
frame this step read; step 5's devastator was the one that attacks Ordos.

#### The win, and where the line stops

Mission 9 was not won, by the program or by hand. The exact refusal:
when the palace stands at 66,867 the purse is 0, twenty-two Sardaukar
buildings are standing, and mission 9 ends only when they are gone. The
line has no army — the whole income between 48,771 and 66,867 went into
the starport, the palace and four repairs of the yard — and the palace's
own weapon takes about 200 hit points off one building a firing. The
furthest state is `harkonnen-9-fired`, game frame 69,124: nine buildings
of mine standing, the palace among them, the Sardaukar at twenty-two.

#### The film

`_devlog/dune/2026-09-15-dune-step6-line.video.mkv`, with its report and
run.yaml beside it. The line starts at power on — the door driven, not
restored — runs the opening, the base, the scout, step 4's spending and
the turrets, holds the base to nine buildings, and then spends its last
26,000 game frames failing to read the yard's list. It stops at game
frame 66,190 with four buildings standing, and the checkpoint at the
furthest point it reaches is `harkonnen-9-step6`.

| | |
|---|---|
| the line | 66,317 frames, folded from 279,752 |
| the run | 245.93 s, 1,137.5 fps, D2 |
| the replay | stride 1, audio kept, 66,317 frames in 318.28 s at 208.4 fps, 1280x896 |
| the verdict | "replayed 66317 frames of the 66317 the tape keeps, hash 353be7b6283dca13, watches match" |

The live run's last frame hashes `ec78c5ce3e4d49bb` and the replay's
`353be7b6283dca13`, because they are different frames of different
lengths — the run is 279,752 frames and the line it folds to is 66,317 —
and the picture is the same one: perceptual `6a95aa42bd5a245e` against
`6a95aa42bd5a845e`, two bits apart.

**What the film does not show is the ladder**, and the reason is a gap
this step could not close. Restored from a checkpoint, the same base at
the same frame draws all fourteen seats of the yard's list and the line
buys off it; inside the film's own line, the list opens (the mode word
says so) and the walk reads the tab strip and then at most the first two
items — concrete and the windtrap — before `step_to` cannot move the
cursor another row. Eight opens in a row read the same five seats, and
the first seat's price word reads garbage (64,469 where a settled list
reads 65,535 or 0), so the rows are read before the game has drawn them.
Releasing the pad and dropping what the hand holds before each open
(`run.release(1)` then `engine.let_go`) moved the read from one seat to
five; what is still missing is the game's own word for "the list has
finished drawing". Step 7 has to find that word — a differential read
across the draw, as step 5 found the item id.

#### Where the hand still counts frames

Every wait this step wrote is on a word: the purse for a price, the
map's own cell for a placement, `Base.quiet` on the unit flags of the
tiles around a pad, `Ladder.afford` on the purse and on what still
stands. What remains a count, and why:

| count | what it is | why it is not a word |
|---|---|---|
| `engine.TAP_HOLD` 6, `PAD_GAP` 2 | how long a press is held and let go | measured: at no gap the second press on the yard is lost, at two the list opens |
| `engine.STILL_FRAMES` 30, `STILL_LIMIT` 240 | the ghost's cursor drift after a release | measured: the word drifts one tile up to forty frames after the release |
| `base.HOLD_POLL` 600, `QUIET_POLL` 60, `SEAT_SETTLE` 12 | poll periods, not waits | the thing polled is the game's own state; the period only sets how often it is read |
| `income.PER` 12,000 | the income sample window | it is the measurement itself |

The line's idle share, over the 279,752 frames the film recorded: 58.0%
of them change neither the pad nor the exact picture hash. The longest
still stretch is 671 frames — 11.2 seconds — at frame 248,559, which is
the ladder polling a list that never drew its rows. The base's own hold
is not where the stillness is: Dune's screen animates under it.
