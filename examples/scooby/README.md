# examples/scooby/

Scooby-Doo Mystery (USA), Genesis, `genesis_plus_gx`. A point-and-click
adventure in two scenarios: Blake's Hotel and Ha Ha Carnival. The goal a
plan is written for is both of them from power on, on one tape, in fewer
frames than the human record (speedrun.com Any% both scenarios 27:19,
about 98,340 frames at 60 fps). This page is the reconnaissance: the
words in work ram, the frames each press costs, and what a plan may rest
on.

Everything below is a number from a run. The bundles are in
`_runs/`; `recon.py` measures the table again from power on, and
`tapes/title-to-control.yaml` is the 2,272 frames that reach the first
frame the player has the crosshair.

## The pad

The core's retropad has no `c`, so the three Genesis face buttons are
`y`, `b` and `a` in that order.

| Genesis | retropad | what it does |
|---|---|---|
| A | `y` | dismisses a caption, same as B |
| B | `b` | raises the crosshair, and is the click |
| C | `a` | swaps the bottom bar between the ten verbs and the inventory |
| Start | `start` | the title's confirm; in play, the password screen |

The game has two modes and the d-pad means a different thing in each.
With the crosshair down the d-pad walks Shaggy; with it up the d-pad moves
the crosshair and Shaggy stands still. `b` toggles between them.

## The counters

Genesis Plus GX keeps 68000 work ram byte-swapped: region offset R is
68000 address R xor 1. Every row below is a 68000 word read little-endian
out of the `system` region, which is how the profile's watches read them.

| what | address | width | proof |
|---|---|---|---|
| room | `0x6ac` | 2 | 16 in the lobby, 9 in the hallway, 13 in the cafe, 3 through the arrival cutscene |
| hovered object | `0x6ae` | 2 | 0 over nothing; in the lobby Mailbox 39, Fire 40, Bell 43, Door 44 |
| verb | `0x6b8` | 2 | 0 until a verb is picked, then 1..10 in the rom's own order; copy at `0x6c0` |
| crosshair x | `0x6dc` | 2 signed | walks exactly 2 a frame under a held direction, clamps -8..247 |
| crosshair y | `0x6de` | 2 signed | the same, clamps 0..207 |
| Shaggy x | `0x4dc` | 2 signed | 288 -> 306 under a held right; the fraction is the word after it |
| Shaggy y | `0x4e0` | 2 signed | 176 -> 185 -> 211 under right then down |
| the status line | `0x8bc` | 40 bytes | the string the game draws over the scene |

The verb word's numbering is the rom's, at `0x02FE5F`: 1 Pull, 2 Take,
3 Open, 4 Shut, 5 Use, 6 Give, 7 Push, 8 Talk to, 9 Eat, 10 Look at. All
ten were picked off the bar and read back.

## The hunt

### The status line, and why no picture is ever read

`0x8bc` is the single most useful object in this game's ram. It is a
null-terminated ascii string in the byte-swapped work ram, forty bytes
long, and it holds:

- the name of the hotspot the crosshair rests on, with no verb picked
  (`Radio`, `Double Doors`, `Bottle of Oil`);
- `<Verb> <Object>` once a verb is picked (`Take Bottle of Oil`,
  `Look at Archway`);
- the same for an item in the inventory panel, when the crosshair rests
  on it.

So a whole room's hotspots are read by rastering the crosshair and
reading forty bytes a frame — no ocr, no template, no `look`. Holding
right and reading the line every frame walks a row in 124 frames. The
cafe answered twelve names that way:

    (48,100)  Cash Register    (56,68)   Key
    (80,28)   The Cook         (112,52)  Double Doors
    (144,20)  Lamp             (144,60)  Radio
    (168,76)  Scooby           (192,60)  Archway
    (192,108) Bottle of Oil    (232,100) Cabinet

The spoken answer to an action is **not** in ram: "I didn't know they
still made old radios like this!" is nowhere in the 64 KB after it is
drawn, and the room name banner ("The Lobby", "The Cafe") is not either.
Both are drawn straight into the tilemap. A caption is read from the
frame instead: it is green (32,100,0) over black across the top two rows,
638 pixels of it on the line measured and none once it clears.

### The room word

Narrowed by walking between rooms and keeping what changed and stayed
changed. 22,000 bytes move on a room change, because the game loads about
23 KB of room data, so the filter was a small value that survives a third
room. `0x6ac` is the survivor: lobby 16, hallway 9, cafe 13, and 3 while
the arrival cutscene plays.

### The crosshair, the hovered object and the verb

Held right and held down move `0x6dc`/`0x6de` by exactly two a frame, and
`0x6ae` turns from 0 to a small number the moment the status line fills:
in the lobby Mailbox 39, Fire 40, Bell 43, Door 44, and 0 over the floor.
`0x6b4` is not a copy of it — it read 44 on the Door and 0 on the other
three, so it is something narrower and this session did not settle what.
`0x6b8` was read after clicking each of the ten cells of the verb bar in
turn; every one matched the rom's numbering and the line the game then
drew.

### Shaggy

With the crosshair down, a two-word hunt over a held right and a held
left left `0x4dc` and a sprite table. `0x4dc` is 288 in the lobby, 306
after a held right, and `0x4e0` goes 176 -> 211 under a held down. The
word after each is a fraction: it moves with the integer and wraps.

### The inventory

Taking the bottle of oil in the cafe puts it in the panel — the proof is
the panel drawn empty before the click and with the bottle after it, and
the status line reading `Take Bottle of Oil` when the crosshair rests on
the panel's first cell afterwards.

The table itself was **not** isolated. What the hunt gives:

- 1,588 bytes rise across the pickup and stay risen, in both directions,
  over four alternating restores of the two states — a stable set, but a
  large one;
- of those, `0x3690` is a 36-byte run that is all zero with an empty
  panel and tile codes (16, 17, 18, 33, 34) with one item held: that is
  the drawn panel's tilemap, not a table of item codes;
- a three-way filter against a neutral third state (look at the radio,
  caption dismissed) and a filter across a room change both failed,
  because `0x6200..0x6c00` is scratch the game rewrites every frame and a
  40-frame stability window catches it still by chance.

What a plan can use meanwhile is the status line: switch the bar to the
inventory with `a` and raster the panel; each cell answers its item's
name. The item codes are work still to do.

### The puzzle flags

Not found, for the same reason. The one solved step available without the
route (take the bottle of oil) does not separate from the scratch. What
the game does say is that a refused action is **silent**: `Take Key` in
the cafe rests on a real hotspot, the click is accepted, and nothing
happens at all — no caption, no inventory change. So a plan cannot read
failure from a refusal line; it must read the inventory or the caption.

### The scenario-complete screen

Not reached in this session. See "What is left".

### Nothing free-running

Twelve frames stepped with the pad untouched leave four words moving:
`0x61e` cycles 8,7,6,5,4,3,2,1 (an animation phase), `0x686` and `0x688`
count down, and `0x117a` jumps about — but the sixteen bytes around
`0x117a` are different every frame and are stack scratch, not a seed.
Nothing counts up by frame the way Monopoly's dice word does. See
"Determinism".

## The setup

### Power on to the crosshair

| what | frames from power on |
|---|---|
| the title's menu text is drawn | 1,888 |
| Start opens START / CONTINUE / CANCEL | 1,888 |
| the verb bar is up in The Lobby | 2,272 |

The title offers three entries, PLAY BLAKE'S HOTEL, PLAY HA HA CARNIVAL
and SOUND TEST, with the first already under the cursor. Start opens
START / CONTINUE / CANCEL under the entry chosen, and Start again takes
START. **The order is the player's**: the game offers Blake's Hotel first
and down moves to Ha Ha Carnival, so a tape may take either scenario
first. There is no text speed and no difficulty to set; CONTINUE wants
the password Start shows in play, which is the game's only other option
and a plan does not need it.

### Start skips the arrival cutscene

From the frame START is taken, to the frame the verb bar is up:

| the pad | frames |
|---|---|
| untouched | 13,560 |
| Genesis A mashed, 1 down 1 up | 3,740 |
| Genesis B mashed, 1 down 1 up | 3,738 |
| Genesis C mashed, 1 down 1 up | 12,734 |
| **Start mashed, 2 down 2 up** | **316** |

Start is the cutscene's skip and A and B are only its "next line", which
is why the tape mashes Start and nothing else. Nothing is lost: the same
lobby, the same Shaggy, the same hash.

### The tape

`tapes/title-to-control.yaml`, four segments, 190 transitions, 2,272
frames. The title animates every frame (lightning over the graveyard), so
its anchor is the menu text alone, matched as an image; the last segment
waits on the verb bar's own blue, 1,587 pixels of (96,100,232) in the
bottom strip in every room measured and none in a cutscene.

    tape    title-to-control (4 segments)
    core    genesis_plus_gx
    profile examples/scooby/profile.yaml
      power-on   none                 timeout 3000   0 transitions
      title      template_image title-to-control.anchors/title.png
                 at 0.95              timeout 3000   0 transitions
      start-blakes-hotel none         timeout 3000   190 transitions
      control    colour 96,100,232 within 8 over 0,170,256,42
                 at least 800         timeout 3000   0 transitions

Played into two fresh sessions it lands on the same frame both times,
2,272 frames, and `tape replay` puts the hash on it:

    replayed 2272 frames of the 2272 the tape keeps,
    hash d1d3bf43721a51e0, watches match

### `intro.py`

Build tash in this checkout, then capture either untouched intro:

```text
python examples/scooby/intro.py --scenario hotel --every 60 --frames 16800 OUT
python examples/scooby/intro.py --scenario carnival --every 60 --frames 16800 \
    OUT
```

Use a separate empty output directory for each run. The tool launches the
built CLI with `profile.yaml` and drives its embedded Python API, like
`recon.py`. It powers on and waits for the title tape's image anchor.
Hotel is the first offer; carnival takes **Down held four frames, released
for sixty**, before confirming. Both take exactly two Starts, each held
four frames, with sixty released frames between them. No further button
is pressed. Room values never gate the capture or cause another Start.

Frame zero is the second Start's release, before another emulated frame.
The tool advances M untouched frames, saving `00000.png` and every Nth
frame through M inclusive when divisible by N. A final partial interval
is advanced without saving. `frames.tsv` has one row per PNG, with columns
`frame`, `room`, `hover`, `verb`, `cursor_x`, `cursor_y`, `shaggy_x`,
`shaggy_y`, and `live`: every watch in the profile, with room first.

The 2026-09-16 runs at every 60 through 16,800 prove the selection:
carnival `00000.png` still shows START CARNIVAL / CONTINUE CARNIVAL /
CANCEL, `00300.png` says HA-HA CARNIVAL above the van on the desert road,
and `16800.png` shows The Front Lot with the verb bar and `live = 1`.
The first and last are also in the untracked `_devlog/` as
`2026-09-16-intro-capture-carnival-first.png` and
`2026-09-16-intro-capture-carnival-last.png`.

| scenario | sampled frames | room | live | beat |
|---|---|---|---|---|
| hotel | 0..900 | 16 | 0 | submenu, then snowy title road |
| hotel | 960 | 3 | 0 | transition into the van interior |
| carnival | 0..900 | 12 | 0 | submenu, then desert title road |
| carnival | 960 | 26 | 0 | transition into the van interior |
| carnival | 9600 | 12 | 0 | transition into The Front Lot |
| carnival | 9720..16800 | 12 | 1 | player control at The Front Lot |

These are sampled boundaries, not exact transition frames. In particular,
the arrival room is **12**, but it already reads 12 on the submenu and
first title beat; `live = 1` identifies control.

Hotel `00300.png` is **not pixel-equal** to the hand capture made the
same night with the MCP tools. An every-frame recapture finds the hand
picture exactly at tool frame **360**, a **60-frame offset**: the hand
capture counted from the second press, the tool counts from its release
plus the sixty released frames. The side-by-side proof is
`_devlog/2026-09-16-intro-capture-hotel-300-vs-hand.png`, tool on the
left and hand on the right. PNGs and ROM data are never committed.

### `recon.py`

`tash run --scenario examples/scooby/recon.py` plays the tape and
measures the table again from power on, in 14,142 frames and sixteen
seconds. What it printed:

    tape {'segments': 4, 'frames': 2271, 'waited': 1888,
          'transitions': 190, 'retries': 0}
    rooms {'hallway': (62, 160), 'cafe': (184, 274)}
    cursor held 117 tapped 230 over 115 taps
    caption at (10, 'Look at Lamp', (146, 25)) mashed 34 shown True
            untouched 232
    determinism ['96283e5f9aa261f7', '96283e5f9aa261f7',
                 '96283e5f9aa261f7']
    free running words 4 ['0x4e2', '0x4fa', '0x61e', '0x686']
    ran 14142 frames (2716 kept, probed 6 checkpoints, 0 parted,
        0 unprobed)

and its verdicts:

    a restored state continues as the straight line   120 frames
      compared, hash for hash
    the tape reaches the lobby with the bar up        watch room
      equal 16 holds at frame 2541
    the verb bar is drawn                             passed
    the crosshair is twice as quick held as tapped    passed
    a mashed caption is far cheaper than an untouched one  passed
    the same inputs reach one hash                    passed
    the bottle of oil is taken                        rested on
      'Take Bottle of Oil' at (194, 107)

## The engine

Every number below is from a run, and each says what it was measured on.

### The crosshair

Two pixels a frame under a held direction, flat: no acceleration, no
glide, one pixel of settle on the release. Crossing the scene from x 10
to x 242:

| how | frames | presses |
|---|---|---|
| held right | 117 | 1 |
| tapped right, 1 down 1 up | 230 | 115 |

A hold is exactly twice a tap, because a tap spends a frame up. Ten held
frames move 10 -> 30 and twenty more frames move nothing: there is no
glide to wait out.

The crosshair clamps at x -8..247 and y 0..207. Below about y 167 it
snaps into the verb bar, whose two rows sit at logical y 167..188 and y
191 and below, and whose five columns snap every 40 logical pixels — so a
verb is aimed at from its cell's centre, not its text.

### A click, and the caption it draws

Look at Radio in the cafe, clicked with `b` held six frames:

| what | frames |
|---|---|
| the click to the caption drawn | 28 |
| the caption, untouched, to cleared | 344 |
| the click to cleared, untouched | 372 |
| the click to cleared, `b` mashed 1 down 1 up | **36** |

A caption answers a **press edge and never a hold**: `b` held 600 frames
leaves it up for all 600 and it clears 60 frames after the release. Nor
does one press edge do it — a single two-frame press at the caption's
onset cleared at 343, the same as doing nothing. What clears it is
mashing, and the cadence matters:

| mash, down/up | frames from the click |
|---|---|
| 1/1 | **36** |
| 3/1 | 38 |
| 4/4 | 38 |
| 6/6 | 42 |
| 3/3 | 48 |
| 2/2 | 62 |
| 1/3 | 62 |

Genesis A mashed clears it too (48 at 3/3). Genesis C and Start do not
(366 and 348, which is the untouched cost). So the cheapest line a plan
can write is `b` at 1/1 until the caption's green is gone — and it must
stop the frame it goes, because the next press edge re-runs the action.

The untouched cost is the line's own length, not a constant: Look at
Radio ("I didn't know they still made old radios like this!", two rows)
runs 372 frames from the click and Look at Lamp one row and 232. Mashed,
the two cost 36 and 34 — the mash makes the length stop mattering, which
is most of why it is worth so much.

### A verb click is not always taken

Clicking a cell of the verb bar sometimes leaves `0x6b8` at 0 with the
crosshair sitting on the right cell. Restoring one state and clicking at
fourteen different idle offsets, with nothing else changed: 0, 1, 2, 3,
5, 8, 13, 16, 18, 20, 22, 30, 60 and 120 frames of idle all take the verb
and 14 and 21 do not. There is no threshold, and the two that fail do not
share a phase of the period-8 counter at `0x61e`.

A plan must therefore **read `0x6b8` back and click again**, which is one
line, costs nothing when the first click lands, and is the difference
between a scenario that reads `Look at Lamp` and one that reads `Lamp`
and clicks on nothing. This reconnaissance's own scenario failed three
runs on exactly that before the retry went in.

### A hotspot is a box, not a pixel

Rastered at two pixels, reading the status line:

| hotspot | wide, at one row | tall, at one column |
|---|---|---|
| Radio | 36 px at y 57 | 62 px at x 142 |
| Bottle of Oil | 14 px at y 104 | 44 px at x 188 |
| Cabinet | 14 px at y 100 | -- |

Tens of pixels, so a plan aims at a hotspot's centre and does not need a
pixel. What it does need is to check the line before it clicks: a
`move()` computed at two pixels a frame lands one pixel off often enough
that a one-pixel miss reads an empty line and the click is wasted.

### A room change

Rooms are walked between, not clicked into: with the crosshair down a held
direction walks Shaggy off a screen edge. A door that has not been opened
is not an edge — Open Door in the lobby is what turns the right edge into
one, and that is the game's whole gating mechanism.

| from | direction | to | room word | room drawn | a click accepted |
|---|---|---|---|---|---|
| Lobby (16), door opened | right | Office (17) | 12 | 102 | 200 |
| Lobby (16) | up | Hallway (9) | 64 | 162 | 184 |
| Lobby (16) | down | Cafe (13) | 176 | 268 | 290 |
| Lobby (16), door shut | right | -- | never | -- | -- |

The third column is the number that matters: a new room **refuses the pad
for 80 to 120 frames after it is drawn**. Pressing `b` at 160 frames into
the office does not raise the crosshair and pressing it at 200 does. A
plan that clicks the moment the picture arrives loses the click.

Out of the lobby's arrival spot, up reaches the hallway, down reaches the
cafe, right reaches the office once the door is opened, and left reaches
nothing. Left out of the hallway and right out of the cafe come back to
the lobby. Everything further is behind another door.

### A click walks Shaggy, and the pad is dead until it lands

The reconnaissance wrote that a click never walks Shaggy. That was read
off one cafe action whose hotspot happened to be within arm's reach, and
it is wrong. Open Door on the hallway's leftmost door, clicked with
Shaggy at the far end of the corridor (x 95) and nothing pressed
afterwards:

| what | frame after the click |
|---|---|
| the status line still reads `Open Door` while Shaggy walks | 0..350 |
| the verb bar goes out and the scene starts | 352 |
| the bar is back, and stays back | 1,072 |
| the caption's green is on the strip | 1,084 |

The pad is not read for any of it. The status line is what a program
waits on: `0x8bc` holds the command from the click until the action
fires and empties the moment it does, so the wait is on a word and not
on a count of frames.

Mashing through that window is worse than wasted. A run that mashed `b`
at 1 down 1 up from the click reached a state where the picture, the
room word, the cursor word and Shaggy's word did not change for 1,200
frames with nothing pressed — a wedge no button and no wait recovered.
So: click, then press nothing until the bar drops.

### Skipping the scene an action starts

The same door from the same state, one button mashed at 2 down 2 up from
the frame the bar went out. The bar flickers back up for a sample or two
mid-scene, so the scene counts as over only once it has been up for
twelve samples running:

| mashed | frames to the bar back |
|---|---|
| nothing | 1,072 |
| Start | 1,068 |
| Genesis C | 1,072 |
| Genesis b | **878** |
| Genesis A | **878** |

