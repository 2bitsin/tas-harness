# examples/columns/

`profile.yaml` runs *Columns* (USA, Europe) on Genesis Plus GX from the
cartridge library in `roms/`; nothing here is committed but the profile. The three
watches on it were found with `tash mem search`, and this is the hunt.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `score` | `0xc81e` | 2 | little | the SCORE panel, counting up one point a frame towards the clear it owes |
| `jewels` | `0xc826` | 2 | little | the JEWELS panel, jewels cleared this game |
| `level` | `0xc82e` | 2 | little | the LEVEL panel, 0 on EASY, 5 on MEDIUM, 10 on HARD |

Little-endian on a 68000 is not a typo. Genesis Plus GX keeps work RAM
byte-swapped against the 68000, so `retro_get_memory_data(SYSTEM)` hands a
buffer in which every 16-bit word reads back the other way round.

Each panel is a 68000 longword followed by a BCD copy the display reads:

| 68000 | buffer, 2 bytes little-endian | holds |
|---|---|---|
| `0xc81c` | `0xc81c` high half, `0xc81e` low half | score |
| `0xc820` | `0xc822` low half | score, BCD |
| `0xc824` | `0xc826` low half | jewels |
| `0xc828` | `0xc82a` low half | jewels, BCD |
| `0xc82c` | `0xc82e` low half | level |
| `0xc830` | `0xc832` low half | level, BCD |

The watches name the low half of each, which is exact below 65536 points;
a run that scores more wants `0xc81c` beside it.

## The hunt

Columns boots to a title, then to an attract mode that teaches the rules
rather than playing a scored game, so the score only moves once a game is
started. Start is three presses -- title, ARCADE on the menu, then the
SELECT LEVEL screen -- and after that holding a direction walks the falling
column into a wall:

    title      run 320; hold start; run 10; release
    menu       run 180; hold start; run 10; release
    level      run 240; hold start; run 10; release
    play       (hold left; run 24; release; hold down; run 60; release;
                run 6; hold right; run 24; release; hold down; run 60;
                release; run 6) x n

`--shot` at the end of a script is what made this readable: the panel says
what the number is, and the number is what the search asks for. Six play
cycles show SCORE 391, nine show 600, so the two together name one address:

    tash mem search --profile examples/columns/profile.yaml \
      --width 2 --endian little \
      --steps "<title><menu><level><play x6>; snapshot; value 391;
               <play x3>; value 600; list 10"

    value 391             frame 1850     1 candidates
    value 600             frame 2390     1 candidates
      system   0x00c81e 600

Jewels the same way at `--width 1`, 9 then 16, which leaves `0xc826` and
`0xc872`; the game over screen, where the panel reads 0, tells them apart.

Level never moves during one EASY game, so it took two: MEDIUM starts at
level 5 and HARD at 10, and one address is 5 in the first and 10 in the
second.

    tash mem search ... --width 1 --steps "<menu>; hold down; run 6;
      release; run 20; hold start; run 10; release; run 200; snapshot;
      value 5; list 60"

Searching with no start pressed at all finds only the attract mode's
timers around `0xc46c`, which is the lesson: a memory search is only as
good as the ground truth on the screen.

## The well

`well.py` reads the board out of one block of work ram, and
`well-after-tape.bin` is the whole 64 KiB region at the checkpoint
`tapes/title-to-game.yaml` leaves, exactly as
`run.memory("system", 0, 0x10000)` answered it.

| what | in the system region | encoding |
|---|---|---|
| a cell | `0xc8c8 + 12*row + 2*lane` | 0 for empty, else the jewel id, 1 to 6 |
| the falling column's lane | `0xc87d` | 0 to 5, left to right |
| the fall | `0x8818` | 2 bytes little-endian, pixels: 151 at the spawn, 16 a row, 0 when none falls |
| its three jewels | `0xc884`, `0xc88e`, `0xc898` | the sprite's tile, top first: `0x16 + 4*(id - 1)` |

Thirteen rows of six cells, top row first: 13 x 12 bytes, each cell a
68000 word whose low byte carries the id and whose high byte is always
zero -- the swap puts the low byte first, so the id is the byte the
address names and the one after it is the padding.