Start first is what the brief asked to try, and Start is exactly what
does not work here: `b` and Genesis A each cut 194 frames off the door
scene, Start and C cut none. `Cursor.skip()` therefore mashes `b` first.

### The crosshair is a mode, and the mode is one byte

`0x8bc` said which object the crosshair was on but nothing said whether
the pad was moving the crosshair or Shaggy, and the reconnaissance
probed it by pressing a direction — which walks Shaggy out of the room
when the guess is wrong. Dumping `0x400`..`0xc00` of work ram either
side of one Genesis A tap and keeping only the bytes that came back when
A was tapped again leaves four bytes:

| address | crosshair up | crosshair down |
|---|---|---|
| `0x0ab5`, byte | `01` | `00` |
| `0x09dc`, `0x09dd`, bytes | `4c` `4c` | `44` `44` |
| `0x080b`, byte | `00` | `b0` |

`0x0ab5` is the flag: 1 while the crosshair is up, checked against five
saved states in three rooms, and it is a byte, not a word.

Two corrections to the reconnaissance come with it. **Genesis A toggles
the crosshair**, both ways; `b` never puts it away, because `b` is the
click and a click over a hotspot runs that object's default action —
which is what the speedrun guide's bare "Click X" steps mean. And the
byte lies for one frame after a state load and after an action
resolves: it reads 1 while the pad still walks Shaggy. Eight frames of a
held direction settle it, and the honest test is whether `0x6dc` moved,
so `Cursor.confirmed()` holds away, reads the word, and holds back.

### The status line lags the hovered word by eight frames

Resting the crosshair on the hallway's third door, sampling every four
frames: `0x6ae` reads 76 at +4 with the line still empty, and the line
reads `Door` at +8. A program that reads the line the frame after it
stops moving reads the last hotspot's name — which is what made the
reconnaissance's survey call one door "Fire" and another "Scooby".

### A picked verb does not survive a walk about the room

`0x6b8` keeps a picked verb through 720 frames of idle with the
crosshair resting on nothing, and through five long moves that touch no
hotspot. It does not keep it while the crosshair is rested on hotspots:
picked Open, rested on door #3 (`Open Door`), off to empty space, back
to the door, on to Scooby (`Open Scooby`), back to the door — and on the
next hop the word was 0 and the line read `Door`. Three rests is not a
rule and the mechanism is not known. What follows for a program is the
same either way: **confirm the line reads verb and object together in
the frame before the click**, and re-pick when it does not. `act()`
loops verb-then-aim three times.

### Hotspots move with the camera

The hallway scrolls, and a hotspot's screen centre is only true for one
camera. Door #1's centre is (215, 76) with Shaggy at x 95 and (47, 76)
with Shaggy at x 269. Anchoring Shaggy against the room's right clamp
before every step was tried first and is not reliable — he stops short
of it against the banister, and every action walks him somewhere else.

`Cursor.locate(word)` instead holds right along one row and watches
`0x6ae`. One row at y 76 crosses all seven hallway doors, the dumb
waiter and the stairs, and costs about 130 frames. `Cursor.reach(word)`
adds the case where the camera has scrolled the hotspot off: it walks
Shaggy 80 units at a time and sweeps again, and walks him back through
the edge when the room word says he left.

## The route

The ordered actions come from four walkthroughs that agree with each other
(sources at the end); what is verified on the emulator in this session is
marked, and what is not is not.

### Blake's Hotel

The speedrun guide on speedrun.com is a literal ordered list of 116 steps;
the casual walkthroughs come to 78. The shape of it, with the gates:

    1  Lobby        open the right door                  -> the Office
    2  Lobby        open the left door                   -> Outside
    3  Hallway      open six of the seven doors          -> the back door
    4  Gardener's   take antacid, book, bed spring, air freshener
    5  Cafe         push then open the radio, take battery
    6  Cafe         open the cabinet, take can opener
    7  Cafe         eat the antacid                      [the cook leaves]
    8  Cafe         take the key                         [needs 7]
    9  Outside L    take shovel, use it on the snowman, take frozen bell
    10 Outside L    use the key on the shed lock         [needs 8]
    11 Outside L    take crowbar, weed killer, work gloves
    12 Outside R    use the shovel then the crowbar on the doors
    13 Basement     take extension cord and screwdriver
    14 Basement     the stairs come out in the Office    [the shortcut]
    15 Office       take scissors and heater
    16 Hallway      the dumb waiter                      -> the Kitchen
    17 Kitchen      pot, water, chili, open it, eat it   -> empty can
    18 Kitchen      push the fridge, take the soda tab
    19 Kitchen      push the flour, look at the peephole [the note appears]
    20 Kitchen      microwave the frozen bell            -> cow bell
    21 Kitchen      screwdriver on the vent, can on the termites
    22 Lobby        pot of water on the fire             [needs 19]
    23 Lobby        take the crumpled note and LOOK AT IT  [mandatory]
    24 Gardener's   work gloves on the poison oak, take it
    25 Outside      extension cord on the outlet, heater on the cord
    26 Outside      poison oak on the bear               [the bridge falls]
    27 Outside      bed spring, take the Christmas lights
    28 Bridge       the pole three times, take the doll
    29 Bridge       scissors on the rope, take the rope
    30 Lobby        the cow bell, give the doll          -> the goblet
    31 Basement     the crumpled note on the wine rack   -> the Mine
    32 Mine         three wheels on the mine car, ride it
    33 Shaft        hose on the engine, hose on the gas, the switch
    34 Pond         the air freshener
    35 Maze         battery + soda tab + light = flashlight, use it
    36 Maze         up left right left left right
    37 Tomb         weed killer on the lettuce, goblet on the statue
    38 Dungeon      rope on the cuffs, use the rope
    39 Dungeon      termites on Uncle Blake              [Blake freed]
    40 Tomb         give him the book, talk to the statue, answer XYZZY
    41 Tomb         take the medallion
    42 Basement     the medallion on the hook            -> complete

The dependency graph, the part a search has to order:

    antacid -> eaten -> the cook leaves -> the key -> the shed ->
      crowbar, weed killer, work gloves
    crowbar -> the basement -> the mine car -> the shaft -> the maze
    the peephole cutscene -> the crumpled note -> looked at ->
      the wine rack -> the mine
    work gloves -> poison oak -> the bear -> the bridge -> the doll ->
      the goblet -> the statue
    screwdriver -> the vent -> termites -> Uncle Blake
    the book -> Uncle Blake -> the statue -> the medallion -> the hook
    the chili can emptied -> the termites can be taken
    the shaft engine -> the mine car works both ways -> the way back

Two of those are soft locks a plan must not walk into: looking at the
crumpled note (step 23) and eating the antacid before trying the key.

### Ha Ha Carnival

76 steps in the speedrun guide. Its one large skip is the hammer game:
the casual route fetches a coupon from Madame Zelda's slot, plays and
loses, hands the hammer back and plays again; the run simply pulls the
pole. The gates: look at the note in the bottle before the Haunted House
(or Shaggy refuses the bandage and the run is lost), free the manager
before the Front Office and the Dressing Room open, the spark plug before
the boat, the tokens before Madame Zelda, and the roller coaster's brake
on the first frame it is reachable.

### What the emulator verified

The first step of the route, and the machinery the rest runs on.

**Step 1 holds.** In the lobby the status line answers nine hotspots —
Fire (2,48), Zebra (6,88), Stairs (98,40), Archway (120,152), Door
(138,32), Scooby (154,56), Bell (164,72), Picture (172,24), Mailbox
(224,40). With the door shut, a held right walks Shaggy nowhere for 600
frames. `Open` picked off the bar and clicked on `Open Door` at (134,33),
then a held right, reaches room 17 in 12 frames, and room 17's banner
reads The Office. That is the walkthrough's first line, proved, and it is
the shape of every other door in the game.

The hallway at its arrival spot answers The Stairs (38,120), Dumb Waiter
(134,48), Door (204,40) and Scooby — one door of the seven the
walkthrough asks for, so the hallway is wider than a screen; and the Dumb
Waiter the route rides down to the Kitchen is there, where the
walkthrough says it is.

The rest is the machinery: the ten verbs picked off the bar and read back
out of `0x6b8`, twelve hotspots read off the status line in the cafe and
nine in the lobby, three room changes timed, and one pickup — the bottle
of oil, which every walkthrough calls a red herring, chosen because it is
the only item reachable from the tape's landing spot without solving
anything.

### What a scenario is likely to cost

From the measured parts, per action:

| part | frames |
|---|---|
| pick a verb off the bar (move in, click, settle) | 170 |
| move the crosshair to a hotspot across half a scene | 90 |
| click and mash the caption away | 36 |
| a room change, walk and draw | 200 |

An action that changes the verb is about 300 frames and one that keeps it
about 130. Blake's Hotel at 116 steps with, say, half of them changing the
verb, plus forty room changes, is 116 x 215 + 40 x 200 = about 33,000
frames, against the human's 13:47 (49,620 frames). Ha Ha Carnival at 76
steps is about 22,000 against 12:07 (43,620). Both of those leave out the
cutscenes the game will not skip, which the route's own guide marks and
this session did not measure — so the honest reading is that the record
is reachable with room to spare, and that the cutscenes are the number
still missing.

## Step 1: Blake's Hotel driven from the tape

`hotel.py` plays `route.py`'s steps through `engine.py`, from the tape,
naming a checkpoint after every one and measuring each. It resumes from
the newest `hotel-NN`, so a failed step costs the step and not the run.

What one clean run from `control` verified is the first eight of the
guide's 116 steps: the lobby to the hallway, the six doors the guide
opens, and the door into the Gardener's Room. Steps 9 onwards are in
`route.py` as data and are not verified.

### Every step, and what it cost

```
  n room      step                           frames  find  verb  move click scene  capt  dist took
  1 lobby     Goto Hallway                      254     0     0     0     0     0     0     0 ok
  2 hallway   Click Door #1                    1324   213   146    75   344   528     1    39 ok
  3 hallway   Click Door #2                    1391   263   120    76   834    90     1   182 ok
  4 hallway   Click Door #3                    1750   262   120   132   406   798     1   159 ok
  5 hallway   Click Door #7                    4221  2648   120    75   166  1190     1    18 ok
  6 hallway   Click Door #5                    1378   265   120    75   178   722     1   105 ok
  7 hallway   Click Door #6                    1304   265   120    75   202   442     0    88 ok
  8 hallway   Goto Gardener's Room              180     0     0     0     0     0     0     0 ok
```

`find` is `reach()` locating the hotspot's word; `verb` the verb bar;
`move` the crosshair onto the hotspot and the nudges that confirm the
line; `click` the click and the walk it starts; `scene` the cutscene;
`capt` the caption; `dist` the crosshair's manhattan distance from where
the last step left it.

Step 5 is the guide's door #7, at the right end of the hallway, and it
costs 3,059 frames to find because opening door #3 walked Shaggy to the
left end and scrolled #7 off the screen: `reach()` walks him back 80
units at a time and sweeps a row after each.

### Where the frames went

```
kind         frames  share
verb            746   6.3%
move            508   4.3%
click          2130  18.0%
scene          3770  31.9%
caption           5   0.0%
walk            434   3.7%
find           3916  33.2%
total         11802
```

Finding hotspots and sitting out scenes are two thirds of it, and both
are addressable: the scroll word would replace every sweep with one
subtraction, and the scenes are already mashed with the only button that
shortens them.

### The cutscenes

```
  n step                                frames button
  2 Click Door #1                          528 y
  4 Click Door #3                          798 y
  5 Click Door #7                         1190 y
  6 Click Door #5                          722 y
  7 Click Door #6                          442 y
```

`y` is Genesis A. Every one was tried with Start first and Start skips
none of them, as the table two sections up measures. Door #2 drew a
caption instead of a scene, which is why it is not here. None of the
five is unskippable; none is skippable to nothing.

### The last screen this run reached

Room word 14, the Gardener's Room, with the crosshair resting on
`Antenna`:

| hash | |
|---|---|
| exact | `e549d93b59b161c6` |
| difference | `cb6362a70b4e2226` |
| perceptual | `10b4665d17bb4969` |

Two runs a hundred thousand frames apart reached it with the same exact
hash. **This is not the scenario's last screen** — the scenario ends at
the guide's step 115, Use Medallion with Hook, and step 1 did not get
there. What follows Blake's Hotel, the frames to the next thing the pad
can do, and the word that says the scenario is complete are all still
open.

### Against the human

**15,994 frames from power on** — 2,272 of tape and 11,802 of route,
plus the settling between them — for 8 of the guide's 116 steps, against
the human speedrun's 49,620 frames (13:47) for the whole scenario. The
honest reading is not a ratio: it is that the engine's per-step cost is
now measured and what remains is a known amount of per-room
reconnaissance rather than an unknown amount of engine work.

### The bundle, and the replay

`tash run --profile examples/scooby/profile.yaml --scenario
examples/scooby/hotel.py --bundle _runs --name hotel-line` writes the
run and its tape; `tash tape replay --bundle <that> --record _runs
--name hotel-replay` plays the tape back from power on:

```
replayed 14074 frames of the 14074 the tape keeps, hash e549d93b59b161c6, watches match
```

The tape holds 14,074 of the run's 15,994 frames because the frames a
checkpoint probe spends are not on the line. `hotel.py` restores no
state when it is run as a scenario, which is what lets the run be
written as one tape at all: a run that restores a checkpoint taken on a
line it left is refused, with `this run cannot be written as one line`.

### The room graph, as far as it is walked

| room | word | held direction | leads to | gate |
|---|---|---|---|---|
| lobby | 16 | up | hallway (9) | none |
| lobby | 16 | down | cafe (13) | none |
| lobby | 16 | right | office (17) | the lobby door, hover 44, Open |
| hallway | 9 | left | lobby (16) | none |
| hallway | 9 | up | gardener (14) | door #1 open |
| gardener | 14 | down | hallway (9) | none |
| cafe | 13 | right | lobby (16) | none |

A door is not an edge. The hallway's seven doors are hotspots clicked
with Open, and opening one is what gates the edge that leads through it.
Which door leads where is known for #1 only.

### The hallway's doors, by hovered word

Centres are for the camera with Shaggy at x 269, which is the hallway's
right-hand scroll; they are wrong for any other, and `locate()` is what
a program should use.

| guide's name | `0x6ae` | centre | what it gates |
|---|---|---|---|
| door #1 | 74 | (47, 76) | the Gardener's Room (room 14) |
| door #2 | 75 | (69, 68) | not walked |
| door #3 | 76 | (85, 60) | not walked |
| door #4 | 81 | (115, 56) | the guide never opens it |
| door #5 | 77 | (143, 64) | not walked |
| door #6 | 78 | (157, 64) | not walked |
| door #7 | 79 | (183, 76) | not walked |
| Dumb Waiter | 80 | (7, 72) | the kitchen, by the guide |
| The Stairs | 82 | (41, 128) | not walked |

The guide opens them in the order #1, #2, #3, #7, #5, #6 and skips #4.

### The Gardener's Room, rastered

| `0x6ae` | name | centre | box |
|---|---|---|---|
| 2 | Scooby | (87, 104) | (33, 60, 131, 152) |
| 83 | Television | (209, 140) | (169, 120, 243, 152) |
| 84 | Antenna | (199, 88) | (179, 72, 223, 108) |
| 85 | Air Freshener | (195, 124) | (177, 104, 213, 140) |
| 86 | Door | (229, 88) | (209, 56, 243, 124) |
| 87 | Scooby | (63, 80) | (49, 72, 73, 92) |
| 88 | Drawer | (171, 108) | (153, 96, 187, 116) |
| 89 | Poison Oak | (33, 108) | (9, 88, 57, 124) |
| 90 | Bed Spring | (145, 96) | (129, 88, 159, 100) |

The guide's step 9 is "Click both Bedside Tables" and the game's word
for it is `Drawer`, hover 88. Every room will have one of these, and the
survey is the only thing that finds them.

### The flags, the inventory table and the soft locks

Not found. Step 1 spent its emulator time on the interface model — the
mode byte, the click's walk, the scroll — and never got to a solved
puzzle step to diff across. What is now in place to do it cheaply: every
word this game has been caught keeping lives in `0x400`..`0xc00`, which
is two kilobytes, so a diff across a solved step is one `run.memory`
call either side of `act()` and one more across a room change to drop
the per-room scratch. `hotel.py` already checkpoints either side of
every step, so the two states are on disk.

## Determinism

The same inputs from one checkpoint reach the same frame, three times:

    run 1  200d20e29849bcf6
    run 2  200d20e29849bcf6
    run 3  200d20e29849bcf6

(right held 40 frames, 20 at rest, `b` tapped, down held 40 frames, 20 at
rest, from the `lobby` checkpoint.)

No rng word was found. Stepping twelve frames from a checkpoint with the
pad untouched leaves four words moving out of 32,768, and all four read
as an animation phase (`0x61e`, a period of 8), two down-counters
(`0x686`, `0x688`) and stack scratch (`0x117a`, whose neighbourhood is
different every frame). Nothing was found that a press frame could
choose, which is what one expects of an adventure game and is the
opposite of Monopoly's dice.

The tape's own replay is the other half of the answer: two fresh sessions
and one `tape replay` all land on `d1d3bf43721a51e0` at frame 2,272.

## Facts a plan rests on

- The state a plan reads every frame is five words and a string: room
  `0x6ac`, hovered object `0x6ae`, verb `0x6b8`, crosshair `0x6dc`/
  `0x6de`, Shaggy `0x4dc`/`0x4e0`, and the status line at `0x8bc` — forty
  null-terminated bytes in the byte-swapped work ram that say what the
  crosshair is on and which verb is about to run on it. No picture is
  read to play this game.
- The tape reaches the first frame the player has the crosshair in 2,272
  frames from power on, exact `d1d3bf43721a51e0`, room 16 (The Lobby),
  and it plays to the same frame twice.
- Start is the skip for the arrival cutscene, and only for it: mashed
  Start takes that one from 13,560 frames to 316. It skips none of the
  scenes an action starts — on a hallway door, Start saves 4 frames of
  1,072 and `b` or Genesis A save 194.
- The crosshair moves exactly two pixels a frame under a held direction —
  no acceleration, no glide, one pixel of settle — so a hold crosses the
  screen in 117 frames and the same distance tapped costs 230. Every
  crosshair move in a plan is a hold.
- A caption answers a press edge and never a hold. Held 600 frames it
  stays up for 600. Mashed `b` at one frame down and one frame up it goes
  36 frames after the click, against 372 untouched — the single largest
  saving in the game, repeated at every line of dialogue.
- The mash must stop the frame the caption's green leaves the top strip,
  because the next press edge runs the action again.
- A verb click is not always taken. The same click from one restored
  state takes the verb at fourteen idle offsets and drops it at two, with
  no threshold and no phase to it. The plan reads `0x6b8` back and clicks
  again; without that a scenario silently runs every action with no verb.
- A hotspot answers over a box of tens of pixels (Radio 36 by 62), so a
  plan aims at a centre — but it must read `0x8bc` and see the verb and
  the object before it clicks, because a one-pixel miss is a silent
  wasted click.