| id | tile | colour |
|---|---|---|
| 1 | `0x16` | red |
| 2 | `0x1a` | blue |
| 3 | `0x1e` | yellow |
| 4 | `0x22` | orange |
| 5 | `0x26` | green |
| 6 | `0x2a` | purple |

The colours are what the frame shows: the six ids sampled at the middle
of their cells in a shot of a full well give (168,0,0), (64,136,232),
(200,168,0), (232,68,0), (0,100,0) and (168,0,168).

A cell holds 0 to 6 only once the board has settled -- 37 dumps taken
after a landing never held anything else. While the game-over animation
fills the well it writes 8 into every cell, row by row, which is what a
dump taken mid-animation shows.

## The hunt for the well

Three differential dumps, from the checkpoint the tape reaches, each one
`run.memory("system", 0, 0x10000)` before and after a single thing:

    run.play("examples/columns/tapes/title-to-game.yaml")
    run.checkpoint("after-tape")
    start = run.memory("system", 0, 0x10000)

Two presses of right, and one byte in `0xc800`..`0xc900` moves:

    0xc87d: 3 -> 5

That is the lane, and it counts lanes rather than pixels. One press of A
from the same checkpoint moves four:

    0xc884: 0x1a -> 0x16
    0xc88b: 0x1c -> 0x18
    0xc898: 0x16 -> 0x1a
    0xc89f: 0x18 -> 0x1c

A rotation turns (2,2,1) into (1,2,2): the top jewel's tile and the
bottom jewel's tile trade places and the middle one does not move. The
three sprites are 10 bytes apart, each carrying its tile at +0 and the
tile of its lower half at +7.

Then the column itself, walked into lane 5 and dropped. Between the two
dumps exactly three bytes of the well block change:

    0xc94a: 0 -> 2
    0xc956: 0 -> 2
    0xc962: 0 -> 1

`0xc94a` is `0xc8c8 + 12*10 + 2*5`: lane 5, rows 10, 11 and 12, holding
the column's own ids from top to bottom. The tape's own two columns sit
the same way, which is what `well-after-tape.bin` decodes to:

    ......
    ......
    ......
    ......
    ......
    ......
    ......
    ......
    ......
    ......
    2..2..
    6..1..
    3..3..
    Falling(x=3, y=0, ids=(2, 2, 1))

The row stride is 12 and not 6 because each cell is a word. Thirteen
rows is the well the screen shows, 16 pixels a row from y 8 to y 216,
and row 0 is where a column spawns: its foot reads 151 at `0x8818`, so
`(pixel - 151) // 16` is the row the foot is in from there on.

That the tiles read from the top down was checked by landing them.
Tiles `0x22 0x1e 0x1e` -> ids (4,3,3) landed as rows 8, 9, 10 holding 4,
3, 3; `0x1a 0x2a 0x1a` -> (2,6,2) landed as 2, 6, 2; `0x22 0x26 0x22` ->
(4,5,4) landed as 4, 5, 4. The three ids are also in a six-byte ring at
`0xc842` that A rotates, which is what the hunt found first, but the
tiles are already in display order and cost no rotation arithmetic.

## No next column

There is no preview to decode. The three jewels drawn to the left of the
well are the column in play: at the checkpoint they are blue, blue, red
and the falling column is (2, 2, 1), and they only change when the next
column spawns.

The search agrees. Over 11 turns, a dump taken while each column was
still falling, no address in the whole 64 KiB holds the ids the next
column turns out to have -- not as ids, not as tiles, at no stride from
1 to 16, in either order; not even one byte holds the next column's top
jewel in all 11 turns. So `Well` carries `rows` and `falling` and
nothing else, and a scenario that wants to plan ahead has to plan with
what is on the board.

## What a clear pays

Measured by playing: read the well, steer a column so its third jewel
lands on two alike, and take the score before and after. No fast drop,
so the number is the clear alone -- a held `down` pays for the rows it
skips, and that would be counted in.

| | level 0 (EASY) | level 5 (MEDIUM) |
|---|---|---|
| one clear of three | 30 | 180 |
| measurements agreeing | 4 | 4 |

The level rises every **35 jewels**. The game keeps the count the next
rise costs at `0xc816`, beside the other counters and in the same
little-endian half-longword: 35 at level 0, 70 at level 1, 105 at level
2. Two observed rises bracket it -- level 1 arrived with jewels going 33
-> 36 and level 2 with 66 -> 69 -> 72, the level flipping on the frame
the count crossed the target.

MEDIUM is the SELECT LEVEL screen with one press of down before start.
It begins at level 5, with 20000 points already on the panel and `0xc816`
reading 50, so its first rise costs 50 jewels and not 35; the EASY number
is the one to plan with.

## What an input costs

From the after-tape checkpoint, one frame at a time, watching the decoded
column: hold the button, step, decode, and see whether the lane or the ids
moved.

| input | frames until the decoded column moves |
|---|---|
| left, held one frame | 1 |
| right, held one frame | 1 |
| A, held one frame | 1 |

Holding longer buys nothing: 1, 2, 3, 4 and 8 frames of the same button all
register on the next frame. Held down, a direction auto-shifts -- the first
move on frame 1, the next on frame 8, then one every two frames until the
wall. A held A rotates once and stops, so a second rotation needs the
release.

Two taps of one frame with one frame between them both register, and so do
the next: three of them walk lane 3 to lane 0, two of them turn (2,2,1) into
(2,1,2). A tap is therefore two frames, which is what `place` uses.

## How fast a column falls on its own

The foot at `0x8818` moves eight pixels at a time, half a row, and 24 of
those steps carry it from the spawn at 151 to the floor at 343. Measured on
an empty well, from the first spawn of a fresh game, with nothing held:

| level | frames per eight-pixel step | frames a row | spawn to the floor | floor to the well taking it |
|---|---|---|---|---|
| 0 (EASY) | 32 | 64 | 768 | 40 |
| 5 (MEDIUM) | 3, 3, 2 repeating | 5.2 | 62 | 39 |
| 10 (HARD) | 1 or 2 | 2.4 | 29 | 39 |

Those three are the SELECT LEVEL screen's own three, reached with zero, one
and two presses of down before start.

The speed steps up with the level, and inside a single fall it steps up
again everywhere except level 0. Playing EASY up through the levels and
measuring the untouched fall of the next column each time the level rose
(the well is not empty by then, so the number of steps differs):

| level | the gaps between eight-pixel steps, first step to last |
|---|---|
| 0 | 32 all the way down |
| 1 | 13 13 11 11 11 10 9 9 9 8 8 8 8 |
| 2 | 9 8 8 8 7 7 6 7 6 5 6 6 5 |
| 3 | 9 8 8 8 7 7 6 7 6 |
| 4 | 7 6 6 5 6 4 5 5 4 4 4 4 4 |
| 5 | 4 3 4 3 3 3 3 3 2 3 3 |
| 6 | 8 7 6 7 6 6 5 6 5 6 5 5 5 4 5 |
| 7 | 4 4 4 3 4 3 3 3 4 3 3 3 3 |
| 8 | 3 2 3 2 2 |

Level 6 is slower than level 5, and the three spawns measured at each level
gave the same gaps every time, so it is the game and not the measurement.
The level-5 row here agrees with the MEDIUM row above, which is the check
that the two routes measure the same thing.

## What a drop costs

`down` held moves the foot eight pixels every frame and cuts the pause
before the well takes the column from 39 frames to 2. From each row the
decoder reports, holding down until the well block changes:

| starting row | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| level 0 | 26 | 24 | 22 | 20 | 18 | 16 | 14 | 12 | 10 | 8 | 6 | 4 | 2 |
| level 5 | 26 | 23 | 21 | 20 | 17 | 15 | 13 | 11 | 9 | 7 | 5 | 3 | 1 |
| level 10 | 26 | 23 | 22 | 19 | 17 | 15 | 13 | 11 | 9 | 7 | 5 | 3 | 1 |

Level 0 is exactly `2 * (12 - row) + 2`. Above it the game's own gravity
lands a step in the same frame now and then, so a drop is never longer and
sometimes a frame or three shorter. 26 frames is the bound a placement
needs from any row at any level.

## When the well has settled