- A refused action is silent: `Take Key` in the cafe rests on a real
  hotspot and the click does nothing at all, with no caption and no
  inventory change. Success is read from the inventory or the caption,
  never from a refusal line.
- A click walks Shaggy to the target and the pad is dead until the
  action resolves: 350 frames of walking and 1,072 to the bar's return
  on a hallway door. Press nothing in that window — mashing through it
  wedged the game beyond recovery once.
- Walking and the crosshair are separate modes, toggled with **Genesis
  A**, and the byte at `0x0ab5` says which is live. `b` is the click,
  never the toggle, and a click with no verb runs the object's default
  action.
- A hotspot's screen centre is only true for one camera position. Find
  it by its hovered word with a held-right row sweep, about 130 frames.
- Rooms are changed by walking off a screen edge with the crosshair down,
  and a door that has not been opened is not an edge: `Open Door` in the
  lobby is what makes a held right reach the Office, in 12 frames, where
  before it reached nothing in 600.
- A new room refuses the pad for 80 to 120 frames after it is drawn: `b`
  at 160 frames into the Office does not raise the crosshair and `b` at
  200 does. Lobby to Hallway is 64 frames of held up, 162 to the picture
  and 184 to the first click the game takes; Lobby to Cafe is 176, 268
  and 290.
- The scenario order is the player's. The title offers PLAY BLAKE'S HOTEL
  first, down moves to PLAY HA HA CARNIVAL, and there is no text speed or
  difficulty to choose.
- Restores are exact and there is no rng: the same inputs from one
  checkpoint reach one hash three times (`96283e5f9aa261f7`), the restore
  probe reports 120 frames compared hash for hash and 0 parted over six
  checkpoints, and no word in the 64 KB advances by frame with the pad
  untouched.

## What is left

Named so a plan knows what it is buying, not hidden:

- **The camera's scroll word.** The single highest-value hunt left: it
  turns every `locate()` sweep into a subtraction and removes a third of
  step 1's frames. Diff work ram across a walk in the hallway and look
  for a word that moves with the hotspots' screen centres.
- **The inventory table.** Narrowed to 1,588 stable bytes and the drawn
  panel's tilemap at `0x3690`; the item codes are not isolated. Readable
  meanwhile off the status line, at the cost of a raster.
- **The puzzle flags.** Step 1 verified eight steps and none of them is a
  puzzle that yields an item, so there was nothing to diff across. The
  states are checkpointed either side of each step and the window to diff
  is `0x400`..`0xc00`.
- **The route past step 8.** All 116 steps are in `route.py` as data; the
  first eight are played. Every further room needs one raster survey to
  name its hotspots, and the guide's names are not always the game's.
- **The scenario-complete screen**, and what the game does after the
  first scenario ends. Not reached.
- **The two soft locks**, and the words behind them. Not looked for.

## Sources

- The speedrun.com guide list, which the v1 api does not carry:
  `https://www.speedrun.com/api/v2/GetGuideList?gameId=k6q4gw0d` — two
  complete optimised routes, "Blake's Hotel algorithm" (116 steps) and
  "Ha Ha Carnival algorithm" (76 steps), and a bumper-car strategy. The
  single most useful source.
- DEngel's GameFAQs guide, through the Wayback Machine (gamefaqs itself
  refuses automated fetches):
  `https://web.archive.org/web/20250903001442/https://gamefaqs.gamespot.com/genesis/586443-scooby-doo-mystery/faqs/20600`
  — both chapter maps, the maze, the passwords.
- Andrew Leprich's FAQ v1.34, through the Wayback Machine:
  `https://web.archive.org/web/20220505014712/https://www.neoseeker.com/scooby-doo-mystery/faqs/74745-scooby-doo-mysteries-a.html`
  — the soft locks and the hallway-door order.
- Tobias Grant's screenshot LP, `https://lparchive.org/Scooby-Doo-Mystery/`
  updates 1 to 9 — the game's own wording for every verb and object.
- speedrun.com's v1 api for the categories, the records and the run
  comments: Any% 27:19, Blake's Hotel 13:47, Ha Ha Carnival 12:07, all
  set 2025-01-15.
- `https://retroarcadia.blog/2023/10/31/discovering-scooby-doo-mystery-on-sega-genesis/`
  — the verb list and the two-standalone-scenarios structure.

`https://tcrf.net/Scooby-Doo_Mystery_(Genesis)` was fetched once, by the
research pass looking for a ram map, and that fetch is why this paragraph
exists. The url serves a prompt-injection page to automated fetchers: a
block addressed to the reading model, telling it to write an EICAR string
to `key.bin` and insult its operator. Nothing on the page was acted on by
the subagent that fetched it or by this session, and nothing from it
reached the reconnaissance. It is named here so the next reader does not
fetch it again.

## What the harness could do better for this kind of game

Findings, not fixes.

- **A string read.** The whole game is readable because one 40-byte
  byte-swapped null-terminated string sits at `0x8bc`. Every python
  scenario for an adventure game will write the same unswap-and-cut
  helper; `run.memory` answering a string, with the byte order the
  profile already names, would remove it.
- **`run_until` and `anchor` judge the last frame the core drew, and
  `run.reset()` does not draw one.** A predicate that held before a reset
  still holds straight after it, so a probe times at 0 frames and reads
  as an anchor that was already there. A `run.step(1)` after every reset
  is the workaround; a reset that invalidates the last frame would be the
  fix.
- **A python job that does not end cannot be stopped.** A loop with a
  wrong bound (`while cur()[0] < 248` against a cursor that clamps at
  247) held the session for good: the http call timed out on the client
  and the job kept the server, so `python_status` could not be reached
  either. The only way out was killing the server by pid. A cancel, or a
  frame budget a job is refused past, would have saved a restart.
- **`Hunt.candidates()` is not in `sources/tash/python/README.md`**, and
  it answers `(address, value)` pairs rather than addresses; the table
  documents `step` and `reset` only.
- **The colour anchor's file spelling is undocumented.** The tape README
  gives the line spelling (`colour <r,g,b> ...`) and the field names
  `colour`, `max_difference`, `region`, `count`, but not that `colour` is
  a map of `red`, `green`, `blue`. The refusal
  (`missing field: segments/3/anchor/colour/red`) is what says so.
- **`memory_stable` is not a rest test in this game.** The 256 bytes at
  `0x600` never hold still for 30 frames, because the scene's sprite
  scratch is rewritten every frame; Monopoly's "the game is waiting"
  trick has no counterpart here. The verb bar's own colour does the job
  instead, which is a frame read, not a memory read.
- **`tash run` extracts the rom into `/tmp/tash-<hash>-session/`**, not
  under the bundle or a directory the caller names. A run under a rule
  that forbids `/tmp` cannot keep it.
- **The python interpreter caches imported modules across calls.** A
  scenario split into `state.py`, `engine.py`, `route.py` and `hotel.py`
  is edited between runs, and `python_file` re-imports nothing: the
  second run executes the first run's code, silently, until an
  `AttributeError` on a newly added method gives it away. Every file in
  this example is therefore driven through a probe that begins with
  `importlib.reload` on each module. Either evaluating each `python_file`
  in a fresh module namespace, or a documented "the interpreter is
  long-lived, reload your own modules" line in the python README, would
  save the next session the hour it cost this one.
- **`run.shot()` is not on the python `Run` object.** The tool list has
  `shot`, and a scenario that wants the same picture in its bundle finds
  `AttributeError: 'tash.Run' object has no attribute 'shot'`; `look`
  with a path is the workaround, but it writes where the caller says and
  not into the bundle's shots.
- **A state load restores the pad but not the guest's idea of it.** The
  rule "release the pad after every restore" is necessary and not
  sufficient here: this game keeps its own input-mode byte, and after a
  load it reads the mode the state was saved with while the pad still
  drives the other one. One held direction resynchronises it. Nothing
  the harness can fix in general, but worth a line in the python README
  next to the release rule, because the failure is silent and expensive:
  the crosshair sweep walked the character out of the room instead.

## The pad is read, and one byte says so

Every wait in step 1 was a number of frames or the verb bar's colour.
Both are guesses. The byte at `0x630` is not: it is 1 while the pad is
read and 0 through every window the game scripts.

It was found by diffing the whole of work ram across one door click:
two snapshots at rest, six spread over the walk, the scene and the
caption. Fourteen bytes hold one value at rest and another, constant,
through all six; `0x630` is the only one that is a clean 1 and 0.

Traced over that same click at four-frame resolution:

| frame | `0x630` | verb bar up | what is on screen |
|------:|--------:|:-----------:|-------------------|
| 51244 | 1 | yes | at rest, the crosshair on Door #1 |
| 51250 | 0 | yes | Shaggy walks to the door |
| 51406 | 0 | no  | the door's scene |
| 51878 | 0 | yes | the caption |
| 52158 | 1 | yes | at rest again |

The verb bar comes back 280 frames before the pad does. Every wait in
`engine.py` is now `run_until("watch live equal 1")` against the
profile's new `live` watch, and `wait_for_play` is one call instead of
a poll. A room change does not clear it: walking from the hallway into
the lobby, `0x630` stayed 1 and only the room word at `0x6ac` changed,
which is why a walk between rooms costs 180 frames and a click costs
900.

## The cartridge's own tables

The core exposes one memory region. `peek --region rom` answers
`peek: this core has no rom memory; it has system`, so the cartridge
was read from the file the profile names and the addresses below are
file offsets, which for this rom are also 68000 addresses.

- **The string block at `0x14199c`** is every line in the game, packed
  and null terminated: room names, object names, captions, dialogue.
- **The object table at `0x1461f0`** is 8 bytes an object: a long
  offset into the string block for its name, then a long offset to its
  script. It is indexed by the very word the game puts at `0x6ae` while
  the crosshair is over the object, so a hovered word is a name without
  ever reading the status line. 178 objects, ids 5 to 182.
- **The room table at `0x0fd488`** is 20 bytes a room: the name's
  offset, then four more offsets into the room's own data. It is
  indexed by the word at `0x6ac`. Rooms 2 to 22, both scenarios.

The tables were found from the outside in. Six hotspots of the
gardener's room were named by hovering them and reading `0x8bc`; their
names' addresses in the string block are 0, 129, 225, 273, 480 and 571
bytes apart; and exactly one place in the two megabytes holds longs
with those differences at a stride of 8 bytes, at `0x146488`. That is
object 83, so the table's base is `0x146488 - 83 * 8`. The room table
fell out the same way from `The Hallway`, `The Cafe`,
`The Gardener's Room` and `The Lobby`, whose live room words 9, 13, 14
and 16 sit exactly 20 bytes apart per room.

Both tables are in `world.py` as `OBJECT_NAME` and `CART_ROOMS`. They
correct the guide in four places the emulator had not reached:

| the guide's word | the cartridge's word | where |
|---|---|---|
| Door #8 | `Dumb Waiter` (80) | hallway |
| Stairs | `The Stairs` (82) | hallway |
| Bedside Table | `Drawer` (87) and `Drawer` (88) | gardener's room |
| Bedside Tables | two objects, not one | gardener's room |

## Hotspots are read, not surveyed

`Cursor.locate()` swept rows of the room until the hovered word
answered, and that was 33% of every step. It is now a lookup:
`world.HOTSPOTS` holds the box each word answered in, surveyed once and
checked in, and the sweep runs only for a word the table does not have.

The survey is a raster: the crosshair is walked to the left edge of a
row, held right, and `0x6ae` read every frame, for rows four pixels
apart down the scene. The crosshair moves two pixels a frame and stops
at 247, so a box is exact to two pixels across and four down, and a box
that touches 247 is cut off there rather than at the object's edge.

Where a screen's route comes from: the crosshair has no obstacles, so
its route to a hotspot is arithmetic, `(x - at_x) / 2` frames of a held
direction from wherever it is; Shaggy's route is not ours at all. A
click hands the walk to the game's own pathfinder, which is why a click
costs 900 frames and why the pad is dead for all of them. `tash.plan`
is not used for either: there is no grid to search on a screen with no
obstacles, and the room graph in `world.EDGES` wants a graph search,
which `tash.plan` does not have.

## The camera's scroll word, and why there is none to find

The step-1 brief put the camera first. The hunt ran and found nothing,
and the reason is that the rooms do not scroll. Walking Shaggy 83
pixels right in the hallway moved every shared hotspot by between one
and three pixels, which is the survey's own resolution, so the expected
rise was already indistinguishable from zero before any candidate word
was tested.

What does differ is the space each thing is kept in. The crosshair at
`0x6dc` runs -8 to 247 across and 0 to 207 down, which is a 256 pixel
screen: this game runs the Genesis in its 32-cell mode. Shaggy's word
at `0x4dc` reads 274 in the hallway and 288 in the lobby, past the
right of that screen, so it is not the crosshair's space. The word at
`0x4e0` is not his height: it moves while nothing but the crosshair
does. Neither matters for aiming, because the table is in the
crosshair's space.

The hotspot boxes themselves are not in memory as numbers. Work ram was
searched for every measured box as bytes and as words, at every offset,
and the cartridge was searched for them as an id-indexed array at every
stride from 4 to 72 bytes and as three blocks at the offsets the room
table gives. Nothing matched. The room's data block is 0xa6a bytes for
the hallway and 0x744 for the gardener's room, which is the size of a
compressed hit mask, not of eight rectangles; reading it is a
decompression job and not this step's.

## Step 1b: nine steps, and what stopped the tenth

The line is one run from power on: the tape to the lobby, then the
steps. Nothing is restored.

| n | room | step | frames | find | verb | move | click | scene | walk |
|--:|------|------|-------:|-----:|-----:|-----:|------:|------:|-----:|
| 1 | lobby | Goto Hallway | 254 | 0 | 0 | 0 | 0 | 0 | 254 |
| 2 | hallway | Click Door #1 | 1313 | 226 | 120 | 75 | 342 | 528 | 0 |
| 3 | hallway | Click Door #2 | 1381 | 388 | 120 | 79 | 698 | 90 | 0 |
| 4 | hallway | Click Door #3 | 1513 | 89 | 107 | 85 | 398 | 802 | 0 |
| 5 | hallway | Click Door #7 | 5274 | 3229 | 120 | 481 | 178 | 1186 | 0 |
| 6 | hallway | Click Door #5 | 2143 | 676 | 120 | 479 | 130 | 718 | 0 |
| 7 | hallway | Click Door #6 | 1331 | 376 | 120 | 81 | 112 | 442 | 0 |
| 8 | hallway | Goto Gardener's Room | 180 | 0 | 0 | 0 | 0 | 0 | 180 |
| 9 | gardener | Click both Bedside Tables | 1700 | 50 | 84 | 62 | 1214 | 90 | 0 |

Steps 1 to 8 cost 13,389 frames against step 1's 15,802 for the same
eight, a saving of 15% and all of it in `find`: the table answers where
a hotspot is and a row sweep runs once a room visit instead of once a
step. What `find` is left is the hallway, which scrolls between steps
so that the shift has to be relearnt; the gardener's room, which does
not, cost 50 frames of finding for its whole step.

**Step 10 could not be reached, and the guide is why.** "Take Antacid"
finds no hotspot that reads `Antacid`, because step 9's "Click both
Bedside Tables" is two objects in the cartridge and `route.py` carries
one. The gardener's room holds `Drawer` (87) and `Drawer` (88); the
route clicks 88, and the raster taken after it shows a new object,
`Book` (91), inside the opened drawer and no `Antacid` anywhere. The
second drawer is the one that holds it. `route.py` needs step 9 to be
two clicks before step 10 can run, and that is a change to the route's
data and not to the engine.

This is the first case of a hotspot that exists only after a puzzle is
solved, which is why `world.HOTSPOTS` carries a `source` that says
which state the survey was taken in: `Book` (91) is marked
`after the drawers are open`.

The whole line, power on to the ninth step, is 19,521 frames: 4,432 of
tape to the lobby and 15,089 of route. The human's run of the whole
scenario, all 116 steps, is 49,620. Nine steps of 116 have cost 30% of
the human's total, so the line as it stands is not yet a competitor;
the `find` column says why and where the next saving is.

The run is `_runs/2026-09-14T19-22-32Z-hotel-1b`, and its tape replays
clean: `replayed 17361 frames of the 17361 the tape keeps, hash
e0df453f63873aae, watches match`, recorded again as
`_runs/2026-09-14T19-23-08Z-hotel-1b-replay`. The last screen is room
14 with the crosshair at rest: exact `e0df453f63873aae`, difference
`c36362a6074e2226`, perceptual `10b6645d1ff34961`.

More harness findings, with the gap:

- **The core exposes no cartridge.** `peek --region rom` answers
  `peek: this core has no rom memory; it has system`, and the python
  `memory` call has the same one region. Every adventure game keeps its
  world in the cartridge and copies almost none of it into work ram, so
  the run has to read the rom out of the file the profile names and
  work in file offsets while the emulator works in addresses. Genesis
  Plus GX's libretro core does publish `RETRO_MEMORY_SYSTEM_RAM` only,
  but it also exposes the rom through `retro_get_memory` in its own
  build; a `rom` region, read-only, would have saved this step the
  whole detour.
- **`tash.plan` searches grids and not graphs.** `route` and
  `distances` want a cost grid of a width and a height. A room graph is
  what an adventure game is: 21 nodes, 22 edges, a cost on each and a
  precondition on some. Dijkstra over an adjacency list, and a
  precedence-constrained tour over it, is what step 3 needs and what it
  will have to write itself.
- **`run.play(tape)` does not reset, and says so obliquely.** A
  scenario that plays its tape after anything else has run fails with
  `tape: segment 'title' did not see its anchor (template_image
  title-to-control.anchors/title.png at 0.95) within 3000 frames`,
  which reads as a broken tape and is in fact a machine that is past
  the title. `Run.reset()` first fixes it. Either playing a tape from
  the start should reset, or the error should say the run is not at
  power on.

## The geometry is read, not measured

Step 1b rastered every room with the crosshair and wrote the centres down.
Step 1c retired that: the game's own hit test was disassembled and the
boxes it reads were decoded out of the cartridge, so a hotspot's centre is
now computed, not measured.

### What the hit test does

`0x000e94` runs once a frame. Disassembled with capstone
(`CS_ARCH_M68K`, `CS_MODE_M68K_000`) over the ROM file:

```
0ea0  d0 = 0x6dc (crosshair x)   0ea6  d1 = 0x6de (crosshair y)
0eac  if d1 >= 0xa8 -> the verb bar, at 0x1092
0eb4  d1 -= 8                    0ebc  d0 += 8
0ec0  d0 += 0x6ca (camera x)     0ec6  d1 += 0x6ce (camera y)
0ecc  d0 >>= 3                   0ece  d1 >>= 3
0ed0  a0 = 0xff1200              0ed8  d3 = 0x6ac (the room)
0ede  a1 = L(0xff068e)           0ee4  a3 = L(a1 + 0x24)
0ee8  a1 = L(a1 + 0x1c)
0f72  if 6(a0) != d3 -> the next object
0f7a  btst #1, 0x18(a0) -> the sprite path, else the box path
1018  d4 = 2(a0)                 1020  a2 = a1 + L(a3 + (d4 - 1) * 4)
102c  hit iff d0 >= (a2) and d1 >= 2(a2)
           and d0 < (a2) + 4(a2) and d1 < 2(a2) + 6(a2)
1052  d7 = d2 + 3                1072  0xff06ae = d7
1056  a0 += 0x1a ; d2 += 1 ; while d2 <= 0xff06f4
```

There is no mask. The game walks a live object array at `0xff1200`, 26
bytes an object, word `id - 3` an entry, and tests one rectangle an
object, in eight-pixel cells, against the crosshair plus the camera.

### The record, and the two tables behind it

| offset | what |
| --- | --- |
| `+2` | box number, one-based, into the room's index table |
| `+6` | the room the object is in; 0 is nowhere |
| `+8` | state |
| `+0x16` | the verb a bare click runs |
| `+0x18` | bit 1: the object is an actor, positioned by its sprite |

`0xff068e` is a long pointing at the room's block. `+0x1c` is the base of
its boxes and `+0x24` the index table above them; for the hotel those are
`0x13e9b6` and `0x145eb8` in the cartridge, and every box is four
big-endian words -- x, y, width, height, all in eight-pixel cells.
`world.BOXES` is the 206 of them, decoded once.

### The centre, in crosshair coordinates

Inverting the test: for the cell `(cx, cy)` in the middle of a box,

    x = cx * 8 + 4 - 8 - camera_x
    y = cy * 8 + 4 + 8 - camera_y

`engine.Cursor.surveyed()` is that arithmetic and nothing else. On the
Gardener's Room it put the crosshair on all eight hotspots first try,
including the drawer the raster survey never found:

```
83 Television      box (22, 14, 10, 5)  ->  (212, 140)  hover 83  ok
84 Antenna         box (23,  8,  7, 5)  ->  (204,  92)  hover 84  ok
85 Air Freshener   box (23, 12,  3, 5)  ->  (188, 124)  hover 85  ok
86 Door            box (27,  6,  5, 9)  ->  (228,  92)  hover 86  ok
87 Drawer          box ( 7,  8,  2, 3)  ->  ( 60,  84)  hover 87  ok
88 Drawer          box (20, 11,  4, 3)  ->  (172, 108)  hover 88  ok
89 Poison Oak      box ( 2, 10,  5, 5)  ->  ( 28, 108)  hover 89  ok
90 Bed Spring      box (17, 10,  3, 2)  ->  (140, 100)  hover 90  ok
```

### What a step needs now

Nothing but a name. `Cursor.present()` reads the array once and answers
every object the cartridge has placed in this room, word to name, out of
the cartridge's own object table; `Cursor.named()` filters it. A route
step carries the object's name, and the word and the centre are derived.
The guide's "Click both Bedside Tables" is one step with `every=True`,
which clicks every object in the room called Drawer -- the two drawers
that stopped step 1b.

### The first attack, which failed

Before the disassembly the hunt was for a decompressed mask in work ram,
fitted against hover samples. It found nothing, and here is the whole of
what was tried, so nobody repeats it: every one of the 65,536 bases in
the 64 KB dump, against cell sizes 1, 2, 4, 8 and 16, widths 8 to 512 in
cells, nibble, byte and word depths, row-major and column-major, the raw
buffer and the byte-swapped one; then a run-length reading of each row,
matched canonically against the sampled rows; then the dump against the
room's `0xa6a`-byte cartridge block byte for byte, which matched only the
status line's text. There is no mask because the game does not use one.

## The whole world, in one array read

The same array answers more than the boxes. Field `+6` is the room an
object is in for every object at once, not just the ones on screen: 0 is
nowhere, 1 is the inventory, and anything else is a room word. One
180-record read at the hallway, twelve steps in, gives the entire state of
Blake's Hotel:

```
 0 nowhere    Goblet, Crumpled Note, Work Gloves, Weed Killer, Crowbar,
              Scissors, Empty Chili Can, Termites, Frozen Bell, Cow Bell,
              Soda Tab, Pot o' Water, Can Opener, Battery, Medallion,
              Doll, Extension Cord, Light Bulb, Bulb and Battery, ...
 1 carried    Antacid
 2 basement   Trap Door, Screwdriver, Stairs, Stairs, Secret Door,
              Locker, Rack, Hook
 6 mine       Mine Car, Wheel, Wheel, Wheel, Passage, Passage, Lantern
 9 hallway    Door x7, Dumb Waiter, The Stairs
11 outside    Christmas Lights, Sign, Double Doors, Locked Doors,
              Snow Covered Doors, Outlet, Main Entrance, Path to Shed,
              Bear, Totem Pole
12 outside    Shed, Path to Main Entrance, Lock, Snowman, Shovel
13 cafe       The Cook, Cabinet, Cash Register, Key, Double Doors, Lamp,
              Radio, Archway, Bottle of Oil
14 gardener   Television, Antenna, Air Freshener, Door, Drawer, Drawer,
              Poison Oak, Bed Spring, Book
15 kitchen    Beads, Chili, Kitchen Sink, Termites, Vent Cover, Dumb
              Waiter, Flour, Stove, Kitchen Door, Microwave,
              Refrigerator, Note, Pot
16 lobby      Bellhop, Picture, Zebra, Mailbox, Fire, Bell, Door,
              Archway, Stairs, Outside Door
17 office     Plaque, Drawer, Book Collection, Missing Book, Heater,
              Chalkboard, Door
18 shaft      Engine, Hose, Switch, Gas, Exit, Stone Path
19 pond       Pungeant Pond, Stone Path, Archway, Stone Bridge
20 tomb       Ancient Writing, Archway, Tomb, Statue, Killer Lettuce
21 bridge     Rope, Pole, Eyes, Totem Pole Bridge
```

That is the item map and the room contents of the whole scenario, read in
one go, with no room visited and no picture looked at. `nowhere` is the
answer to where an item that has to be made is: the Empty Chili Can and
the Bulb and Battery exist as objects from power on and are moved into
the inventory when their puzzle is solved.

So `FACT_WORDS` is not a hunt any more. An item is carried when its own
record's room word reads 1, and a door is open when the verb a bare click
would run has turned from Open into Shut. `world.fact()` writes that as an
address, and `world.FACT_WORDS` has 37 of them.

## Step 1c: twelve steps, the room graph, and where the guide is wrong

### The room graph, explored rather than assumed

Every room reached was left in all four directions from its own
checkpoint, restoring between tries, and where a plain hold failed the
walker stood under each of the room's boxes in turn and tried again.
What the emulator answered:

| from | hold | to | through | frames |
| --- | --- | --- | --- | --- |
| hallway | up | gardener | -- | 240 |
| gardener | right | hallway | the Air Freshener's box | 406 |
| lobby | up | hallway | -- | 252 |
| lobby | down | cafe | -- | 366 |
| cafe | right | lobby | -- | 274 |
| cafe | up | hallway | the Cabinet's box | 574 |

Two things fall out of that. The guide's "Goto Hallway" out of the
Gardener's Room is not down, it is right, and it only works once Shaggy
is standing at the back of the floor under the door -- holding a
direction from where the last click left him never leaves the room.
`Cursor.leave()` is that: hold the named edge, then the other three, then
stand under each of the room's three nearest boxes and try again.

No click on any door, archway or staircase changes the room. Every one of
`Double Doors`, `Archway`, `Cabinet`, `Door` and `Stairs` was clicked
with its own default verb from a checkpoint; all of them ran their verb
and left the room word alone. Rooms are only ever walked between.

### The two steps the guide gets wrong, and the one still open

Step 9 is "Click both Bedside Tables" and there really are two: objects
87 and 88, both called Drawer, at opposite ends of the room. Step 1b
clicked only 88 and step 10 then failed. With `every=True` the step
clicks every Drawer the room holds and step 10 takes the Antacid.

Step 12 is "Click Dumb Waiter (CLONK)" and the summary of the guide in
this file reads it as the way into the Kitchen. It is not. The dumb
waiter was clicked with Open, Use, Pull and Push, opened and clicked
again; the room word never moved off the hallway. The Kitchen was not
reached in this run, and the line stops at step 13 for want of it.

### Every step, and what it cost

```
  n room      step                           frames  find  verb  move click scene  capt  dist took
  1 lobby     Goto Hallway                      254     0     0     0     0     0     0     0 ok
  2 hallway   Click Door #1                    1150    86   100    78   336   528     1     1 ok
  3 hallway   Click Door #2                    1054    98   104    82   672    90     1     0 ok
  4 hallway   Click Door #3                    1516    94   104    82   404   802     1     1 ok
  5 hallway   Click Door #7                    1791     0   100    78   124  1190     1     1 ok
  6 hallway   Click Door #5                    1164    90   104    82   150   718     1     1 ok
  7 hallway   Click Door #6                    1068    96   104    82   140   446     0     0 ok
  8 hallway   Goto Gardener's Room              196     0     0     0     0     0     0     0 ok
  9 gardener  Click both Bedside Tables        3503   170   180   145  2428   180     0     2 ok
 10 gardener  Take Antacid                     1758    81    96    77  1214    90     0     2 ok
 11 gardener  Goto Hallway                      516     0     0     0     0     0     0     0 ok
 12 hallway   Click Dumb Waiter (CLONK)         601    93   100   110     8    90     0     0 ok
kind         frames  share
verb            992   6.8%
move            816   5.6%
click          5476  37.6%
scene          4134  28.4%
caption           5   0.0%
walk            966   6.6%
find            808   5.5%
total         14571
```

Steps 1 to 8 cost 8,193 frames against step 1b's 13,389 for the same
eight, a 39% fall, and all of it is the `find` column: reading a centre
out of the cartridge costs nothing, and the crosshair only moves once.
`find` is 808 frames over twelve steps now, against 3,580 over nine.

Power on to the end of step 12 is 19,723 frames, of which 5,152 is the
tape to the crosshair. The human's whole scenario is 49,620.

### The last screen this run reached

    frame        19723
    exact        8bcb50bad99559c7
    difference   b9b23074a63e0a26
    perceptual   27a7775f19094858
    room         9 (The Hallway)
    verb         3 (Open)
    line         Open Dumb Waiter

### The bundle, and the replay

    _runs/2026-09-14T20-04-45Z-hotel-1c2          19723 recorded
    _runs/2026-09-14T20-05-15Z-hotel-1c2-replay   16843 of 16843, watches match

`replayed 16843 frames of the 16843 the tape keeps, hash 8bcb50bad99559c7,
watches match`.

### What the cartridge region changed

With `run.memory(address, count, region="cartridge")` the hand-copied
tables are gone. `cartridge.Cartridge` reads 168 object names, 21 room
banners and 206 boxes straight out of the rom, and the four names this
step corrected in the route checked themselves against it: `Covered
Doors` is `Snow Covered Doors`, `Wine Rack` is `Rack`, `Lettuce` is
`Killer Lettuce`, and `Flashlight` was right all along -- it is word 183,
one past the object count the hit test loops to, which is why the first
pass missed it.

`tash.plan.graph` now carries the walking. `Hotel.way_to()` hands it the
timed edges with the room words as ids and gets the holds back, and
`Hotel.fetch()` uses it before a step whose object the array says is in
another room.

### What is still open after step 1c

The Kitchen, and with it steps 13 onwards. The guide's step 12 does not
lead there, no click on any hotspot changes a room, and the walk out of
the hallway towards the lobby was not found either: `The Stairs` is at
room x 32, the hallway's camera sits at 144 with Shaggy at the far right,
and walking him back over to the stairs did not move him past x 248 in
the frames allowed. The rooms the run has proved it can reach are the
lobby, the hallway, the Gardener's Room and the Cafe.

Everything the step needed of the geometry is done and `HOTSPOTS` is
derived, so what is left is topology: which hold, from which spot on the
floor, leaves which room. The explorer that found the six edges is the
tool for it -- it wants a wider budget and Shaggy walked to each
boxless hotspot, not just to each box.

### Harness findings, step 1c

All three of step 1b's findings landed on main as 8f25ab0: there is a
`cartridge` region, `tash.plan` searches graphs as well as grids, and
`run.play` refuses a machine that is past power on instead of failing at
the anchor. This step used all three.

One thing left, small and worth fixing while it is cheap: the region
argument sits in different places in the two readers.
`run.memory(region, address, count)` takes it first and positionally,
`run.string(address, length, region=...)` takes it last with a default.
Code that reads both -- `cartridge.Cartridge` does, a name is a string at
an address a long gives -- has to remember which is which, and the
mistake type-checks. Closed in 1ac12b5: `run.string(region, address,
length)` now takes the region first and requires it, like `run.memory`.

## Step 1d: the script VM, and the room graph read out of it

Step 1c read the geometry out of the cartridge. Step 1d reads the
*topology* out of it: not where a hotspot is, but what clicking it does.

### The interpreter

`0x0023dc` is the interpreter loop, and the disassembly is short enough to
quote in full:

```
0023dc  cmpa.l  a5, a6          ; a5 is the script pointer, a6 its end
0023de  ble.w   $2404
0023e2  move.w  (a5), d0        ; the opcode is a plain word, 0..33
0023e4  lsl.w   #$2, d0
0023e6  lea.l   $2354.l, a0     ; the dispatch table, 34 longs
0023ec  movea.l (a0, d0.w), a0
0023f0  moveq   #$0, d0         ; d0 = 0: the pass that only advances a5
0023f2  jsr     (a0)
```

There are two loops over the same script and the same table. The one above
passes `d0 = 0`, and every handler starts with a branch on that flag to an
epilogue that does nothing but `lea $N(a5), a5` - so the same handlers that
run the game also *measure* it. That is where the instruction lengths come
from: follow the first conditional branch of each of the 34 handlers to its
`lea`, and the answer is a table.

Four opcodes carry their body inside their length, which makes a script a
tree rather than a list:

| op | what it is | header | length |
| --- | --- | --- | --- |
| 1 | `on verb V (with object T)` | 8 | word at +6 |
| 3 | `if <condition>` | 0x10 | word at +2 |
| 4 | `while <condition>` | 0x10 | word at +2 |
| 19 | the script's own extent | 4 | word at +2 |

and two more are variable without a body: op 2 (say, length at +0xa) and op
33 (spawn, 8 plus the word at +2). Everything else is fixed, 2 to 12 bytes.

### Where the scripts are

The object table at `0x1461f0` is eight bytes an object. The first long is
the name's offset into the string block. The second long is a script
offset - but it belongs to the object *before* it: object w's script is the
long at `w * 8 - 4`, and the base is the scenario block's `+0x2c`,
`0x1467ac`, ending at its `+0x30`, `0x14f5dc`: 36,400 bytes for 181
scripts. Each script opens with a six byte header whose first word is the
code length, so the region checks itself - 181 of 181 scripts walk from
header to exactly the length the header claims.

The off-by-one is not cosmetic and it is not a guess. Every script is full
of self-references (op 9, 11 and 15 all take an object word, and all three
index `0xff1200 + (w - 3) * 0x1a` like every other opcode), and read one
entry along they all named the neighbour: the script filed under "Kitchen
Door" animated the Microwave. Read one entry back every script animates
itself, the puzzles come out in the guide's order, and every room change
lands on an object whose name is the doorway - "Stairs" to the Hallway,
"Archway" to the Cafe, "Main Entrance" to the Lobby. The first decode had
the Cafe's door to the Kitchen called *Key* and the fridge called *Cow
Bell*; the names were not the game being whimsical, they were the table
being read one object late.

`examples/scooby/script.py` is that walk, reading the rom through the
harness's `cartridge` region.

### What a script says

```
=== obj 46 'Stairs'
   on verb Look at
     op 16 f006 0000 1884
   on verb walk-onto
     op 27 0006 ffff ffff 01c8 0058
     go to room 9 (The Hallway) with fade
```

Opcode 24 is `move.w $2(a5), $ff06ac.l` - the room word, the one this plan
has been watching since step 1a. Opcode 17 is the same with a fade. Every
room change in Blake's Hotel is one of those two instructions inside some
object's verb block, so the room graph is not explored, it is read: 49 of
them in the cartridge, 39 in objects the hotel has placed, 36 distinct
edges.

Verb 11 is not on the verb bar. It is the verb the game runs when Shaggy
*walks into* an object's box, which is what a doorway is.

| from | to | object | verb |
| --- | --- | --- | --- |
| The Basement | The Mine | 143 Secret Door | walk-onto |
| The Basement | Outside the Hotel | 142 Stairs | walk-onto |
| The Basement | The Office | 141 Stairs | walk-onto |
| The Dungeon | The Tomb | 20 Passage | walk-onto |
| The Dungeon | The Dungeon | 25 Rope | Use |
| The Mine | The Basement | 33 Passage | walk-onto |
| The Mine | The Shaft | 29 Mine Car | Use |
| The Maze | The pond | 158 Exit | walk-onto |
| The Maze | The pond | 160 Exit | walk-onto |
| The Maze | The Tomb | 161 Exit | walk-onto |
| The Hallway | The Gardener's Room | 81 Door | walk-onto |
| The Hallway | The Kitchen | 80 Dumb Waiter | Use |
| The Hallway | The Lobby | 82 The Stairs | walk-onto |
| Outside the Hotel | The Basement | 57 Double Doors | walk-onto |
| Outside the Hotel | Outside the Hotel | 62 Path to Shed | walk-onto |
| Outside the Hotel | The Lobby | 61 Main Entrance | walk-onto |
| Outside the Hotel | Outside the Hotel | 69 Path to Main Entrance | walk-onto |
| The Cafe | The Kitchen | 130 Double Doors | walk-onto |
| The Cafe | The Lobby | 136 Archway | walk-onto |
| The Gardener's Room | The Hallway | 86 Door | walk-onto |
| The Kitchen | The Hallway | 110 Dumb Waiter | Use |
| The Kitchen | The Cafe | 115 Kitchen Door | walk-onto |
| The Lobby | The Hallway | 46 Stairs | walk-onto |
| The Lobby | Outside the Hotel | 47 Outside Door | walk-onto |
| The Lobby | The Cafe | 45 Archway | walk-onto |
| The Lobby | The Office | 44 Door | walk-onto |
| The Office | The Lobby | 101 Door | walk-onto |
| The Shaft | The Mine | 151 Exit | walk-onto |
| The Shaft | The pond | 152 Stone Path | walk-onto |
| The pond | The Maze | 156 Archway | walk-onto |
| The pond | The Shaft | 155 Stone Path | walk-onto |
| The pond | The Tomb | 156 Archway | walk-onto |
| The Tomb | The Dungeon | 167 Archway | walk-onto |
| The Tomb | The pond | 163 Archway | walk-onto |
| Across the River | The Dungeon | 169 Rope | Use |
| Across the River | Outside the Hotel | 174 Totem Pole Bridge | walk-onto |

(`world.CROSSINGS` holds it as data; `Cursor.exits()` rebuilds it live from
the scripts and the object array, so it follows objects that move.)

### The Kitchen, which step 1c could not reach

The guide's step 12 is "Click Dumb Waiter (CLONK)", and it reads as a
puzzle step because the guide never says what it is for. The script says:
the Dumb Waiter answers `Look at` and `Use`, and its `Use` block ends in
`put obj 3 in room 15` - object 3 is Shaggy. The dumb waiter is the lift to
the Kitchen, the CLONK is it arriving, and the Kitchen is one `Use` away
from the Hallway rather than behind any of the seven doors.