`run.memory_stable("system", 0xc8c8, 156, 30)` over the thirteen rows of the
well block, called on the frame the well takes the column. The window has to
clear the quiet inside a clear: the jewels blink every three frames from
frame 2 to frame 32, then the block sits still for sixteen frames, then the
board collapses. Walking the window over one clear from a checkpoint on the
landing frame, 12 through 16 all stop in that gap and answer the window plus
32; 17 is the first that rides through and answers the window plus 56. 30 is
the window used here, which leaves the same margin again.

| what happened | frames memory_stable answered | the last frame the block moved |
|---|---|---|
| no clear | 30 | none: the merge is one write, on the landing frame |
| one clear of three | 79 to 86 | 49 to 56 |
| a chain of two | 132 to 139 | 102 to 109 |
| a chain of three | 237 | 207 |

From 60 landings of a game played by trying all eighteen placements and
keeping the one that cleared most. Every answer is the window plus the
animation, so the animation is the number minus 30. `timeout_frames` of 400
covers the longest chain seen; the harness's own default is 600.

## placement_test.py -- thirty columns where they are asked

    buildutil test --release        # the suite, skipped without the rom
    TASH_COLUMNS_LEVEL=0 PYTHONPATH=. tash run --profile profile.yaml \
      --scenario placement_test.py

One file, two halves. Under pytest it is two tests, one per level, each
driving the other half as a scenario through the built `tash`; both skip
when the rom share is not mounted, so CI passes without it. Under
`tash run` it plays thirty columns and judges every one.

`place(run, column, rotation)` is the numbers above and nothing else: walk
the falling column to the lane with taps of two frames, rotate it with taps
of A, hold down until the well block changes (26 frames is the bound), and
`memory_stable` for the settle. It answers the well before the drop, the
well on the landing frame and the well after the settle. A placement is
right when the cells that changed between the first two are exactly the
column's three jewels, in the asked lane, in the order they fell.

Walking before rotating is what makes it thirty out of thirty. Rotating
first spends four frames at the top of the well, and at level 5 that is most
of a row: the last placement of the level-5 run landed in lane 2 instead of
the lane 0 it was asked for, because by the time the walk started the column
had fallen below the top of the stack it had to cross, and Columns will not
walk a column through a jewel. Rotation cannot be blocked, so it goes last.
That was the only miss.

Level 0 comes from `tapes/title-to-game.yaml`; level 5 is the same start
scripted with one press of down on SELECT LEVEL. The lane asked for is
whichever is stacked lowest, which is enough to place thirty without topping
out: level 0 ends on 1603 points with 41 jewels cleared, level 5 on 24524
(MEDIUM starts on 20000) with 27, and the tallest lane at the end is 11 of
13. A thirty-first column is not promised.

The level-0 run crosses into level 1 on its way, at 35 jewels, and the
placements after it land the same way: `place` polls the well rather than
counting the frames a fall takes, so the numbers above are bounds it waits
inside and not a schedule it follows.

## The magic column

One placement in the level-0 run landed three jewels of id 7, which the
tables above do not list. That is Columns' flashing column: it clears every
jewel of the colour it lands on, and for the one landing frame the well
block holds 7s where it fell. By the time `memory_stable` answers, they are
gone and the board is back to ids 0 to 6, which is what the well section
says. A decoder that validates cells has to let 7 through on the landing
frame.

## What a search costs

Eighteen placements -- six lanes, three rotations -- scored by the `score`
watch read after `memory_stable`. `tash.search` saves a state, plays each
placement from it, keeps the best and moves on. Fifty decisions on this box,
from the level-0 start:

| depth | wall | a decision | trials | frames | score after 50 |
| --- | --- | --- | --- | --- | --- |
| 1 | 39.1 s | 0.78 s | 900 | 57841 | 6054 |
| 2 | 746.7 s | 14.93 s | 17100 | 1130139 | 7971 |

Depth 2 is 19 times the trials for a third more score: it plays eighteen
replies to each of eighteen placements, 342 trials a decision against 18.
Both run the emulator at about 1500 fps, 25 times real time, so the wall
time is the frames and nothing else -- a placement costs around 64 frames,
and the settle wait is most of it.