### Use and Give run the *second* object's script

`Use Door` alone leaves the status line reading "Use Door with " forever.
The verb-block handler at `0x00246a` says why: a block whose target word is
zero is accepted at once, but the search only runs over the object clicked
*second*, with the first click parked in `0x6b4`. So "Use A with B" runs
B's script with A as the target, and a target of zero means any A will do.

That is derivable per step, not guesswork: `Script.verbs(word)` answers the
pairs `(verb, target)` the object's script holds, so the first object to
click is data too. Where the block wants any object, `Cursor.helper()`
picks the nearest named one that is not called the same thing.

### Which word says what the crosshair is on

`0x6ae` is the box the hit test found. `0x6b4` is the object the status
line names, and they are not always the same object: in the Kitchen the
Note's box sits inside the Refrigerator's, `0x6ae` reads Note and the line
reads "Refrigerator". While a pair is open the two shift again - `0x6b4`
holds the first object for as long as the line says "Use Door with ", and
the object under the crosshair moves to `0x6b6`. `state.hover()` is that
rule in three lines, and `Cursor.aim()` gets the right answer through a
pair and over an overlap without knowing either case exists.

### The verb bar snaps, and pick_verb was wrong about it

Below `y = 160` the crosshair stops moving two pixels a frame and snaps to
the bar's ten cells. `Cursor.move()` does pixel arithmetic, so every verb
the route had not tried yet landed on the wrong cell - `Shut` selected
`Talk to`, `Use` selected `Pull`. It had survived twelve steps because
those steps only ever ask for Take, Open, Use and Eat from positions that
happened to work. `pick_verb` now drops into the bar, reads which cell it
snapped to off the crosshair, and nudges cell by cell; all ten verbs then
select first try.

### What puts what where, and what gates it

Opcode 5 is `move.w $4(a5), $6(a0)` - an object's room field. Walking every
script for it gives the other half of the dependency graph: which object,
under which verb, makes which object appear. The Kitchen alone:

```
obj  80 Dumb Waiter    on Use    put   3 Shaggy       in room 15
obj 111 Flour          on Push   put 112 Peephole     in room 15
obj 114 Stove          on Open   put 113 Stove        in room 15
obj 116 Microwave      on Open   put 118 Cow Bell     in room 15
obj 116 Microwave      on Open   put 117 Frozen Bell  in room 15
obj 119 Refrigerator   on Push   put 121 Soda Tab     in room 15
obj 119 Refrigerator   on Open   put 120 Fridge Door  in room 15
```

which is the guide's steps 12 to 17 in the cartridge's own words.

Most of those puts sit inside conditions, and the conditions are the game's
flag array: `0xff2a00`, one bit per fact, addressed as byte `n` bit `b` by
`btst d4, (a0, d3.w)`. Opcode 8 is the assignment that sets them. So a gate
is data too - `Microwave, Open` only yields the Cow Bell while
`flag[15].3` is set, and `flag[15].3` is set by `Microwave, Use`, which is
the player putting the bell in. Reading the gates and their setters
together turns "the item did not appear" from a mystery into a lookup.

### Every step, and what it cost

```
  n room      step                           frames  find  verb  move click scene  capt  dist took
  1 lobby     Goto Hallway                      254     0     0     0     0     0     0     0 ok
  2 hallway   Click Door #1                    1157    86   109    76   340   528     1     1 ok
  3 hallway   Click Door #2                    1068    98   106    80   686    90     1     0 ok
  4 hallway   Click Door #3                    1509    94    99    80   418   798     1     1 ok
  5 hallway   Click Door #7                    1800     0   109    76   124  1190     1     1 ok
  6 hallway   Click Door #5                    1164    90   106    80   150   718     1     1 ok
  7 hallway   Click Door #6                    1064    96   106    80   140   442     0     0 ok
  8 hallway   Goto Gardener's Room              188     0     0     0     0     0     0     0 ok
  9 gardener  Click both Bedside Tables        3534   170   193   163  2428   180     0     2 ok
 10 gardener  Take Antacid                     1831    81   126   120  1214    90     0     2 ok
 11 gardener  Goto Hallway                      514     0     0     0     0     0     0     0 ok
 12 hallway   Click Dumb Waiter (CLONK)         610    95   109   108     8    90     0     1 ok
 13 kitchen   Push Refrigerator                3459   105    98   312  1214    90     0     0 ok
 14 kitchen   Take Soda Tab                    1719    51    82    82  1214    90     0     0 ok
 15 kitchen   Use Pot with Kitchen Sink        1999   106     0   387  1216    90     0     1 ok
 16 kitchen   Click Microwave                  1735     0   119   112  1214    90     0    18 ok
kind         frames  share
verb           1362   5.8%
move           1756   7.4%
click         10366  43.9%
scene          4486  19.0%
caption           5   0.0%
walk           2396  10.2%
find           1072   4.5%
total         23605
```

Step 13 carries the ride: `Hotel.travel` plans over `Cursor.exits()`,
takes the Hallway to Kitchen edge, and the two `Use` clicks and the
cutscene between them are 2,020 of its 3,459 frames. Steps 1 to 12 are
frame for frame what they cost in step 1c bar the verb column, which the
bar-snap fix moved by 368 frames over twelve steps.

Power on to the end of step 16 is 29,717 frames, of which 5,152 is the
tape to the crosshair and 23,605 the sixteen steps; the rest is the
settle around each step's checkpoint. The human's whole scenario is
49,620. The run's tape replays into a bundle of its own frame for frame,
25,877 frames to hash `a0e9e25e7cf94b3d`, watches matching.

Step 15 is why the yield of every item-yielding step is now read back out
of `world.FACT_WORDS` before the step counts as taken. "Use Pot with
Kitchen Sink" clicked, the line read what it should and the pot stayed
empty: the pair's *first* object was whatever `Cursor.helper()` found
nearest, because the sink's `Use` block wants any object at all. The
guide's own line names the one it wants, and `Hotel.wants()` reads it off
the step's source text - "Use **Pot** with Kitchen Sink". The check cost
nothing to add and it is the only reason the failure was visible: the
click itself was perfectly successful.

### Where the line stops now

Step 17 is "Take Chili" and the Chili's script answers `Look at` and
`Use`, nothing else: `Cursor.instead()` will not substitute a paired verb
for a step that did not ask for one, so the step fails on the line it
wanted rather than clicking something arbitrary. The chili wants the can
opener from the Cafe, which is step 26 - so either the guide's order is
not the cheapest one, or the step means the *can* and the route has the
wrong object. That is the next thing to derive, and the dependency table
already holds the answer.

### Harness findings, step 1d

Step 1c's finding was closed in 1ac12b5: both readers now take the region
first and require it, `run.memory(region, address, count)` and
`run.string(region, address, length)`.

One more, and it is the same shape. Genesis Plus GX keeps 68000 work ram
byte-swapped, so every word this scenario reads is `raw[i ^ 1]` and every
byte read is `memory(region, address ^ 1, 1)`. `state.py`, `engine.py` and
every probe in this step carry that xor. The harness knows which core it
loaded and the core knows its word order; a `run.word(region, address,
signed=False)` that answers the number the *guest* would read would delete
the xor from four files and the whole class of bug where it is forgotten
in one of them. As it stands the mistake is silent: a byte read one address
out is a plausible number, not an error.

## Step 1e: the hotel by its own graph

`graph.py` derives the dependency graph from the cartridge's scripts and
nothing else; `plan.py` chains it back from the ending and prices the
result with the room graph and step 1d's measured frames. `route.py`
stays as the cross-check: the guide's 116 lines as data, never as input
to the plan.

### What the walker was missing

Three facts about the script VM, each read off a handler and each one a
correction to what step 1d wrote down.

Opcode 2 is not a leaf. Its handler at rom 0x25ac keeps the word at +0xa
as the whole instruction's length, stores `a5 + 0xe` as a script pointer
in a table of at most five entries, and skips `a5` by that length. It is
a dialogue choice whose body runs when the player picks the line. The
statue's reward -- `put 165 'Medallion' in room 20` and `flag[11].0 = 1`
-- lives in the body of the **fourth** such choice, which is exactly the
guide's "Talk to Statue (answer 4)". Opcode 33 stores `a5 + 8` the same
way.

Execution does not stop at the end of the verb block it matched. The
interpreter at 0x23dc scans with `d0 = 0`, where every handler's leading
`beq` skips it; opcode 1 alone falls through, checks the verb against
`d7` and the second object against 0xff06b6, and on a match sets bit 0 of
0xff0ac9, which stops the scan. The execute pass at 0x2406 then runs from
inside that block to the end of the script, and there opcode 1 skips its
whole block. So an action's effects are its body **and every instruction
after it**.

Opcode 4 is if/else, not a guard. True runs the body through the nested
runner at 0xff0878 and then skips the word at +4 bytes that follow the
block; false skips the body and falls into them. The statue proves it:
the outer block's +4 is 0x10 and the 16 bytes after it are the second
`on Talk to`, the one that answers once the puzzle is solved.

### The graph

Over the hotel's 183 objects: **375 actions, 190 facts, 119 gates, 34
doorways**, 181 actions carrying 544 effect branches. The verb census is
`{Pull 4, Take 47, Open 34, Shut 27, Use 65, Give 3, Push 6, Talk 5,
Eat 4, Look 140, walk-onto 40}`.

The ending is read from the scripts, not chosen: `Use Medallion(165)
with Hook(146)` is the only action in the cartridge that reaches room 10,
and its branch puts Daphne, Velma, Fred, Uncle Blake, the Bellhop, the
Cook, the Ancient Chieftan and the Double Doors there, clears
`flag[12].4` and goes. **The completion word is 0x6ac == 10.**

### The chain

`Plan.spine()` chains back from that action: a need is a gate that does
not hold at the seed, an object that is nowhere, the first-clicked object
of a pair not being carried, or a room no open doorway reaches. Nine
actions and nine precedence pairs come out of it:

    Take Medallion        -> Use Medallion with Hook
    Talk to Statue        -> Take Medallion
    Give Book to Blake    -> Talk to Statue
    Open Drawer           -> Give Book to Blake
    Take Book             -> Give Book to Blake
    Open Drawer           -> Take Book
    Open Door (three)     -> Open Drawer

`Plan.tour()` is the branch and bound over the topological orders of that
DAG, each node simulated through `Graph.run` and priced by
`tash.plan.graph_distances` over the cartridge's own doorways plus the
measured on-screen frames. It does not yet close: the chain is built
against the seed state, so a door whose gate is a flag several other
doors share can be scheduled in an order where its gate no longer holds,
and the bound then finds no winning order. The unfinished part is the
chain's static view of those gates, not the bound.

### Where it disagrees with the guide

1. The Chili (103) is already in the Kitchen (room 15) at the seed, on
   the emulator and in the scripts. The guide's step that fetches it is
   dead.
2. The guide's "Use X with Y" names the object clicked **second**. The
   script that runs belongs to the object clicked **first**: the ending
   block lives in the Medallion's script, not the Hook's, and the Hook's
   script is empty. Step 1d's README has this backwards.
3. `flag[12].3`, which the Book's `Give ... to Uncle Blake` block at rom
   0x14bfa6 tests, is written by no instruction in the cartridge's
   script region and by no 68000 instruction that names 0xff2a0c. The
   guide's "Use Termites with Uncle Blake" sets `flag[12].5`, which only
   the Termites read. `plan.OPEN_GATES` names the one fact the planner
   takes as given for that reason.

## Step 1g: the bounded block clicked, and six defects in the search

### The bounded block, proved on the emulator

The spec's correction 1 says a click runs the matched verb block and
stops at `a5 + word(a5+6)`; step 1f took that from the disassembly at
0x1a5a without a click behind it. Two clicks now stand behind it. Both
were chosen by searching the scripts for a block whose *unbounded*
reading -- one that runs on into the blocks that follow it -- predicts a
different state from the bounded reading, and then driving that click
from a checkpoint.

`Look at Key` (word 129, cafe, room 13). The unbounded reading predicts
`field(70,6) = 0` (the Lock leaves the world) and `flag[4].0 = 1`; the
bounded reading predicts nothing at all.

    before  room 13  flags 0100000000000000203a00400282ff01000000...
            record 129 Key      00740074001f000d0002ffff0000000000000000
            record  70 Lock     006400640000000c0001ffff0000000000000000
            record 125 The Cook 0001000d0000000d0001ffff000000000000005a
    after   room 13  flags 0100000000000000203a00400282ff01000000...
            record 129 Key      00740074001f000d0002ffff0000000000000000
            record  70 Lock     006400640000000c0001ffff0000000000000000
            record 125 The Cook 0001000d0000000d0001ffff000000000000005a
    flag bits changed: none

`Look at Dumb Waiter` (word 80, hallway, room 9). The unbounded reading
predicts the room word becomes 15 -- the Kitchen, the way the Open block
below it goes -- and `field(3,0) = 8`. The bounded reading predicts
nothing.

    before  room 9  record 80 00000034000000090002ffff0000000000000000
    after   room 9  record 80 00000034000000090002ffff0000000000000000
    the click took in 345 frames; flag bits changed: none

The emulator agrees with the bounded reading in both. Correction 1
stands and `graph.follows` needs no change.

The Statue the brief names is route step 101 of 116; the only checkpoint
that exists is the lobby, and the tomb is past the mine car, the pond,
the flashlight and the maze. The two clicks above test the same question
in rooms a checkpoint reaches.

### The search: what was wrong, and how far it gets

`Plan.search()` returned None because of six defects in the model, each
read off the cartridge:

1. `Plan.key()` named `(word, verb, target)`, which collapsed two blocks
   of one verb into one action. 23 of 387 keys were duplicated, three of
   them in the chain, and the dict kept the effect-less survivor.
   `graph.py` now names the block address `at` in every action dict.
2. Only verb 11 doorways counted as travel. The Kitchen is entered by
   `Use Dumb Waiter` (word 80, rom 0x4d74) and the Dungeon's lower room
   by `Use Rope` (word 169, rom 0x8496), so both were unreachable.
   `Plan.crossings()` takes every branch that writes the room word.
3. The spine was chained against the seed state, where the Termites and
   the Rope are nowhere, so the actions that place them were never
   relevant: 44 actions where the closure has 102.
4. A maker was asked for its handiest branch, not for the branch that
   writes the fact it was chosen for. `Open Locker` landed on its
   scissors branch and left the Extension Cord where it was.
   `Plan.aiming()` picks the branches that can write the aim, and the
   old behaviour is the fallback when the aimed chain fails.
5. Travel teleported. A doorway's own puts were never written, so the
   Office had no way out: the Trap Door that the Stairs place on the way
   up (`v11 Stairs`, rom 0x797a, `put 96 -> 17`) never appeared.
   `Plan.walked()` plays every crossing on the walked path.
6. A gate asking for a room an object is not in owed nothing, so every
   writer of that field was a candidate. `Plan.owes()` answers
   EVERYWHERE less that room.

From the lobby seed the regression now closes 11 of 17 rungs and 16
actions for 30,956 predicted frames:

     1 Open Door            1304      9 Open Drawer            700
     2 Open Cabinet          700     10 Take Can Opener         700
     3 Open Outside Door     700     11 Open Double Doors       700
     4 Open Door            1378     12 Take Book               700
     5 Open Door            1391     13 Use Can Opener+Chili    700
     6 Open Door            1750     14 Take Screwdriver        700
     7 Open Door            4221     15 Use Screwdriver+Vent    700
     8 Open Door            1324     16 Use Chili Can+Termites  700

The rung it stalls on is `Use Termites with Uncle Blake`, and the gate
that never holds is `field(169,6) == 4`: the Rope has to be in the
Dungeon. The sub-chain that would do it is the whole bear line --
`Open Locker` (cord into room 2), `Take Extension Cord`, `Use Extension
Cord with Outlet`, `Take Heater`, `Use Heater with Extension Cord`,
`Use Heater with Bear` (flag[15].2), `Use Work Gloves with Poison Oak`,
`Use Poison Oak with Bear` (the Totem Pole Bridge, words 55 and 56,
into room 21), `Use Scissors with Rope` (flag[13].2), `Take Rope`,
`Use Rope with Cuffs` -- and it dies at `Take Key`, whose only gate is
`field(125,6) == 13` wanted false: the Cook has to leave the cafe, which
`Eat Antacid` does through a room script the action model does not carry
as an effect. That is the next fact to fix, not a bound to raise: every
run above is bounded at 20,000 expansions and 240 seconds and spends
885 of them before it gives up.

## Step 1h: the actors room scripts move, and the search that closes

### What moves the Cook, read off the cartridge and measured

The brief for this step took the Cook to leave the cafe through the
cafe's own per-frame room script. He does not. Room 13's per-frame
script (room record `+0x10`, script at `0x2a4`, 62 bytes) is

    if flag[12].2 == 1 { image Cook=3; say; image Cook=4; say;
                         flag[12].2 = 0; image Cook=1 }

-- a reaction, gated on a flag, with no `put` in it. The write that
moves the Cook is in the Antacid's own `on verb 9` (Eat) block at
`0x58d0`: `put obj 125 'The Cook' -> room 0` at `0x5962`, under the
gates `here == 13` and `field(125,6) == 13`, followed by
`set flag[12].2 = 1` at `0x598a`. The Archway (object 136) at `0x7800`
puts him back into room 13 when `field(125,6) != 13` and
`flag[0].5 == 0`, which is why the computed plan eats an Antacid twice.

Measured on the emulator, from the lobby checkpoint through the route to
the cafe with the Antacid in hand (checkpoint `cafe-antacid`), with the
click placed by `engine.Cursor` and nothing else touched:

    before, frame 97,905   cook record 0001 000d 0000 000d 0001 ffff
                                       0000 0000 0000 005a 006a 0a0a 0700
                           field(125,6) = 13        flags[12] = 0x02
    click  "Eat Antacid"   frame 98,589 (verb 9, item strip cell (50,184))
    +651   frame 99,240    field(125,6) 13 -> 0
    +710   frame 99,299    flags[12] 0x02 -> 0x06   (bit 2 set)
    after, frame 100,089   cook record 0001 000d 0000 0000 0001 ffff
                                       0000 0000 0000 0090 0060 0a0a 07ff
                           field(125,6) = 0         flags[12] = 0x06

The put lands 651 frames after the click and the flag 59 frames after
the put, which is the `say` between them in the same block. The room
script's own effect -- clearing flag[12].2 once it has played the
reaction -- is what `graph.chores()` already carries as an action gated
on `here == 13`.

### Room scripts as actions, in the search

`graph.chores()` had already made every room's entry and per-frame
script an action whose extra gate is standing in that room. What the
search could not do with them was three things, all fixed in plan.py:

- `where()` ignored an action's own `here == R` gate, so an action the
  cartridge only runs in one room was judged doable anywhere. Eight of
  410 actions carry such a gate, each naming exactly one room.
- `lands()` read the effect against the state it was called in, so an
  action gated on the room always yielded nothing and was refused. It
  now reads the effect against the state the walk leaves (`stood()`).
- nothing required the objects an action clicks to be in that room or
  in hand, so Eat Antacid closed without the Antacid (`athand()`).

Then the search still stalled, and the stall was a fact, not a bound:
from the frontier state the Dungeon is unreachable, and `ways()` only
asked the gates of the doors into the room it wanted. A way out of a
room no route reaches yet is a way whose own room has to be opened
first, so `ways()` now walks back over the room graph through every
room it cannot stand in, owing the gates of the first shut door it
reaches that it can. The engine's own walks (`world.WALKS`, the maze
step keyed on the flashlight flag) belong to that walk back too, and
their makers have to seed `relevant()` -- no action reads a flag that
only a walk reads, so the backward closure missed the flashlight line
entirely.

### The plan the search computes

All 17 rungs close from the lobby seed, bounded at 20,000 expansions
and 240 seconds and spending 0.8 s of it: 57 actions, 87,250 predicted
frames over `Graph.run` costed by `plan.price`, ending in room 10.

     1 Open Outside Door@2716   16     700   30 Take Extension Cord@88f6  2   700
     2 Open Door@4458            9    1556   31 Use Extension Cord@898e  11  1066
     3 Push Radio@748c          13    1432   32 Take Heater@5c0e         17  1432
     4 Push Refrigerator@6b28   15    6705   33 Use Heater@5d3a          11  1432
     5 Open Cabinet@710e        13    1066   34 Open Shed@2d30           12  1066
     6 Open Door@3fd4            9    1904   35 Take Work Gloves@2e90    12   700
     7 Open Door@3740            9    1391   36 Use Work Gloves@2ed0     14  1924
     8 Open Door@3ac4            9    1750   37 Use Poison Oak@5462      11  1838
     9 Open Door@4862            9    4221   38 Open Drawer@59d2         17  1432
    10 Open Door@32f6            9    1324   39 Take Scissors@5ab8       17   700
    11 Open Drawer@5296         14     940   40 Use Scissors@5ae8        21  1798
    12 Take Soda Tab@6d9e       15    4845   41 Take Rope@83d0           21   700
    13 Open Radio@7588          13    1066   42 Take Bed Spring@5642     14  1924
    14 Take Battery@76ac        13     700   43 Use Bed Spring@5672      11  1838
    15 Take Can Opener@71f8     13     700   44 Take Christmas Lights    11   700
    16 Open Double Doors@2a24   11    1340   45 Use Battery@7704         11   700
    17 Take Screwdriver@7934     2    1066   46 Use Soda Tab@6e7a        11   700
    18 Open Drawer@5374         14    1924   47 Take Wheel@223c           6  1432
    19 Take Antacid@58a0        14     700   48 Take Wheel@2192           6   700
    20 Eat Antacid@58d0         13    1838   49 Take Wheel@20e8           6   700
    21 Use Screwdriver@78c2     15    3946   50 Use Wheels@1d1e           6   700
    22 Use Can Opener@7228      15     700   51 Use Rope@8454             4  3596
    23 Take Book@57aa           14    1832   52 Use Termites@60f0         5  1766
    24 Use Empty Chili Can@6074 15    3826   53 room 20@03ae             20   366
    25 Eat Antacid@58d0         13    1066   54 Give Book@580a           20   700
    26 Take Key@72cc            13     700   55 Talk to Statue@82b6      20   700
    27 Use Key@7314             12    1706   56 Take Medallion@7f4c      20   700
    28 Take Christmas Lights    11    1066   57 Use Medallion@7f82        2  2164
    29 Open Locker@7a22          2    1066

The second column is the room the action is done in and the third the
frames `travel` plus `price` predict. Rung 53 is a room script played as
an action: standing in the Tomb is the whole of it.

### The item strip, and what it cost

Three of the plan's actions in five click something that is carried, and
none of them could be clicked before this step. The panel behind the
verb bar is not a grid of ten cells: it is a strip that shows one item
at a time, whose last cell (cursor `(220,184)`) is an arrow, and
clicking the arrow brings the next item in. `panel_spot` walked the ten
cells it believed in, found whatever happened to be showing, and
answered None for everything else; `panel_cell` had the same fault, and
`Hotel.centre` never asked the panel at all. Measured: with five items
held, only `Battery` read at `(60,184)`; one click on the arrow and the
same cell read `Antacid`.

`engine.Cursor` now sweeps the strip's row, turns it at the arrow and
waits out the status line's lag (`names()`), `act()` clicks a held
object through the strip instead of aiming at the room, and
`Hotel.centre` answers the strip's cell for anything held. The Cook's
reading above is the first click this made possible.

### Measured against predicted, where the play reached

`examples/scooby/finish.py` plays what `Plan.order()` computes: each
action becomes a guide-shaped step, `Hotel.one` runs it, and every
action that takes is checkpointed as `plan-NN`.

      n room      action                          frames    plan took
      1 lobby     Open Outside Door                 2150     700 ok
      2 hallway   Open Door                        30841    1556 ok
      3 cafe      Push Radio                        5080    1432 ok
      4 kitchen   Push Refrigerator                13207    6705 no hotspot
                                                                  reads
                                                                  'Refrigerator'

Action 4 is the fact this step stops on: `Hotel.travel` takes any
derived exit out of a room, and the way into the kitchen is the Dumb
Waiter, a click gated on its own flag. The plan's `hops()` knows that --
it only counts a crossing whose gates are open -- but the player does
not consult it, so it walked to the cafe, could not find a Refrigerator
there and refused. The fix is for `Finish` to walk the room list
`Plan.route()` gives and to open a shut crossing as an action, not to
place a press by hand; that is where the next session starts.

The route play (`hotel.py`, the guide's own 116 steps) reaches step 23
in 42,037 frames from power on. Step 20, Goto Cafe, used to fail --
`leave()` walks the room edges and then the boxes nearest the edge,
which never included the Kitchen Door -- and now costs 432 frames:
`Hotel.travel` crosses a walk-on exit by its own box first
(`cross(word, verb)`), and only falls back to `leave()`.


## Step 1i: a shut crossing is an action, and what a doorway really is

### The finisher walks the plan's own room list

`Finish.steps()` turns every rung of `Plan.order()` into one or more
guide-shaped steps. The hops come first, from `Plan.route()`'s room list
and nothing else:

```python
    def crossings(self, number, facts, room, names):
        here = facts[(graph.HERE,)]
        walk = self._plan.route(here, room, facts) if room else None
        walk, out = walk or (), []
        for one, two in zip(walk, walk[1:]):
            cross = self._plan.crosser(one, two, facts)
            word = cross["word"] if cross else None
            out.append(dict(route.step(
                number, names.get(two, str(two)), route.WALK,
                "Goto %s" % names.get(two, two),
                verb=cross["verb"] if cross else None,
                obj=self._cart.name(word) if word else None, hover=word)))
            facts = self._plan.walked(one, two, facts)
        return out
```

`Plan.crosser()` is new and is the whole of the fix: it answers the
action whose open branch writes the room word between two rooms, so the
step carries that action's own verb and object. The Dumb Waiter into the
kitchen is `verb 5` (Use) on word 80 and becomes a click; the Archway out
of the cafe is `verb 11` and becomes a walk. `Hotel.one` runs both
through the same door, `Hotel.hop`, which no longer routes for itself:

```python
    def hop(self, step, room):
        was = self._state.room()
        went = self._cursor.cross(step["hover"], step["verb"])
        here = self._state.room()
        done = here == room if room else here != was
```

Nothing in the finisher places a press. Every press is `engine.Cursor`
reading the room word, the hovered word, the verb, the crosshair and the
status line and answering with a hold or a tap.

### Measured against predicted, where the line reached

`Finish.play()` runs the rungs in order and stops at the first refusal;
every rung that takes is checkpointed `plan-NN` and its tape length read
back out of the checkpoint's own yaml. The measured column counts every
frame the trials spent, the trials a restore folded away included; the
line column is what the tape actually holds at that checkpoint.

      n room      action                  hops  frames    plan    line off
      1 lobby     Open Outside Door          0    2198     700    4470   0
      2 hallway   Open Door                  1    8694    1556    6562   0
      3 cafe      Push Radio                 2   55294    1432       0   5

Rungs 4 to 57 were not reached: `Goto cafe` refused, and its reading is
`obj 45 verb 11 left 16 into 16, not 13` -- the Archway (word 45) is
the crossing the plan names out of the lobby, and none of the 145
trials the crossing search makes took it from where rung 2 leaves
Shaggy standing. Their predictions stand as step 1h computed them, in
the table above: 87,250 frames over all 57, against the human record's
49,620 for the whole game. Measured against predicted over the three
rungs that ran: 66,186 against 3,688.

The `off` column is new and is `Finish.drift()`: after every action the
facts the plan's model wrote are read back out of ram, and the count is
how many of them the ram does not hold. Rungs 1 and 2 drift by nothing,
which is what says the click's script really ran; rung 3's 5 are the
room word and the four facts that follow from being in the wrong room.

### How a click finds its hotspot, counted

`Cursor.locate()` reads an object's centre off the cartridge and rasters
rows only when putting the crosshair there does not read the word back.
Counted over the final play (`Cursor.tally()`, `Cursor.looks()`): two
hotspot lookups, both answered by the cartridge's own centre, no row
sweep at all and no lookup that failed.

Two is a thin measurement and it is worth saying why, because the
session record shows the crosshair moving almost continuously. Only a
click asks for a hotspot, and the line stopped after two clicks. The
motion in the record is not `locate()` rastering: it is
`Cursor.walk_onto` searching for a crossing, which toggles the crosshair
down, holds a direction to place Shaggy, restores and tries the next
column -- 145 trials and 55,294 frames on rung 3 alone. The row sweep
this table counts costs nothing at present and the crossing search costs
everything.

### The film, the bundle and the hashes

The session's bundle is `_runs/2026-09-15T10-54-39Z-scooby` in the
worktree: 812,263 frames produced, of which the tape keeps 6,562 --
restoring `plan-02` folds the line back to what the two rungs that took
actually cost. Shutting the session down writes `tape.yaml`, and

    tash tape replay --bundle _runs/2026-09-15T10-54-39Z-scooby \
      --record _runs --name scooby-step1i-line

replays those 6,562 frames at stride 1 in 28.34 s (231.5 fps, 3.86x
real time) into `_runs/2026-09-15T12-00-14Z-scooby-step1i-line`. The
live hash after the restore and the replay's own hash are the same
value, `01336315ad8f82dc`, and the replay reports `watches match`. The
film is 109.5 s of 1280x896 h264 with a `pcm_s16le` track at 44.1 kHz,
32.7 MB, and it is copied unchanged into `_devlog/scooby/` as
`2026-09-15-scooby-step1i-line.video.mkv` with its report and run.yaml.


### What a doorway really is, measured

A verb-11 block is reached by walking, and the question is where. Three
things are now known, all read off the emulator rather than guessed.

**The box is the crosshair's, not the floor's.** In the lobby the
Stairs (word 46) carry box `(32, 4, 3, 2)` -- cells 32 to 34 across,
rows 4 and 5 down -- while Shaggy walks the floor at y 176 and below.
Standing under any column of that box and holding `up` does nothing. A
16-pixel sweep of the whole floor from the lobby checkpoint answers:

```
 x 352 up   -> room 9    shaggy (279, 312)
 x 352 down -> room 13   shaggy (314, 249)
```

x 352 is past the right wall, so `place()` leaves him against it at
x 333; from there `up` is the hallway and `down` is the cafe. The box
is 70 pixels to the left of both. `Cursor.walk_onto` therefore tries the
box's own columns first, then the floor sweep, then the measured hold
from `world.EDGES` with no placing, then `leave()`; the lane that works
is remembered per (room, room, word) so the next crossing of the same
pair costs one trial.

**A room has more than one doorway, so a changed room word is not a
taken crossing.** Walking onto the hallway's Door (word 81, box
`(33, 4, 3, 5)`, `goes 14`) landed in room 16 instead, through The
Stairs (word 82) that share the room. Every trial is now judged against
`Cursor.destination(word)`, the room that object's own script names,
and a trial that lands anywhere else is restored away exactly like a
trial that never left.

**Where Shaggy stands when he arrives decides what he can reach.** This
is the fact that stops the run at rung 3 and it is worth stating
exactly. Coming into the lobby down the stairs leaves him at
`(264, 264)`; from there `place()` walks him to `(256, 312)` and he
stops, and every one of the 140 (column, hold) trials plus the four
plain holds plus `leave()` either stays in room 16 or goes back up to
room 9. From the lobby checkpoint, standing at `(133, 192)`, the same
search finds the cafe in one trial. The floor of the lobby is not
convex for a single held direction: the stairs landing is a lane he has
to walk off before the archway is reachable, and nothing in the engine
yet plans a path across a floor -- it holds one direction at a time.
That is the next step's work, and it is a fact about the game, not a
bound on the search.

### Harness findings

- **A run seeded by a restore cannot be replayed.** `TapeRecording::
  Seeded` (sources/tash/tash/tape-recording.cpp) clears `_stretches` and
  sets `_line` to the checkpoint's own length, but `Folded(frames)`
  sums stretch lengths from zero to find the cut, so after a seed the
  arithmetic is off by the seed's length and the fold truncates
  nothing. The next restore finds `LineAt(place->harness)` no longer
  agreeing with `place->frames - 1`, and with no `line` on an in-memory
  checkpoint the recording goes `_adrift`; from then on `LineUpTo`
  answers nothing and every checkpoint yaml is written without its
  `line:` block. Measured: `plan-01..05.yaml` carried lines of 4,484 /
  35,325 / 40,405 / 44,334 / 47,104 and `plan-06..10.yaml` carried
  none. Worked around by powering on and playing the tape at the start
  of every run instead of restoring the lobby.
- **The scenario api cannot read the tape's own length.** `run.frames()`
  and `run.observe()["frame"]` both answer the session's produced
  frames, which count every trial a restore folded away. The tape's
  length is only in a checkpoint's yaml, so `Finish.line_frames()`
  reads it out of `<bundle>/_checkpoints/<hash>/<name>.yaml` by hand.
  A `run.line()` returning the current line length, and `None` when the
  recording is adrift, would make both the measurement and the fault
  above visible from python.
- **`run.checkpoint(name)` has no scratch kind.** Every python
  checkpoint is probed (two restores and 2 x PROBE_FRAMES) and written
  to disk, which is the right default for a named moment and the wrong
  one for the seed a search restores to a hundred times.
  `tash.search` uses `CheckpointKind::SCRATCH` internally and python
  cannot ask for it.

## Step 1j: what a click really walks, and the floor walked by presses

### What a click does to Shaggy's position, measured

Three clicks were measured from `plan-02` (the hallway with the door
open) and from the lobby checkpoint, reading `shaggy_x` per frame.

- **A floor spot with no object under it.** Never taken. `live` (0x630)
  stays 1 through the click and `shaggy_x` does not change: lobby
  (40,140), (216,140), (120,120) and hallway (100,150) all read the same
  word before and after.
- **The crossing's object under its walk-on verb.** There is no such
  click: verb 11 is not on the verb bar, so a walk-on doorway has no
  clickable verb at all. The hallway's The Stairs (word 82) answers verb
  11 and nothing else and never reaches the status line. The lobby's
  Archway (45), the lobby's Stairs (46) and the hallway's Dumb Waiter
  (80) do answer `Look at` (verb 10), and that block is one opcode 16
  (say) with no opcode 15 in it: the caption plays and `shaggy_x` holds.
- **A far object whose matched block carries opcode 15.** The game walks
  him the whole way. `Open Door` on the lobby's Door (44) took him from
  x 201 to x 296 in 198 frames -- x 296 is exactly the box's own left
  edge, cells (37,3,2,5) times eight -- and the walk carried him into
  that door's verb-11 box, so the room word turned to 17 at +198 and the
  click cost 302 frames all in. In the hallway `Open Door` on word 74
  walked him from x 276 to x 228.

The walk belongs to the script, not to the click: opcode 15 is what
moves him, and no doorway carries it under a clickable verb. So the
brief's second branch is the one the measurement picks -- the engine
plans a path over the floor -- and `Cursor.cross` never clicks a floor
spot.

### A held direction is one step; a walk is a run of presses

The measurement that changed the step. In the hallway from `plan-02`,
holding `left` for 840 frames moves `shaggy_x` from 276 to 241 and no
further; the word then sits at 241 for the remaining 780 frames. Five
presses of the same direction -- 60 frames down, 12 frames up -- move
him 276, 241, 217, 152, 78, 53, and the fifth turns the room word to
the lobby. The pad is not read as a held walk: each press is one step of
the game's own walk and the next step needs a new press.

Two more facts fell out of the same measurements:

- `shaggy_y` (0x4e0) is not a position. With no pad input at all from
  `plan-02` it wanders 248..296 over 1,200 frames while `shaggy_x` holds
  at 276. Every stop condition that watched the pair never fired, which
  is what made the old crossing search spend 1,200 frames a trial doing
  nothing. Only x is a position word; `state.State.shaggy()` still
  answers both and only `[0]` is read.
- `up` and `down` do not move him in the hallway at all: 300 frames of
  either leaves the exact screen hash of the idle frame. The hallway is
  a corridor and its floor is one line.

### The crossing: a turning point counted in presses

`examples/scooby/floor.py` is the whole of it. A path is
`(first, many, way)`: `many` presses of `first` along the floor, then
presses of `way`, which is the direction the room graph measured for
that edge. `tash.search` searches that space under checkpoint and
restore, scoring a trial `TAKEN` when the room word is the one the
object's own script names, `-TAKEN` when it is any other room, and minus
the x gap to the doorway's box otherwise. Round one is the four ways
with no turning point at all; round two adds `first` in (right, left)
and `many` in 1..19, the graph's own way first. What wins is remembered
against `(room, x // 32, word)` and replayed as one trial next time.

The turning point is counted in presses and not in pixels because the
stops a press run makes are exactly the standing positions the floor
allows; a column in pixels names a spot the walk may step over. Measured
in the lobby after the stairs, with a 24-frame press: the first seven
presses leave x at 264, then the stops are 273, 284, 294, 307, 311, and
`down` crosses into the cafe from 294 on. The 60-frame press used first
stopped at 308, one pixel short of a stop that crosses, and the search
found nothing.

    crossing                     path taken           trials  frames
    hallway -> lobby, word 82    (None, 0, 'left')         4   2,989
    lobby -> cafe, word 45       ('right', 2, 'down')     42  30,168
                                                          46  31,412

Step 1i spent 145 trials and 55,294 frames on the lobby-to-cafe crossing
and did not take it.

The one defect that hid the answer for three attempts: the crossing leg
ended on "Shaggy's own word stopped changing", and a crossing leg is
vertical, so `shaggy_x` never changes and the leg ended after one press
every time. `Floor.march` counts presses and nothing else.

### The hotel to rung eleven