Recording the run costs little: depth 1 with `--bundle` took 44.2 s instead
of 39.1. `tash tape replay --bundle <dir>` on that bundle answers `replayed
5920 frames, hash 2d7d2c849405e58e, watches match` -- the tape holds the
fifty placements that were kept, not the nine hundred that were tried.

The loop that measured this was a throwaway and is not in the tree.

## well.py -- the well as six columns of thirteen

    import well
    print(well.decode(run.memory("system", 0, 0x10000)))

`decode` takes the whole system region, because the falling column's
height is at `0x8818` and the board is at `0xc8c8`. It answers a `Well`
whose `rows` are 13 tuples of 6 ids, whose `falling` is the lane, the row
and the three ids (or `None` between columns), and whose `__str__` is six
columns of thirteen characters, top row first, `.` for an empty cell.

`well_test.py` decodes `well-after-tape.bin` and asserts the six cells
the checkpoint's shot shows, the falling column over lane 3, and the
printed shape. It is one of the suites `buildutil.toml` declares, so
`buildutil test` runs it beside the C++ tests.

## Watching a run

    tash run --profile examples/columns/profile.yaml --frames 0 \
      --steps "<title><menu><level><play x14>; run 310" \
      --trace columns.tash

3600 frames of that write 423 watch records for three watches -- 3.9% of
the 10800 samples taken, because a record is only written where the value
moved.

## tapes/title-to-game.yaml -- power on to the first falling column

    tash tape check examples/columns/tapes/title-to-game.yaml
    tash run --profile examples/columns/profile.yaml \
      --tape examples/columns/tapes/title-to-game.yaml \
      --frames 600 --bundle _runs

Five segments: the title, the menu with ARCADE under the arrow, SELECT
LEVEL, the playfield, and the first column on the floor.

| segment | anchor | waits |
|---|---|---|
| `title` | perceptual hash `4396eb43383c47c5` within 4 | 320 frames |
| `menu-arcade` | template `menu.png` in (109,139,102,50) | 0 |
| `select-level` | template `select-level.png` in (18,42,122,88) | 60 |
| `playfield` | template `playfield.png` in (14,94,52,68) | 293 |
| `first-column` | watch `score` greater than 0 | 0 |

The title's SEGA banner animates, so its exact hash moves every frame and
its perceptual hash does not; the three template crops are the parts of
each screen that hold still. `menu.png` starts below the SELECT row, whose
colours cycle. `playfield.png` is the empty grid where the SELECT LEVEL
panel used to be, and its region is that panel's own rectangle -- an empty
grid matches an empty grid anywhere, so the region is what makes the
anchor mean "the panel is gone".

`level` equal to 0 does not work as the game-start anchor: work ram is
zero from power on, so it holds on the title screen too. `score` greater
than 0 does work, because Columns pays for a fast drop -- the score only
leaves zero once the first column has been driven down and landed, which
is the exit check in one line.

The crops were cut with tash itself:

    tash run --profile examples/columns/profile.yaml --frames 0 \
      --steps "run 320; hold start; run 10; release; run 60" \
      --shot tapes/title-to-game.anchors/menu.png \
      --shot-region "113,143,94,42"

## scenario.py -- the tape, then six columns

    tash run --profile examples/columns/profile.yaml \
      --scenario examples/columns/scenario.py --bundle _runs

Plays the tape, expects a game is running at level 0 with the first column
already landed, then steers six columns into six lanes and drops each one.
After every burst it calls `observe()`; when the change since the last one
passes `BUSY_CHANGE` it marks the trace and writes a shot, so the bundle
keeps a picture of whatever was worth looking at. It ends judging that the
score rose, and prints the three watches.

`BUSY_CHANGE = 0.07` is measured. A column landing moves 2.3% to 4.2% of
the screen across a burst, the burst that cleared three jewels moved 9.9%,
and a screen that has changed altogether moves 84%; 7% is the gap between
a landing and a clear.

Six bursts is also measured: the pile tops out around the eighth, after
which the score never moves again. The run is deterministic, so the same
plan gives the same board every time.

The bundle it leaves holds `video.mkv`, `trace.bin` with frame, input,
watch, mark and verdict records, `shots/` from the trigger,
`verdicts.jsonl`, and a `run.yaml` whose outcome is `completed`.