`Finish.main` powers the machine on, plays the lobby tape and runs the
plan's rungs in order. Ten took; the eleventh refused.

      n room     action             hops  frames   plan   line off took
      1 lobby    Open Outside Door     0    2274    700   4546   0 ok
      2 hallway  Open Door             1   19938   1556   6784   0 ok
      3 cafe     Push Radio            2   36188   1432   9852   0 ok
      4 kitchen  Push Refrigerator     3    7656   6705  13808   1 drift
      5 cafe     Open Cabinet          1  133331   1066  16539   0 ok
      6 hallway  Open Door             2   37578   1904  19681   1 drift
      7 hallway  Open Door             0    1078   1391  20759   1 drift
      8 hallway  Open Door             0    1513   1750  22272   1 drift
      9 hallway  Open Door             0    1951   4221  24223   1 drift
     10 hallway  Open Door             0     978   1324  25201   6 drift
     11 gardener Open Drawer           1   92554    940      0   4 refused

Measured against predicted over the eleven: 335,039 frames against
22,989. The measured column counts every trial a restore folded away;
the line column is what `run.line()` answers after the rung's
checkpoint, and 25,201 of those frames are what a replay runs. Step 1i
reached rung 2 and 6,562 line frames.

The refusal, exactly: rung 11 is `Open Drawer` in the Gardener's Room
and its first step is `Goto gardener`, which reads
`obj 81 verb 11 left 9 into 9, not 14`. Word 81 is the hallway's
Gardener's Room door and it is still shut: rung 10's drift says so.
Opening a hallway door ors its own bit into `field(81,12)`, the plan's
model has all six standing at 63, and the ram holds 47 -- one door's bit
never went in, so the derived crossing the plan walks does not exist and
no path over the floor can take it. The four facts rung 11 is off by are
that room word and the three that follow from it. Rungs 6 to 10 each
drift by one for the same reason, the bit they wrote arriving where the
model did not put it.

The click lookups, counted the way step 1i counts them
(`Cursor.tally()`, `Cursor.looks()`): seventeen hotspot lookups, all
seventeen answered by the cartridge's own centre, no row sweep and no
lookup that failed. The crossing searches: 451 trials and 310,270
frames over seven remembered paths, which is 93% of everything the line
spent.

### The film, the bundle and the hashes

The session's bundle is `_runs/2026-09-15T12-53-20Z-scooby` in the
worktree: 339,952 frames produced, of which the tape keeps 25,202 --
restoring `plan-10` folds the line back to what the ten rungs that took
actually cost. `run.tape()` writes `tape.yaml` without shutting the
session down, which is new on main this morning, and

    tash tape replay --bundle _runs/2026-09-15T12-53-20Z-scooby \
      --record _runs --name scooby-step1j-line

replays those 25,202 frames at stride 1 in 111.84 s (225.3 fps, 3.76x
real time) into `_runs/2026-09-15T12-58-32Z-scooby-step1j-line`. The
live hash after the restore and the replay's own hash are the same
value, `1addbc0616715f90`, and the replay reports `watches match`. The
film is 420.6 s of 1280x896 h264 with a `pcm_s16le` track at 44.1 kHz,
127.1 MB, copied unchanged into `_devlog/scooby/` as
`2026-09-15-scooby-step1j-line.video.mkv` with its report and run.yaml.

### Harness findings

- **Two `tash.search` calls cannot be alive at once.**
  `StateSearch::Explore` in `sources/tash/python/state-search.cpp` names
  its scratch checkpoint `"tash.search/" + level` with `level` local to
  the call, so a search started inside another search's score callback
  takes the same name, and the inner one's `Forget` drops the outer
  one's state: `RuntimeError: python: 'tash.search/0' is not a
  checkpoint name`. A prefix unique per call would make nesting work.
  Worked around here by keeping the search one level deep and putting
  the second leg in the candidate tuple.
- **`run.line()` and `run.tape()` land as promised.** The line reads
  back at every checkpoint of this run and the film no longer needs the
  session shut. `Finish.seed()` still powers on and plays the tape
  rather than restoring, because that is what the line from frame zero
  needs and nothing measured here asked for the restore path.

## Step 1k: the bit the hallway takes back, and the entry script

### Which bit, and what takes it

The six hallway doors are words 74 to 79 -- the guide's doors #1, #2, #3,
#5, #6 and #7 -- and each one's `Open` block ors a distinct bit into
`field(81,12)`, the record of word 81, which is door #4, the Gardener's
Room door. Read off the cartridge and then measured, one door at a time,
from a hallway with the mask at zero:

    door  word  bit  mask after  frames
    #1      74    1           1   1,152
    #2      75    2           2   1,328
    #3      76    4           4   1,848
    #5      77    8           8   2,215
    #6      78   16          16   1,780
    #7      79   32          32   2,357

Door #1's script carries the rest of the puzzle. Its `Open` block is an
opcode 4 whose true side is a refusal line gated on `flag[11].3`, and
everything below is the else: the or, the door's own animation, and then
a second opcode 4 gated on `field(81,12) == 63` whose body is the whole
reward -- every door slammed shut in turn, `field(81,0) = 63` and
`field(86,0) = 122` (the Gardener's Room door's two poses),
`flag[14].2 = 0`, and `flag[11].3 = 1`. Word 81's own script is gated the
other way: `if flag[14].2 == 1` guards its `Open` block, and that block
asks for `flag[11].3 == 1`. So the Gardener's Room door is not opened by
clicking it; it is opened by the sixth of the six, whichever it is, and
the click that opens it is the click that completes the mask.

The ram held 47 after rungs 6 to 10 of step 1j, which is 63 without bit
16, and bit 16 is door #6, word 78. Step 1j's rung 2 was door #6: the
first hallway rung of the line opened it, wrote its bit, and the plan
then left the hallway for the cafe and the kitchen and came back.

**What takes the bit is the hallway's own entry script.** Room 9's
`+0x0c` script is two instructions and nothing else:

    flag[11].4 = 0
    field(81,12) = 0

The VM spec has said since step 1d that entering a room runs the room
record's `+0x0c` script through the execute loop (3.2 c). What the
planner did with that is the defect: `Graph.chores()` made every room
script -- entry and per-frame alike -- an action a plan may order, gated
on standing in the room, and `Plan.walked()` walked between rooms
without running any of them. A per-frame script is fairly an action: the
Tomb's is rung 54 of the plan below, and it fires when its own flags
line up. An entry script is not an action at all. It is what arriving
means.

Measured on the emulator, from step 1j's own checkpoints:

    checkpoint  room      field(81,12)  flag[11].3
    lobby       lobby                0           0
    plan-01     lobby                0           0
    plan-02     hallway             16           0
    plan-03     cafe                 0           0
    plan-04     kitchen              0           0
    plan-05     cafe                 0           0
    plan-06     hallway              8           0
    plan-07     hallway             10           0
    plan-08     hallway             14           0
    plan-09     hallway             46           0
    plan-10     hallway             47           0

and the clearing caught in the act, from `plan-10`:

    plan-10, in the hallway          mask 47
    walk out onto word 82, lobby     mask 47   2,840 frames
    walk back in onto word 46        mask  0   2,118 frames

Leaving does not clear it. Arriving does. And the payoff, also from
`plan-10`: `Open Door` on word 78 turns the mask to 63, `flag[11].3` to
1 and `flag[14].2` to 0 in 1,033 frames, and walking onto word 81 then
crosses into the Gardener's Room in 1,788 frames, which is the crossing
rung 11 of step 1j asked for and could not have.

### The model fix

`graph.py` splits the room scripts in two. `Graph.chores()` keeps only
the per-frame ones as orderable actions; `Graph.arrivals()` answers each
room's entry script, and nothing may order one. `plan.py` gains
`Plan.arrived(room, facts)`, which runs them, and `Plan.played(action,
facts)`, which runs an action and then the entry script of any room the
action lands in. `Plan.walked()`, `Plan.lands()` and `Plan.step()` go
through them, so every walk the planner prices and every rung
`Finish.steps()` builds now carries the arrival's own writes.

Only room 9's entry script has any effect at all, of the twenty-one
rooms in both scenarios, which is why nothing before this step needed
it.

One more fact parted from the ram at rung 4, and it is not a flag:
`field(3,0)`, which is Shaggy's own pose. No gate in either scenario
reads any field of object 3, and his walk rewrites the pose the next
frame, so a script's `pose Shaggy` is not a fact that stands.
`Graph.transient()` drops it from an action's effects; a pose on a thing
that is not Shaggy stays, because the Mine Car's ride is gated on one.

### The buried box: three doors in one rectangle

With the arrival rule in, the plan reached the hotel's outside and
stopped at `Open Double Doors` with a refusal the engine had never
printed before: `the line never read 'Open Double Doors'`. The crosshair
was where the cartridge's own box table says the Double Doors are, and
the status line named something else.

Room 11 keeps three records in almost one rectangle:

    word  name                box  cells
      57  Double Doors         84  (15, 10, 11, 4)
      58  Locked Doors         91  (15, 10, 11, 4)
      59  Snow Covered Doors   90  (15,  9, 11, 5)

Step 1i measured that the hit test at ROM 0x000e94 walks the live object
array forwards and answers the *last* record it hits, so the highest
word wins. 59's box covers every cell of 58's, and 58's is 57's exactly.
All forty-four cells of 57's box were probed one at a time on the
emulator and every one of them answered 59. Neither 57 nor 58 can be
clicked while 59 stands in the room, and no sweep will ever find them.

`Plan.buried(word, facts, room)` is the rule: an object's own box, less
the cells every higher word standing in the same room covers, and the
hiders named when nothing is left. `Plan.athand()` refuses a buried
object, `Plan.needed()` owes that each hider's room word leaves the
room, and `Plan.reads()` declares the hiders' room words so the search
knows the dependency is real. The planner then derives the guide's own
chain without being told it:

    Use Key with Lock -> Open Shed -> Take Crowbar -> Take Shovel
    -> Use Shovel with Snow Covered Doors  (59 leaves the room)
    -> Use Crowbar with Locked Doors       (58 leaves the room)
    -> Open Double Doors

### The turning point has two axes

The crossing from Outside the Hotel (room 11) to the shed's path (room
12) is word 62, `Path to Shed`, whose box is `(0, 9, 1, 10)` -- one cell
wide at the left edge of the room, ten cells tall. `floor.py` searched a
path as `(first, many, way)` with `first` drawn from `("right",
"left")`: the turning point could only ever move along the floor, never
across it. Arriving from the lobby puts Shaggy at x 232, row 72 --
cell (29, 9), which is the box's own top row, and walking left from
there drifts one pixel up into cell row 8, which is word 61, the Main
Entrance, and he walks straight back into the lobby. Measured:

    from the lobby arrival        (232, 72)
    left x5                       (219, 71)   room 11, then 16
    down x5                       (234, 119)  room 11
    down x3, left x30             (218,  71)  room 16, wrong
    down x5, left x30             (  6, 119)  room 12, taken
    down x8, left x30             (  7, 135)  room 12, taken

and one press is about 22 pixels of floor: from the turning point at
(234, 119), eleven left presses read x 219, 199, 177, 152, 132, 105, 80,
59, 33, 8, 3, and the eleventh turns the room. `LEG`, fourteen presses,
was never the shortfall.

So the turning point is now the axis *across* the way, not along the
floor: `ACROSS` pairs each way with the two perpendicular ones, and the
way itself is derived from the cartridge rather than guessed.
`Floor.apart(word)` answers how many cells outside the box Shaggy stands
on each axis, reading his x at 0xff04dc and his row at **0xff04f4** --
the y the doorway test itself divides by eight (vm-spec 3.2 b), not the
0x4e0 step 1j proved wanders. `Floor.onto(word)` closes the larger gap
first, and `Floor.gap()` scores both axes where it scored only x.
`world.EDGES` carried `edge("outside-left", "right", "outside-right")`,
a guide word with no measurement behind it; the way across is `left`,
and the entry now says so.

Searched fresh from the lobby arrival the crossing costs 42 trials and
15,210 frames and answers `('down', 4, 'left')`, and rung 20 of the line
below takes it.

### The plan, and what it measured

`Plan.search()` answers 62 rungs and 93,258 predicted frames, against
step 1h's 57 and step 1j's 58: the arrival rule adds the hallway's sixth
door back where it belongs and the buried-box rule adds the shed chain.

    plan   1 lobby     Open Outside Door                     700
    plan   2 hallway   Open Door                            1556
    plan   3 cafe      Push Radio                           1432
    plan   4 kitchen   Push Refrigerator                    6705
    plan   5 cafe      Open Cabinet                         1066
    plan   6 hallway   Open Door                            1850
    plan   7 hallway   Open Door                            1378
    plan   8 hallway   Open Door                            1391
    plan   9 hallway   Open Door                            1750
    plan  10 hallway   Open Door                            4221
    plan  11 hallway   Open Door                            1304
    plan  12 gardener  Open Drawer                           940
    plan  13 kitchen   Take Soda Tab                        4845
    plan  14 cafe      Open Radio                           1066
    plan  15 cafe      Take Battery                          700
    plan  16 cafe      Take Can Opener                       700
    plan  17 gardener  Take Antacid                         1466
    plan  18 cafe      Eat Antacid                          1838
    plan  19 cafe      Take Key                              700
    plan  20 outside-right Use Key with Lock                 1706
    plan  21 outside-right Open Shed                          700
    plan  22 outside-right Take Crowbar                       700
    plan  23 outside-right Take Shovel                        700
    plan  24 outside-left Use Shovel with Snow Covered D     1066
    plan  25 outside-left Use Crowbar with Locked Doors       700
    plan  26 outside-left Open Double Doors                   700
    plan  27 basement  Take Screwdriver                     1066
    plan  28 gardener  Open Drawer                          1924
    plan  29 cafe      Eat Antacid                          1838
    plan  30 kitchen   Use Screwdriver with Vent Cover      3946
    plan  31 kitchen   Use Can Opener with Chili             700
    plan  32 gardener  Take Book                            1832
    plan  33 kitchen   Use Empty Chili Can with Termites    3826
    plan  34 outside-left Take Christmas Lights              1706
    plan  35 basement  Open Locker                          1066
    plan  36 basement  Take Extension Cord                   700
    plan  37 outside-left Use Extension Cord with Outlet     1066
    plan  38 office    Take Heater                          1432
    plan  39 outside-left Use Heater with Bear               1432
    plan  40 outside-right Take Work Gloves                  1066
    plan  41 gardener  Use Work Gloves with Poison Oak      1924
    plan  42 outside-left Use Poison Oak with Bear           1838
    plan  43 office    Open Drawer                          1432
    plan  44 office    Take Scissors                         700
    plan  45 bridge    Use Scissors with Rope               1798
    plan  46 bridge    Take Rope                             700
    plan  47 gardener  Take Bed Spring                      1924
    plan  48 outside-left Use Bed Spring                     1838
    plan  49 outside-left Take Christmas Lights               700
    plan  50 outside-left Use Battery with Light Bulb         700
    plan  51 outside-left Use Soda Tab with Bulb and Batte    700
    plan  52 mine      Take Wheel                           1432
    plan  53 mine      Take Wheel                            700
    plan  54 mine      Take Wheel                            700
    plan  55 mine      Use Wheels with Mine Car              700
    plan  56 dungeon   Use Rope with Cuffs                  3596
    plan  57 5         Use Termites with Uncle Blake        1766
    plan  58 tomb      Stand in room 0                       366
    plan  59 tomb      Give Book to Uncle Blake              700
    plan  60 tomb      Talk to Statue with 16246             700
    plan  61 tomb      Take Medallion                        700
    plan  62 basement  Use Medallion with Hook              2164

Played from power on with the tape, rungs 1 to 16 in one sitting and 17
to 25 resumed from `plan-16`. `frames` is what the rung measured,
`plan` what it predicted, `line` the tape's length after it, `off` the
facts the ram did not hold:

      n room      action                          hops  frames    plan     line   off took
      1 lobby     Open Outside Door                  0    2274     700     4546     0 ok
      2 hallway   Open Door                          1   19938    1556     6784     0 ok
      3 cafe      Push Radio                         2   36188    1432     9852     0 ok
      4 kitchen   Push Refrigerator                  3    7656    6705    13808     0 ok
      5 cafe      Open Cabinet                       1  133331    1066    16539     0 ok
      6 hallway   Open Door                          2   37188    1850    19291     0 ok
      7 hallway   Open Door                          0    1153    1378    20444     0 ok
      8 hallway   Open Door                          0    1057    1391    21501     0 ok
      9 hallway   Open Door                          0    1501    1750    23002     0 ok
     10 hallway   Open Door                          0    2049    4221    25051     0 ok
     11 hallway   Open Door                          0    1086    1304    26137     0 ok
     12 gardener  Open Drawer                        1    3590     940    28145     0 ok
     13 kitchen   Take Soda Tab                      2   96208    4845    31783     0 ok
     14 cafe      Open Radio                         1  133081    1066    34284     0 ok
     15 cafe      Take Battery                       0    1800     700    36084     0 ok
     16 cafe      Take Can Opener                    0    1833     700    37917     0 ok
     17 gardener  Take Antacid                       3  169860    1466    41295     0 ok
     18 cafe      Eat Antacid                        3  151534    1838    66939     0 ok
     19 cafe      Take Key                           0    2262     700    69201     0 ok
     20 outside-right Use Key with Lock              3   68617    1706    91890     1 drift {('here',): (12, 11)}
     21 outside-right Open Shed                      0    4309     700    94335     0 ok
     22 outside-right Take Crowbar                   0     726     700    95061     3 drift
     23 outside-right Take Shovel                    0    1653     700    96714     0 ok
     24 outside-left Use Shovel with Snow Covered D   1    2781    1066    97545     2 drift
     25 outside-left Use Crowbar with Locked Doors   0   10033     700        0     2 the line never read 'Open Locked Doors'

Twenty-four rungs taken, every fact through rung 19 exactly as the model
wrote it, and the line 97,545 frames against step 1j's 25,201. Rungs 1
to 16 spent 733 crossing trials and 568,114 frames of session; 17 to 25
spent 500 trials and 339,492 frames of the 411,775 those nine rungs
measured, which is 82% of them. Of the 24 click lookups rungs 17 to 25
made, 15 were answered by the cartridge's own centre and 9 missed --
every miss a word a higher word answers for, which is the next step.

Three drifts stand, and the last of them is why the line stops.

- Rung 20, `('here',): (12, 11)`. The model prices `Use Key with Lock`
  as done from room 12 and the ram stands in room 11. The crossing the
  planner walks is real -- the rung took -- but the lock is reachable
  from either side of the path, so the room the plan names and the room
  the click happens in differ by one hop. Harmless here, and it is the
  only `here` drift in twenty-four rungs.
- Rung 22, `Take Crowbar`: the model writes `field(67,6) = 1` (carried)
  and the ram leaves 67 in room 12 while `field(68,6)` goes from 12 to
  0. The click took word 68 and not word 67. Word 68 is the Nail, box
  `(4, 6, 2, 1)`, and word 67 is the Crowbar, box `(4, 6, 2, 4)`: the
  Nail is one cell tall and sits on the Crowbar's own top row, and it is
  the higher word, so it answers there. Three of the Crowbar's four rows
  are free, so `Plan.buried()` -- which refuses only a box with no free
  cell at all -- let it through, and the cartridge's centre for 67 aims
  at a cell 68 answers for. (The Shed, word 64, box `(3, 4, 6, 9)`,
  covers the Crowbar entirely and is harmless: it is the lower word.)
- Rung 24, `Use Shovel with Snow Covered Doors`: the model writes
  `field(59,6) = 0` and `flag[5].1 = 1`, the ram holds 11 and 0. The
  click was accepted and the block that clears the snow did not run,
  which is the same shape one rung on: the pair's first object is the
  crowbar rung 22 never took.

Rung 25 then refuses honestly. `Use Crowbar with Locked Doors` needs the
status line to read `Open Locked Doors`; word 59 still stands in room 11
and the hit test answers it for every cell of 58's box, so the line
reads `Open Snow Covered Doors` and the click is never made. The screen
the run stops on is Outside the Hotel with the crosshair on the Snow
Covered Doors, `368fda9ecb13615b`.

**The next step is the centre, not the box.** `Plan.buried()` asks
whether a word has any free cell; what a click needs is that the cell it
aims at is free. `Cursor.aimed()` and `Cursor.locate()` should choose a
cell of the word's own box that no higher word standing in the room
covers, and the planner should own the same rule, so that `Take
Crowbar` aims where 67 answers rather than where 68 does.

### The film, the bundle and the hashes

The session's bundle is `_runs/2026-09-15T13-11-25Z-scooby` in the
worktree: 2,088,601 frames produced over the whole step, of which the
tape keeps 97,545 -- restoring `plan-24` folds the line back to what the
twenty-four rungs that took actually cost, and `run.tape()` writes
`tape.yaml` and the manifest with the session still alive. Then

    tash tape replay --bundle _runs/2026-09-15T13-11-25Z-scooby \
      --record _runs --name scooby-step1k-line

replayed those 97,545 frames at stride 1 in 445.70 s -- 218.9 fps, 3.65x
real time, `D2`, `0 dropped` -- into
`_runs/2026-09-15T14-06-08Z-scooby-step1k-line`. The live hash after the
restore and the replay's own hash are the same value,
`fc842a3f665d3b9d`, and the replay reports `watches match` with
`probed 0, parted 0`. The film is 1,627.8 s of 1280x896 h264 with a
`pcm_s16le` track at 44.1 kHz, 482.5 MB, copied unchanged into
`_devlog/scooby/` as `2026-09-15-scooby-step1k-line.video.mkv` with its
report and `run.yaml`.

Twenty-seven minutes of it, and what it shows: the lobby's outside door,
the six hallway doors one after another with the mask now holding, the
cafe's radio and cabinet and the kitchen's refrigerator, the Gardener's
Room reached at last through word 81, the antacid that moves the cook,
the key, the walk out of the hotel and down to the shed, the lock, the
shed, the crowbar and the shovel -- and it stops outside the hotel with
the crosshair on the Snow Covered Doors, screen `368fda9ecb13615b`,
because the click one rung back took the Nail instead of the crowbar.
The hotel does not end in this step.

### Harness findings

- **An in-memory checkpoint restores without its line, and the
  recording goes adrift.** `ScenarioRun::Restore` in
  `sources/tash/python/scenario-run.cpp` builds
  `session::Checkpoint{ name, state, pixels, at, frames, place }` for a
  name it still holds in `_checkpoints` and never sets `.line`, where
  the disk branch twenty lines below it does (`if (kept->line)
  back.line = &*kept->line;`). `TapeRecording::OnRewind` then has no
  `back.line` to `Seeded()` from once `back.place` has left the line,
  so it sets `_adrift` and `run.line()` answers `None` from that call
  on -- and every checkpoint written afterwards lands on disk with no
  `line:` key at all. Measured here: after `restore("lobby")` and then
  `restore("plan-16")`, `run.line()` read `None` at the live frame and
  at both `plan-19` and `plan-16`, and `plan-17`, `plan-18` and
  `plan-19` have no line in their yaml while `plan-10` and `plan-16`
  do. The gap is that one missing assignment; the in-memory branch
  could carry `_parts.line->LineUpTo(kept.frames)` exactly as
  `ScenarioRun::Cache` already does when it writes the state out.
  Worked around in python with `run.forget(name)` followed by
  `run.restore(name)`, which forces the disk path: the same `plan-16`
  then read a line of 37,917 frames and the film was made from it.
- **Two `tash.search` calls still cannot be alive at once**, as step 1j
  reported: `StateSearch::Explore` names its scratch checkpoint
  `"tash.search/" + level` with `level` local to the call. Every search
  here is one level deep.

## Step 1l: the click aimed at a free cell, and three presses that lied

### The aim

Step 1i measured the hit test at ROM 0x000e94: it walks the live object
array forwards, tests one rectangle per object against the crosshair and
the camera, and writes the **last** record it hits into 0xff06ae. The
higher word wins. Step 1k turned that into `Plan.buried()`, which
refuses a word only when the higher words in the room leave its box no
free cell at all -- and rung 22 then aimed `Take Crowbar` at the
cartridge's centre of word 67's box `(4, 6, 2, 4)` and took word 68, the
Nail, box `(4, 6, 2, 1)`, which is one cell tall on the Crowbar's own
top row.

The rule this step is the cell, not the box, and it is one sentence on
both sides of the harness:

    the cells of the word's box, less the cells of every higher word
    standing in the room, and the free cell nearest the box's middle

In `engine.py` that is `Cursor.cells()`, `Cursor.hiders()`,
`Cursor.spread()`, `Cursor.buried()` and `Cursor.surveyed()`;
`Cursor.aimed()` answers the free cell the camera has on screen and
`Cursor.locate()` refuses a buried word by name instead of rastering the
screen for it. In `plan.py` it is `Plan.hiders()`, `Plan.free()` and
`Plan.buried()`, reading the same boxes out of the cartridge and the
same room field out of the object array. Nothing in either reads a
pixel.

### Three presses that lied

The aim alone did not take the rung. Three more defects stood between
`Take Crowbar` and the hotel's doors, and each one is a press the engine
believed and the game did not.

**A pair keeps its verb.** `Cursor.instead(word, verb)` answered the
verb the clicked word's record handles, so `Use` on word 59 (Snow
Covered Doors, whose own verbs are 10 and 3) became `Open`. A pair's
block does not belong to the second object: `Use Shovel with Snow
Covered Doors` is the Shovel's block, word 72, with 59 as its target,
and the second object need not answer the verb at all. Measured both
ways from `plan-23`: shovel first and snow second clears the snow in
about 20,660 frames from any cell of the box; snow first and shovel
second does nothing. `instead()` now never substitutes a pairing verb.

**The inventory strip has two rows.** `PANEL_WALK` was
`("left",) * 4 + ("right",) * 4`, one row and nothing else, and
`panel_spot()` opened with a blind `tap(C)`. Measured from `plan-24`,
with seven things carried:

    row  cursor y   cells at x 60, 100, 140, 180
    0         180   Soda Tab, Battery, Can Opener, Antacid
    1         207   Key, Crowbar, Shovel, --

The Crowbar was never on the row the walk visited. The walk now covers
both rows; `panel_spot()` tries each face of the bar in turn instead of
assuming which one is up, and `panel_face()` turns the strip only on a
face where the hit test answers a carried word -- because `turn_panel()`
is a `b` press, and a `b` press on the verb face is a verb clicked.

**`start` is the pause key.** `Cursor.skip()` mashed `(B, START, A)`
with the last of them given the whole cutscene budget. `START` opens the
game's password menu -- the SCOOBY-DOO plate over five five-character
groups, `CONTINUE` and `QUIT MYSTERY` -- and that screen has no verb
bar, so `skip()` reads "not playing", presses `START` again, and toggles
the menu until the budget is spent. It is why rung 25 checkpointed on
the menu and rung 26 then spent 28,028 frames on "the bar refused the
verb". With `START` dropped, rung 25 costs 21,463 frames and rung 26
takes in 2,057.

`Cursor.confirmed()` also reads Shaggy's x now: a held direction moves
the crosshair in aim mode and moves him in walking mode, and the byte at
0xff0ab5 is the mode but flickers frame to frame, so the press is the
authority and the two readings must agree.

### The plan, and the rungs it took

`Plan.search()` rerun from the lobby seed with the free-cell rule on
both sides answers **62 rungs and 93,258 predicted frames** -- the same
count and the same frames as step 1k, and the same order rung for rung.
The rule did not add or remove a rung; it changed where the crosshair
goes, not what the planner asks for.

`Take Crowbar` is the proof. Restored from `plan-21`, word 68 (the Nail)
is in room 0 and not in room 12 at all -- step 1k's diagnosis had it
standing over the Crowbar, and it does not; `Take Crowbar`'s own block
is what *puts* 68 into room 12. With the free cell aimed at, the rung
writes `field(67,6) = 1` and `field(68,6) = 12`, the status line reads
`Take Crowbar`, and the inventory panel holds it. The shovel on the snow
and the crowbar on the lock then each run their own block, and the
double doors open.

`frames` is what the rung measured, `plan` what it predicted, `line` the
tape's length after it, `off` the facts the ram did not hold:

      n room      action                          hops  frames    plan     line   off took
     22 outside-right Take Crowbar                   0    1740     700    96094     0 ok
     23 outside-right Take Shovel                    0    1805     700    97899     0 ok
     24 outside-left Use Shovel with Snow Covered D   1   23102    1066   119068     0 ok
     25 outside-left Use Crowbar with Locked Doors    0   21463     700   140594     0 ok
     26 outside-left Open Double Doors                0    2057     700   142651     0 ok
     27 basement  Take Screwdriver                    1    4110    1066   145129     0 ok
     28 gardener  Open Drawer                         4  237779    1924   149564     0 ok
     29 cafe      Eat Antacid                         3  150387    1838   175503     0 ok
     30 kitchen   Use Screwdriver with Vent Cover     3   53844    3946   199693     0 ok
     31 kitchen   Use Can Opener with Chili           0   21386     700   221079     0 ok

**Thirty-one rungs of sixty-two, and not one drifted fact.** Every rung
from 22 on wrote exactly the facts the model said it would, which is the
first stretch of this line where `off` is zero the whole way down.

Rungs 25 to 31 measured 491,026 frames against 10,874 predicted, and
521,688 of those frames -- 776 trials -- went into crossing searches.
The eleven paths the run remembered, keyed by (room, lane, word):

    (11, 5, 57) (None, 0, 'up')     (2, 2, 142) ('right', 7, 'up')
    (11, 5, 61) ('right', 4, 'up')  (16, 3, 46) ('right', 8, 'up')
    (9, 2, 81)  ('right', 6, 'up')  (14, 4, 86) ('right', 3, 'up')
    (9, 8, 82)  (None, 0, 'left')   (16, 8, 45) ('right', 2, 'down')
    (13, 4, 136)(None, 0, 'right')  (16, 9, 46) (None, 0, 'up')
    (15, 10, 115) ('left', 4, 'up')

Of the twelve click lookups rungs 25 to 31 made, eleven were answered by
the cartridge's own box and one missed: word 92 in room 13, "off
screen", which the sweep then found. No lookup this step was refused for
being buried.

### Where it stopped

Not at a refusal. The step's budget was spent inside rung 32,
`gardener Take Book`, in the middle of a crossing search: the play had
run 633,498 frames of session since `plan-24` and 2.7 million since
power on, and the four rungs before it had cost 463,000 frames of
search between them. **The hotel does not end in this step**, so there
is no scenario-complete screen to hash and no won word to hunt across
it; that stays for the next step, and the profile is unchanged.

The frames are all in the same place they were in step 1k. A crossing
searched fresh costs between fifteen and two hundred thousand frames;
one replayed from the run's own table costs one trial. The table dies
with the run. Checking it in, keyed by (room, lane, word) exactly as
`Floor._paths` holds it, is worth more than any other change available.

### The film, the bundle and the hashes

The session's bundle is `_runs/2026-09-15T14-58-58Z-scooby` in the
worktree: 3,049,838 frames produced over the whole step, of which the
tape keeps 221,080 -- the line from power on to the end of rung 31.
Round 3's fixes are what make that number possible: 776 crossing
searches ran inside it and `run.line()` still answered, so no rung had
to be replayed to rebuild the tape. Then

    tash tape replay --bundle _runs/2026-09-15T14-58-58Z-scooby \
      --record _runs --name scooby-step1l-line

replayed those 221,080 frames at stride 1 in 1,038.99 s -- 212.8 fps,
3.55x real time, `D2`, `0 dropped` -- into
`_runs/2026-09-15T15-44-37Z-scooby-step1l-line`. The live hash after
the restore of `plan-31` and the replay's own hash are the same value,
`fea6180aad788ec7`, and the replay reports `watches match` with
`probed 0, parted 0`. The film is 3,689.9 s of 1280x896 h264 with a
`pcm_s16le` track, 1,076.4 MB, copied unchanged into `_devlog/scooby/`
as `2026-09-15-scooby-step1l-line.video.mkv` with its report, its
`run.yaml` and its `tape.yaml`.

Sixty-one minutes of it, and what it shows: the whole hotel from the
lobby's outside door to the kitchen -- the six hallway doors, the
radio, the cabinet, the refrigerator, the Gardener's Room, the antacid
and the cook, the key, the shed's lock, and then the crowbar taken
correctly at last (word 67, not the nail), the shovel on the snow, the
crowbar on the lock, the double doors open, the basement's screwdriver,
the drawer, the vent cover and the chili -- and it stops in the kitchen
with the chili opened, screen `fea6180aad788ec7`, thirty-one rungs of
sixty-two done and the step's frame budget gone inside rung 32.

### What the line waits on, measured

The user watched step 1k's film and called the standing still padding.
Measured on this line's own tape and trace, with a frame counted idle
when the pad writes no transition and the picture's exact hash repeats
the frame before: **73,418 of 221,080 frames, 33.2%** -- but in 48,507
separate stretches, a mean of 1.5 frames each, which is the Genesis
redrawing at half the frame rate, not a player standing still. The
longest single still is **303 frames, 5.1 s**, at line frames 239 and
886, both in the boot before the title; the longest with the game live
is **295 frames, 4.9 s** at frame 12,389, rung 4 in the kitchen. Only
3.0% of the line sits in stills of half a second or more, and nothing
at all is still for longer than 5.1 s, against 1k's longest of 12 s.

The pad tells the other half of the story. The line writes 76,491
transitions and the three longest stretches between them are:

    start      frames      rung  what held it
        0        1890  before 1  tapes/title-to-control.yaml, the `title`
                                 segment's template_image anchor at 0.95
     3048        1200         1  Cursor.click_and_read, ACTION_LIMIT
     8354        1200         3  Cursor.click_and_read, ACTION_LIMIT

The boot is honest: the tape's `title` segment carries `frames: 1` and
an anchor on the title's own picture, so the 1,890 frames are what the
ROM's logos cost, not a number anyone wrote. The other two are not.
`Cursor.click_and_read(wanted)` presses b and then steps two frames at
a time until `state.line()` reads something other than what was
clicked, giving up at `ACTION_LIMIT = 1200`. On twenty of the
thirty-one rungs the status line never changes -- the block runs,
Shaggy walks over and opens the thing, and the line still reads
`Open Outside Door` at the end of it -- so the loop spends its whole
ceiling: **23,687 frames, 10.7% of the line**, seventeen of them
exactly 1,200. `Cursor.act` then clicks a second time (`CLICK_TRIES`)
and that click is taken in 36 frames, which is why a 326-frame stretch
(36 + `SCENE_ONSET` 90 + `CAPTION_WAIT` 200) follows every one of them.

The word to wait on is already in the profile: `live` (0xff0630) drops
to 0 within four frames of the click and is back at 1 between 66 and
132 frames later, so **18,982 frames, 8.6% of the whole line**, are
spent after the game has taken the pad back. That is the single largest
cut available to the tightening step, and it is one `run_until` on a
watch the line already carries.

Where the four constants the plan names actually stand:

- **`Floor.rest`, `REST_LIMIT = 900`** -- never reached. `rest` steps
  `TICK = 2` and breaks when Shaggy's own x has been still for
  `REST_STILL = 16` ticks and `live` reads 1, so it is on the word
  already; the tape holds no press-free stretch of 900 frames at all.
  Its cost is the 32 frames past the last move that `REST_STILL` asks
  for, once per rest.
- **`CLICK_AFTER = 20`** -- 245 taps, **4,900 frames, 2.2%**. A fixed
  count with no word behind it yet.
- **`LIFT_FRAMES = 60`** -- never reached either; `lift` and
  `enter_bar` both break on the cursor's own y word, and no 60-frame
  press-free stretch exists in the tape.
- **`LINE_LAG = 12`** -- 112 stretches of 13 frames, **1,456 frames,
  0.7%**, which is the status line's own draw delay measured in step
  1h.

One more that the plan does not name and that costs more than three of
those four together: **`MODE_PROBE = 8`**, the held direction in
`Cursor.confirmed()`, 1,269 stretches for **10,152 frames, 4.6%**. The
step widened `confirmed()` to read Shaggy's x beside the crosshair's
but did not touch the 8; the crosshair word answers a held direction in
fewer frames than that, and nothing measured here says how many.

No wait in this step's changes is a new round number: `confirmed()`
reads two words the frame after the hold, `panel_face` and `panel_spot`
turn on what the hit test answers, and the aim reads boxes off the
cartridge with no waiting at all.

### Harness findings

Round 3 landed both of step 1k's gaps and this step ran without either
workaround: `run.restore()` on a name the session still holds in memory
carries its line, so `plan-24` restored in memory read 119,069 frames
and every checkpoint after it landed with a `line:` key; and a
`tash.search` no longer costs the run its line, so the 776 crossing
trials rungs 25 to 31 spent left `run.line()` answering and `run.tape()`
writing a 221,080-frame tape at the end of them. The `forget` then
`restore` dance is gone from this example.

What still stands, and one new thing:

- **A checkpoint restored from another run cannot be taped.** Only the
  checkpoints this session's own line wrote can seed a film, so a step
  that resumes from the previous step's `_checkpoints` has to replay the
  rungs it wants in the film. This step replayed rungs 24 and 25 for
  that reason alone.
- **Two `tash.search` calls still cannot be alive at once** in this
  example's own code, though the harness now nests them: `Floor.sift()`
  is the only caller and it is one level deep.
- **`python_status` answers a running job and `observe`/`look` answer
  beside it**, which is how every rung above was watched without
  stopping the play. `shot` does not: it refuses with "python-NN is
  running", so a screenshot of a live job has to come from `look`.

Round 4 landed while the play was running, and the branch was rebased
onto it before the film: `tash session shutdown` now ends the server
with its last session -- though not here, see below; a
restore of a name the session holds serves the session's own copy and
says when a newer file on disk was passed over; a search straight after
a seeding restore tapes and replays with watches matching, so the
`run.step(1)` this step still put between the restore and the tape is
no longer needed; and a detached python job runs under no frame budget,
which is why `python-42` reported `ran 0 frames under no budget`. The
play itself ran on the binary built before the rebase; the film, the
measurements above and this text ran on the one built after it.

- **`tash session shutdown` did not free the port.** It ended the run
  and wrote the bundle -- `ran 3049839 frames, 971553 encoded` -- but
  the server kept listening on 7821 and `session observe` answered
  `nothing is launched; call launch first`, so a session without a run
  still counts as a session. The port was freed by killing the pid
  `ss -ltnp` names. A server started as `tash serve` from a shell,
  rather than by a client that disconnects, is the case that does not
  end itself.
