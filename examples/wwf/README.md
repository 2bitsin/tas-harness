# examples/wwf/

`profile.yaml` runs *WWF WrestleMania: The Arcade Game* (USA, Europe) on
Genesis Plus GX from the cartridge library in `roms/`. `tapes/title-to-first-bell.yaml`
carries a fresh machine from power on to the first bell of the first
Intercontinental match as Bret Hart, and `scenario.py` plays one match from
there by mashing. Everything below was measured on this box on 2026-09-14.

## The character

**Bret Hart.** The published guides agree he is the fastest wrestler on the
roster and the one whose combos every FAQ works through, so his list is the
best documented to check against; he is also three presses of `down` from
where the roster cursor starts, which is the cheapest pick to tape.

## The counters

| watch | address | width | endianness | what |
|---|---|---|---|---|
| `p1_health` | `0xb312` | 2 | little | the player's health, 164 at the bell, 0 at a fall |
| `p1_bar` | `0xb316` | 2 | little | the lit pixels of the player's health bar, 104 at the bell |
| `p1_meter` | `0xb31c` | 2 | little | the player's combo meter |
| `cpu_health` | `0xb32e` | 2 | little | the standing opponent's health |
| `cpu_bar` | `0xb332` | 2 | little | its bar, mirrored to the right of the screen |
| `cpu_meter` | `0xb338` | 2 | little | its combo meter |
| `timer_tens` | `0xaae6` | 2 | little | the match clock's tens digit, 9 at the bell |
| `timer_ones` | `0xaae8` | 2 | little | its ones digit, 9 at the bell and 8 on the first tick |
| `match` | `0xab14` | 2 | little | which match of the ladder, 1 upwards; 0 off a ladder |
| `opponent` | `0xb37a` | 2 | little | the opponent's character id |
| `p1_x` | `0xaabe` | 2 | little | the player's x in the ring |
| `cpu_x` | `0xaade` | 2 | little | the opponent's x in the ring |
| `p1_action` | `0x8548` | 2 | little | the move the player is playing, `0x2aee` standing |
| `cpu_action` | `0x87b8` | 2 | little | the same field of the opponent, one object record on |
| `skill` | `0xf4f0` | 2 | little | the skill OPTIONS is set to, 2 to 10 in twos |

Little-endian on a 68000 is not a typo, for the reason
`examples/columns/README.md` gives: Genesis Plus GX hands
`retro_get_memory_data(SYSTEM)` back byte-swapped against the 68000, so a
word's value byte is the even address the watch names and the odd byte after
it is the zero padding.

### The wrestler records

Health, bar and meter are three fields of one record, and the records are a
table of six at a stride of 14 bytes from `0xb312`:

| offset | holds |
|---|---|
| +0 | health |
| +2 | health again, the copy the bar is drawn from |
| +4 | the bar's lit pixels |
| +6 | `0x5400`, the same in every record |
| +10 | the combo meter |
| +12 | the meter less two |

The player is record 0 (`0xb312`) and the opponent who is on his feet is
record 2 (`0xb32e`). Records 3 to 5 carry the extra bodies of the handicap
matches; a scenario that plays those has to look at all of them, because
`cpu_health` alone reads 0 while a second opponent is still up.

The bar is health times 0.635: 104 pixels for 164 health at the bell and 91
for 143 mid-match, both measured off a shot (the bar's interior is rows 18
to 21, the left bar x 9 to 117 and the right one mirrored; the empty colour
is `(32,32,32)`). The second bar, rows 24 to 31, is the combo meter.

### The opponent ids

`0xb37a` is the only byte in the whole 64 KiB that is constant through a
match and different in the next. Six ids came out of five matches of one
ladder run plus two plays of the tape, each confirmed against the name the
HUD prints:

| id | wrestler |
|---|---|
| 0 | Bret Hart |
| 1 | Razor Ramon |
| 2 | Undertaker |
| 3 | Yokozuna |
| 4 | Shawn Michaels |
| 5 | Bam Bam Bigelow |
| 8 | Lex Luger |

Doink was never drawn as an opponent, so 6 and 7 are unmeasured. In a
handicap match `0xb37a` holds whichever of the two is in the ring.

### What was looked for and not found

- **A wins counter.** Over 105 dumps of the whole system region, no byte
  holds `match - 1` at any point of the run, and no byte is constant inside
  a match and distinct across matches except `0xab14` itself and `0xb37a`.
  `0xa8c0` tracks `0xab14` but steps about ten shots earlier, on the screen
  that awards the match rather than on the first frame of the next one.
- **A combo hit counter.** `0x857c` counts attacks thrown, cumulatively,
  and never resets; it is not it. Nothing else moved (see below, no combo
  was ever made to register, so there was never a differential dump with a
  counted combo inside it).
- **Round or fall state.** `0xb36c` went 0 to 1 between the first and second
  fall of one match, but `0xb376` (2) and `0xb378` (1) were constant across
  both and do not read as a pin count. The falls are legible on the screen
  ("FIRST FALL AWARDED TO:", "MATCH AWARDED TO:") and in `p1_health` and
  `cpu_health` reaching 0, which is what `scenario.py` counts.


## The action id

A move is registered by its animation, not by its damage: step 3 wrote the
head grab off as "a bare super punch" because the two do the same damage,
and they do not do the same thing.

The word is `0x8548`. It was found by playing a bare P, a bare SP, the
Rolling Uppercut and the Super Flying Kick from one checkpoint and asking
`run.hunt` for the words that are `changed` between two moves and `equal`
across the frames of one move's swing. Two candidates came first and both
were wrong, which is the useful part:

- `0x8538` reads as a perfect id for one move and then shifts by exactly
  +30 over 30 frames: it is a frame counter with a per-move constant added,
  not an id. The protocol that kills it is sampling the *same* move at two
  offsets and requiring `equal`.
- `0xb2f2` is a perfect id at the `MEDIUM` bell (standing `0x2216`, P
  `0x1753`, SP `0x1755`, K `0x1655`, SK `0x1658`, the eye rake `0x1654`, the
  uppercut `0x5416`) and collapses at the VERY HARD bell, where a bare punch
  moves it not at all. It is shared scratch, not a per-wrestler field.

`0x8548` holds at both bells and for both wrestlers: the opponent's copy is
`0x87b8`, one object record on at a stride of `0x270` (624). The vocabulary
at point blank, measured in `_runs/2026-09-14T11-14-32Z-wwf-published`:

| id | what the player is doing |
|---|---|
| `0x2aee` | standing |
| `0x212c`, `0x2be8` | the two walks |
| `0x3022` | running |
| `0x7fbe` | blocking |
| `0x3268` | punch |
| `0x3d1e` | kick |
| `0x378c` | super punch |
| `0x4062` | super kick |
| `0x2b2e`, `0x2ee2`, `0x2ef6`, `0x2f54` | the recoveries a move ends in |
| `0x8692`, `0x872c`, `0x8950`, `0x81ca` | being hit |

The ids a move *may* pass through on its own are measured before it is
judged: each context is left alone for 260 frames and every id it reaches by
itself joins the neutral set, and every run of ids from the frame the player
first loses health is discarded. That is what separates a move from the
state the game was going to enter anyway -- without it the Sharpshooter
"registers" `0x3022`, which is the player getting to his feet after the
knockdown and comes out whatever button is pressed.

## The skills

OPTIONS is reached from the roster, not from the title: four taps of `down`
from the cursor's home on Doink land on it (three land on Bret Hart), `C`
opens it and `B` leaves it with the cursor back where it started. The screen
offers five skills, in this order:

    VERY EASY   EASY   MEDIUM (the default)   HARD   VERY HARD

`VERY HARD` is the one the proofs and the player use. The setting is a word
at `0xf4f0` reading 2, 4, 6, 8, 10 down that list; it is 6 at boot and it
survives to the bell, which is what makes it checkable there. The panel also
writes 2 to 6 into `0x9a4c` and `0x9a50` while it is open, and both read 0
once the match starts, so neither is the watch.

At VERY HARD the opponent is a different animal: standing still for 600
frames at the bell costs 143 of 164 health, and a bare super punch or super
kick from point blank is blocked for 0 damage where a punch still gets 12
through.

## The hunt

The game boots to a title, a roster, a title menu and then a match, and
nothing worth searching for exists until the bell. The route, from
`tash run --steps`/`--scenario` with `--shot` after every leg:

    title       run 837 to the logo
    roster      tap start (ignored for the first 20 frames the title is up)
    Bret Hart   down x3, then C
    title menu  C on INTERCONTINENTAL, the default highlight
    first bell  the clock is set to 99, then ticks to 98

The clock was the first thing found, because it is the only number on the
screen at the bell: a dump at 99 and a dump at 98 leave `0xaae6` and
`0xaae8`, and they are the two digits kept apart rather than one number --
tens and ones, not 99.

Health came from a differential dump around one kick. Two dumps of
`run.memory("system", 0, 0x10000)`, one before the kick and one after the
opponent's bar had shortened, differ in a handful of places, three of them
14 bytes apart from another three:

    0xb32e: 164 -> 144      the opponent's health
    0xb330: 164 -> 144      its copy
    0xb332: 104 ->  91      its bar

and the player's own three sit two records earlier at `0xb312`, `0xb314`,
`0xb316`, which is the stride the table above records. Walking the stride
out to six records is what showed the handicap matches' extra bodies.

The positions came from holding `right` for thirty frames and diffing: one
word climbs by 15 (`0xaabe`), one does not move (`0xaade`, the opponent),
and one follows the player at a distance (`0xaac2`, the camera).

The match number came from the ladder run: 105 dumps, one every time the
screen changed by more than 0.25, and one byte in all of them takes the
values 1, 2, 3, 4, 5 in order and holds each across a whole match.

## The moves

Bret Hart's published list (Goh_Billy and DKozoil on GameFAQs, JLowe's 32X
button legend), in this game's own notation: P punch, K kick, SP super
punch, SK super kick, BLK block, RUN run, F forward, B back, D down, HCF a
half circle from down to forward.

The pad legend on the OPTIONS screen (`6 BUTTON - A`) names the six:

| Genesis | libretro | function |
|---|---|---|
| X | `l` | P |
| Y | `x` | RUN |
| Z | `r` | SP |
| A | `y` | K |
| B | `b` | BLK |
| C | `a` | SK |

### What comes out, measured

From a checkpoint at a clean distance, one move per restore, reading
`cpu_health` before and after. "frames" is from the first input frame to
the frame the opponent's health first moves.

| move | inputs | frames | damage | landed |
|---|---|---|---|---|
| Punch | P | 10 | 20 | yes |
| Kick | K | 8 | 20 | yes |
| Super Punch | SP | 9 | 8 | yes |
| Super Kick | SK | 8 | 20 | yes |
| Rolling Uppercut | HCF + SP | 17 | 32 | yes |
| dash attack | F, F held, + SP | 30 to 60 | 46 | yes |
| dash attack | F, F held, + SK | 30 to 60 | 40 | yes |
| Eye Rake | HCF + P | 18 | 20 | no, a bare punch came out |
| Run Uppercut | D, D + P | 25 | 2 | no |
| Jump Kick | B, B + SK | 16 | 20 | no, a bare super kick; entered as three taps two frames apart it is the Super Flying Kick, 45 |
| Slam | B, B + P | 20 | 20 | no, a bare punch |
| Throw into ropes | B, B + SP | never | 0 | no |
| Head grab | F, F + SP | 17 | 8 | no, a bare super punch |

The diagonal is what separates a special from a button. `HCF + SP` is 32
damage where a bare SP is 8, and it is 32 every time at three different tap
lengths; the same move entered as `D, D, F` with no `DF` in between is a
bare super punch. A motion entered as separate taps with the stick back at
neutral when the button goes down never produced anything but the bare
button, at any distance from 16 to 65 pixels and at tap lengths of 2, 3, 4
and 6 frames with gaps of 1 to 8.

### There is a dash

`F` tapped and then `F` held is a dash, and it was missed for a long time
because the second `F` has to stay down. Thirty frames from a standing
start, from the same checkpoint:

| held | pixels moved |
|---|---|
| `F` | 15 |
| `F`, release 2, `F` held | 94 |
| `F` + RUN held | 168 |

The dash is what makes `F, F + SP` a different move: 46 damage instead of
8. RUN moves fastest but nothing pressed during a RUN ever connected (16
attempts, four buttons at two trigger distances, twice) -- Bret runs past
the opponent to the ropes.

### No combo was made to register

**Corrected by the third line:** the super combos do register, and this section is
what the reconnaissance tried without the gate's rules. See "The combo meter, and
what the last line got wrong about it" below.

The game prints a banner over the ring apron when something worth naming
happens. **Not once, in the reconnaissance or in the 57,600 trials below, did
a hit count appear**, and the combo table the brief asks for is therefore the
character's moves and nothing more. What the reconnaissance tried, all of it
against a `VERY EASY` opponent so that the attempt was not simply interrupted:

| attempt | inputs | frames | registered |
|---|---|---|---|
| chained buttons | P, K, SP, SK at a fixed spacing, 8 spacings from 3 to 24 frames | 12 to 100 | no; best 34 damage, which is two separate hits |
| dial-a-combo | hold F, tap one button six times, 4 buttons x 3 rhythms | 36 to 72 | no; exactly 20 damage every time, the first hit knocking the opponent down and the rest whiffing |
| grab then string | `F, F + SP`, then each of `F, F + SK/P/K/SP` and each of P/K/SK/SP tapped six times | 60 to 150 | no; 46 damage, the dash attack's own number, plus at most one more hit |
| starter then taps | `HCF + P/K/SK/SP` then one button tapped five times, two rhythms, 24 combinations | 40 to 90 | no; 37 damage at best, again two hits |
| super combo | the same from a full meter (`p1_meter` 16, "COMBO!" flashing) | 60 | no; the meter did not spend |
| motion prefixes | `FF`, `BB`, `DF`, `DD` before every attack button, at gaps 16, 26, 40, 45, 55, 65 | 12 to 25 | no; the bare button's damage every time |
| run-in | RUN held, then each attack at 25, 50 and 90 pixels | 40 to 200 | no; no contact at all |

`p1_meter` reaches 16 and the HUD replaces the meter bar with a flashing
"COMBO!", and it goes on to 17, so 16 is the point the text appears and not a
cap.

### Point blank is a band, not a distance

The reconnaissance read `REACH` as 85 and walked in until the gap was under
it, which is what cost it the grab. Walking a frame at a time from the bell
and restoring to try one bare punch at each frame:

| gap (`cpu_x - p1_x`) | what a bare punch does |
|---|---|
| over 90 | out of range, nothing |
| 70 to 90 | lands, 20 damage; the sprites touch at 70 |
| under 70 | nothing at all, at any button: the player has walked **into** the opponent and the two pass through each other |

So the approach every search below uses holds the gap inside 70 to 90 and,
because the opponent is live, only stops on a frame from which a bare punch
actually lands (`combos.approach`). A checkpoint taken at a gap of 84 that has
not been checked that way is a coin toss: at `MEDIUM` the opponent blocks, and
from the frame after a landed hit **no bare punch lands again for 600 frames**
of holding the band. That is the single most useful thing this step found, and
it is why every earlier "the string whiffed" reading is really "the opponent
was blocking".

### The search for the grab

`search.py` is the sweep: every
string of two to four inputs over the six buttons and the four directions
read from RAM (`P RUN SP K BLK SK F B D U`), at spacings of 2 and 6 frames,
holds of 2 frames, each string tried twice -- once all tapped, once with the
first input held through the rest -- from four checkpoints. It scores a trial
by the opponent's health, read a frame at a time, so a trial answers both the
damage and the frames each separate drop happened on; two drops no more than
30 frames apart count as linked.

| context | how it is built | strings | trials | beat 40 damage | linked two hits |
|---|---|---|---|---|---|
| `standing` | the approach above, lengths 2, 3 and 4 | 11,100 | 44,400 | 82 | 1,374 |
| `stunned` | from `standing`, one landed punch, the frame the health moves; lengths 2 and 3 | 1,100 | 4,400 | 0 | 0 |
| `grounded` | from `standing`, a landed super kick and 24 frames; lengths 2 and 3 | 1,100 | 4,400 | 0 | 0 |
| `ropes` | the opponent punched along to `cpu_x` 1270, 48 from the ring's right bound; lengths 2 and 3 | 1,100 | 4,400 | 0 | 0 |

**57,600 trials, and the table of strings whose damage exceeds the best single
move (46) is empty.** The closest results:

| context | best | string | hits |
|---|---|---|---|
| `standing` | 45 | `RUN+B+F+SK @6`, and 79 more strings ending in a RUN kick | one |
| `standing` | 16 | `F+SP+SP+SP @6 sustained` | three, at frames 9, 25 and 42 |
| `standing` | 32 | `P+SP+P+SK @6` | two, at frames 10 and 39 |
| `stunned` | 13 | `RUN+SP+SP @6` | one |
| `grounded` | 22 | `U+RUN+RUN @6` | one |
| `ropes` | 30 | `U+RUN+SK @6` | one |

Of the 1,456 trials worth printing, 82 were a single hit, 1,371 were two hits
and **three were three hits** -- all of them `SP` tapped three times with a
direction held, 16 damage in total, the drops 14 to 17 frames apart. Nothing
in the space put two drops closer than 13 frames, which is what a combo would
look like.

The sweep splits by opening symbol and by context (`WWF_OPENERS`,
`WWF_CONTEXTS`, `WWF_LENGTHS`), which is how the 57,600 trials were run as
twenty processes in about half an hour instead of two and a half hours in one.

### Bret Hart's published list, as step 3 read it

The table below is step 3's, kept because it is measured; it judges a move
by its damage, which is why it writes the head grab off. The section after it
judges by the action id and supersedes it.

`published.py` at the time played every combo the guides name (BOdom and Goh_Billy on
GameFAQs, the Cheatbook console list) at every spacing from 2 to 12 frames.
`n/d` below is the linked hits and the damage; the dash rows tap forward, hold
it and run in from the bell rather than starting at point blank.

| published as | best result |
|---|---|
| grab (hold F, tap P) x6 | **three hits, 30 damage**, at spacings 8 to 11 |
| grab, then F+K | one hit, 20 |
| dash + SK ("FF SK, 5 hits") | one hit, 27, identical at all eleven spacings |
| dash + SK + P / + SP / + SP,P,K | one hit, 27; the follow-ups never come out |
| dash + P+K, dash + P+SP, and the arcade's FF PK PP P K | nothing connects |
| P+K, P+K as a pair | one hit, 33, only at spacings 3 and 4 |
| Eye poking `D, F, P` | nothing |
| **Super flying kick `B, B, SK`** | **one hit, 45 damage, at spacing 2 only** |
| Super uppercut `D, B, P` | nothing |
| mini combo `BLK+K, K, SK` and `BLK+SK, SK, K` | one hit, 20, at spacings 4 and 5 |
| `P, P, K, SK` | two hits, 32, at spacings 5 and 6 |

The published five-, eight-, thirteen- and sixteen-hit combos do not exist on
this build at any spacing this harness can enter. The two published inputs
that do pay are the grab string and the super flying kick, and both are in the
table below.

### Bret Hart's published list, move by move

`published.py` plays every row of the published list from a checkpoint at
the context the list asks for, at every spacing from 2 to 12 frames, and at
every lead from 8 to 44 frames for the moves taken during the hold. The list
is the union of the Genesis FAQ by Fire_Pro_Fan, JLowe's 32X list, the
PlayStation lists and Cheatbook. A move is registered when it plays an
action id of its own: an id it holds for ten frames or more, that the four
bare buttons do not reach, that the context does not reach on its own, and
that starts before the player is first hit. `step` is the spacing in frames,
or the lead after the grab for the during-the-hold rows, or the frames waited
over the fallen body for the Sharpshooter. The run is
`_runs/2026-09-14T11-14-32Z-wwf-published` (27,630 frames, 22 rows, VERY
HARD, from `tapes/title-to-first-bell-hardest.yaml`).

| move | inputs as published | action id | step | first hit | damage | hits | meter |
|---|---|---|---|---|---|---|---|
| P | the button alone | 0x3268 | -- | 9 | 12 | 1 | 0 |
| K | the button alone | 0x3d1e | -- | 10 | 21 | 1 | 0 |
| SP | the button alone | 0x378c | -- | -1 | 0 | 0 | 0 |
| SK | the button alone | 0x4062 | -- | -1 | 0 | 0 | 0 |
| Rolling Uppercut | D, DF, F + SP | 0x7de2 | 3 | 20 | 32 | 1 | 1 |
| Eye Rake | D, DF, F + P | 0x7a3a | 2 | 43 | 26 | 1 | 1 |
| Eye Rake, P held | hold P about 3 s | did not register | 2 | 9 | 12 | 1 | 0 |
| Quick Uppercut | D, D + P | 0x32d8 | 3 | 22 | 32 | 1 | 1 |
| Super Flying Kick | B, B + SK | 0x3fa4 | 6 | 32 | 45 | 1 | 1 |
| Arm Drag | B, B + P, in close | 0x5bae | 2 | 60 | 22 | 1 | 1 |
| Backbreaker | D, D + SK | did not register | 3 | 20 | 27 | 1 | 1 |
| DDT | RUN held, SP | 0x6c84 | 2 | 37 | 33 | 1 | 1 |
| Head grab | F, F + SP | 0x649e | 2 | -1 | 0 | 0 | 1 |
| Head grab, dashed | F, F + SP off a dash | did not register | 2 | -1 | 0 | 0 | 0 |
| Sharpshooter, SP | SP at the feet | did not register | 2 | -1 | 0 | 0 | 1 |
| Sharpshooter, SK held | hold SK at the feet | did not register | 2 | -1 | 0 | 0 | 1 |
| grab string | F+P tapped six times | did not register | 7 | 9 | 22 | 3 | 0 |
| punch string | P, P, K, SK | did not register | 4 | 9 | 24 | 2 | 1 |
| Head Slam, during the hold | the grab, then P | 0x4748 | 32 | 47 | 20 | 1 | 2 |
| Head Slam, during the hold, F+P | the grab, then F + P | 0x4a22 | 32 | 53 | 12 | 1 | 3 |
| Bulldog, during the hold | the grab, then D + SK | did not register | 8 | -1 | 0 | 0 | 1 |
| Uppercut, during the hold | the grab, then D + SP | 0x3428 | 32 | 50 | 16 | 1 | 2 |
| Face Slam, during the hold | the grab, then D, DF, F + P tapped four times | 0x74d4 | 38 | 85 | 29 | 1 | 2 |
| post-grab string | the grab, then F held, P tapped five times, SK | 0x4a22 | 20 | 53 | 17 | 2 | 3 |
| post-grab 16-hit string | the grab, then F, F + SK, SP, P, K, SK | 0x4748 | 8 | 47 | 20 | 1 | 2 |
| combo string | the grab, then P and K tapped eight times | 0x4748 | 26 | 47 | 34 | 1 | 2 |

Fourteen of the twenty-two published rows register, eleven of them with an
animation no other row reaches. What the refusals were tried with:

- **Eye Rake with P held** -- P held 180 frames at point blank, once; the
  punch comes out on the press and nothing follows it. The quarter circle
  version does register, so the rake exists and the hold is not its input on
  this build.
- **Backbreaker** -- `D, D + SK` at all eleven spacings; every one is a bare
  super kick. Its damage at spacing 3 (27) is the super kick landing, not a
  backbreaker.
- **Head grab off a dash** -- the dash closes and the SP is bare. The grab
  wants the two forward taps from a standstill.
- **Sharpshooter** -- from a real knockdown (a Super Flying Kick for 45,
  `cpu_action` `0xedf4`, then a walk to a gap of 20) at eleven waits from 20
  to 120 frames over the body, both as SP and as SK held 60 frames. Nothing
  but the get-up states the context reaches by itself.
- **Bulldog during the hold** -- `D + SK` at all seven leads; the hold simply
  runs out.
- **the grab string and the punch string** -- they chain (three hits for 22,
  two for 24) but every animation in them is a bare punch or kick, so by the
  action-id rule they are not moves of their own. Both stay in the table
  because they land.

`prove.py` replays the table at VERY HARD and every row reproduces its own
id, its frame to the first hit, its damage and its hit count exactly
(`_runs/2026-09-14T11-16-40Z-wwf-prove`, 20,418 frames, 19 rows, 19 pass).

### The combo meter, and what the last line got wrong about it

`p1_meter` is the right word and the reading of it was wrong. The address is
record 0's +10, and the player is not always record 0, so the meter is

    0xb312 + 14 * word(object + 0x5c) + 10

and `0xb31c` is that expression for a match the player loads into slot 0.
Record +12 is the value the bar is drawn from:

    drawn = (meter * 0xe1e1 + 0x8000) >> 16       ROM $3eb18

which is `round(meter * 0.88184)`; the old README's "+12, the meter less
two" is that formula read at meter 16 and nowhere else.

It is not a chain counter and it does not fall back to 0 when a chain
breaks. Measured from one checkpoint:

- it does not move over 1,200 frames of standing still;
- it survives a lost fall and the bell after it (meter 16 was still 16
  3,672 frames later with both wrestlers back to 164);
- it rises by one **per qualifying move started**, not per landed hit: a
  Super Kick thrown at a gap of 210 ring units, hitting nothing, still
  scores its +1;
- spamming Super Kick at range, the shortest gap between two ticks is 32
  frames.

The last line reached 3 because it spent its trials on chains out of the
grab and read the number back as a hit count; the third slam of a string
is +3 because a slam is worth 3, not because three hits landed.

#### What each row is worth

Measured at VERY EASY from one checkpoint, one row a restore
(`gain.py`): damage, the frames the row costs, and the meter it adds.

| row | damage | frames | meter |
|---|---|---|---|
| Punch | 18 | 82 | +0 |
| Kick | 14 | 82 | +0 |
| Super Punch | 46 | 82 | +1 |
| Super Kick | 40 | 82 | +1 |
| Rolling Uppercut | 47 | 92 | +1 |
| Eye Rake | 39 | 90 | +1 |
| Quick Uppercut | 47 | 87 | +1 |
| Super Flying Kick | 0 | 98 | +1 |
| Arm Drag | 0 | 82 | +0 |
| DDT | 0 | 82 | +0 |
| head grab | 0 | 86 | +1 |
| Head Slam | 29 | 122 | +2 |
| Slam | 18 | 122 | +3 |
| Uppercut in the hold | 24 | 122 | +2 |
| Face Slam | 43 | 158 | +2 |
| post-grab string | 34 | 140 | +3 |
| combo string | 29 | 210 | +2 |
| punch string | 35 | 100 | +1 |
| grab string | 18 | 120 | +0 |

Sixteen is the number to reach, so the cheapest fill is slams and the
cheapest fill that also hurts is Super Punch and Rolling Uppercut; the
player does not choose from this table by hand, `climb.Climb` hands the
whole table to `tash.search` with `60 * meter - 2 * health - 0.1 * band`
a frame as the score and plays whatever comes first.

### The gate: ROM $3ef52

One subroutine reads the meter, and every combo in the game asks it first.

    $3ef52  move.w  $5c(a0), d1          the wrestler's record index
    $3ef56  move.w  $abf8.w, d7
    $3ef64  mulu.w  #$e, d1
    $3ef68  addi.l  #$ffb312, d1
    $3ef6e  move.w  $a(a6, d1.l), d7     the meter
    $3ef72  move.w  $b23e.w, d1          the infinite-combo cheat flag
    $3ef76  bne.b   $3ef7e
    $3ef78  move.w  #$10, d1             the threshold, 16
    $3ef7e  moveq   #$0, d1
    $3ef80  cmp.w   d1, d7
    $3ef82  rts                          the caller branches blt to refuse

`#$10` is the threshold the user's "100%" means, and it appears again at
$3f01a. `$b23e` is the cheat that drops it to 0 -- an infinite-combo
switch, off in a normal game. The charge is $3ef84 to $3eff4
(`add.w d3, d2` into `$a(a6, d5.l)`, then `jsr $3eb18` and the result
into `$c(a6, d5.l)`), and $3ecf8 zeroes both when a wrestler is reset.

Every read of the meter word in the ROM is this one, and the only writes
are the reset at $3ecf8, the charge at $3efda, and two script opcodes at
$41624 and $41632 that set it to 0 and to 10. There are exactly 33
`jsr $3ef52` sites and they are the whole of what a full meter is for:

| sites | where | what a full meter does there |
|---|---|---|
| 31 | the eight characters' combo listeners (each asks twice, once before the direction taps and once before it commits; one of the sixteen has no pre-gate) | the super combo comes out |
| 1 | `$e392`, the **opponent's** brain | the opponent throws its own combo out of its own hold |
| 1 | `$392d2`, a wrestler on the mat | the pinned player may rise, once, on the fall that would lose the match |

### The bar under the health, and COMBO!

The second bar, rows 24 to 31 of the frame, is the meter. The player's
runs from x 58 to x 111 and **fills from the right**: its interior is rows
26 to 29, `(168, 32, 96)` against `(32, 32, 64)`, and the purple part is

    4 * drawn - 2 pixels wide, ending at x 109

so the crop `58,26,54,4` holds `16 * drawn - 8` purple pixels and nothing
else -- the opponent's bar is the mirror image on the right of the screen
and fills the other way. `run.colours("58,26,54,4", (168, 32, 96))` counted
off a meter walked up one super punch at a time reads 8, 24, 40, 72, 88,
104 and 121 at drawn 1, 2, 3, 5, 6, 7 and 8, which is that line to within
a pixel. At the charged checkpoint, drawn 17, it reads **0**: the bar is
gone and a flashing `COMBO!` stands in its place. drawn 14 is meter 16 and
no lower meter reaches it -- meter 15 draws 13 -- so the flash the user saw
and the `#$10` the gate loads are the same instant. (`run.colours` landed
on main with v6 step 1; before the rebase the same counts were taken by
decoding the PNGs `look()` wrote.)

### The opponent charges one too, and uses it

`$f57a` and `$f57e` are the two wrestlers' state words as the opponent's
brain reads them; `$f57e` tracked the player's `$64(a0)` exactly through a
whole fall, and `$f57a` read `0x10` on the frames the player's state was
`0x11`. At $e382 the brain asks

    $e382  cmpi.w #$10, $f57a.w      am I holding him
    $e392  jsr    $3ef52.l           is my meter full
    $e398  blt.b  $e3a6
    $e39c  movea.l #$f956, a1        then run this script list

so the opponent throws its own combo out of its own hold on a full meter.
`cpu_meter` (`0xb32e + 10` = `0xb338`) climbs through a match like the
player's. That is the second reason to counter a hold and not sit in it.

### The eight combo listeners, read out of the ROM

A wrestler is a coroutine list, and two of every character's coroutines are
combo listeners. Each one has the same shape:

1. wait until `$64(a0)` is `0x10` (this wrestler has the other in a hold),
2. `jsr $3ef52`, `blt` out -- the gate,
3. the edge-triggered pad word `$190(a0)` reads `#$8`, toward the opponent,
4. `#$8` again within `#$3c` frames (60),
5. `andi.w` a mask over `$190(a0)` and `cmpi.w` the attack button,
6. `jsr $3ef52`, `blt` out -- the gate again,
7. `$142(a0)` is 0 and the state is still `0x10` or `0x11`,
8. `move.l #$4xxxx, $10c(a0)` -- the combo script starts.

So every initiator in the game is *forward, forward, an attack button from a
hold, on a full meter*, which is what the published guides say in words:
Goh_Billy's FAQ gives the universal grapple as `f, f, HP` and BOdom's gives
the combo as a grab and then an `f, f` initiator. The button each listener
waits for, decoded from the mask and the compare:

| coroutine list | listener | listener |
|---|---|---|
| 1 (Bret Hart, measured) | `$2ac18` F,F,P | `$2ad00` F,F,SK |
| 2 | `$2cf68` F,F,SP | `$2d036` F,F,K |
| 3 | `$2f27e` F,F,SK | `$2f372` F,F,K |
| 4 | `$313d0` F,F,SP | `$314d0` F,F,P |
| 5 | `$33b86` (no pre-gate) | `$33cc6` F,F,K |
| 6 | `$358de` F,F,SP | `$359e0` F,F,P |
| 7 | `$378a2` F,F,SP | `$3798a` F,F,SK |
| 8 | `$3a462` F,F,SK | `$3a562` F,F,K |

Eight lists, two listeners each, two gate calls each but for one listener
that has no pre-gate: 31 of the 33 `jsr $3ef52` sites in the ROM. Which list belongs to which name was not
resolved -- no pointer table from the roster to the coroutine lists was
found, and only Bret Hart's pair was confirmed by playing it -- so the table
is the shape of the roster's combos, not a per-name move list. Bret's pair
agrees with BOdom's `FF PK` (hard kick, our SK) and Goh_Billy's `f, f, LP`
(our P).

`$2add6` is a ninth listener of the same shape with **no gate**: it accepts
state `0x11` (being held) as well as `0x10`, wants F, F, SP, and when the
state is `0x11` it writes `#$10` into its own `$64(a0)`, puts `#$f` into the
opponent's `$142`, and starts `$470da`. That is the reversal.

### The pad word, and why the combo would not come out

`$190(a0)` is the object's edge-triggered pad: it holds a code for exactly
one frame, one frame after the press. The codes are **relative to the
opponent's side**, not absolute: `0x8` is toward him and `0x4` away, `0x2`
down, `0x1` up, and `0x10` P, `0x20` BLK, `0x40` SP, `0x80` K, `0x90` RUN,
`0x100` SK.

Deriving forward from `cpu_x - p1_x`, which is what `combos.forward` does,
is wrong inside a hold: the two bodies overlap and the sign is noise, and
`$13e(a0)` is a walk direction, not a facing (it reads 0 all through a
hold). `Ring.toward` taps a direction, reads the code the object saw, and
uses the other one when it was not `0x8`. That single change took the
initiator's fire rate from 7 in 17 to **6 in 6**.

The listener also needs a rhythm. Sweeping six hold checkpoints against
twenty cadences: a tap shorter than 2 frames, or a gap shorter than 2
frames, never registers at all; 2 and 2 is the cheapest that always does.
`ring.TAP_HOLD` and `ring.TAP_GAP` are those two numbers.

### The pin: a full meter buys one rise, on the fall that decides it

The user's account -- `COMBO!` gives a last-minute chance to rise once when
pinned -- is exactly right, and it took a two-by-two to see it. The 33rd
`jsr $3ef52` is at **$392d2**, inside the routine that decides what a
wrestler on the mat may do:

    $392c8  cmpi.w #$2, $5c(a5)      one of the two front records
    $392ce  bge.b  $392fc
    $392d0  movea.l a5, a0
    $392d2  jsr    $3ef52.l          the meter against #$10
    $392d8  blt.b  $392fc
    $392da  move.w $66(a5), d7       three more words, all of them 0
    $392e0  move.w $b442.w, d7
    $392e6  move.w $ab38.w, d7
    $392ec  clr.w  $180(a5)
    $392f0  ori.l  #$1000, $78(a5)   the rise is offered
    ...
    $392fc  ori.l  #$2000, $78(a5)   it is not

Measured at VERY HARD, four checkpoints that differ in the meter and in
which fall is being lost, each played twice -- mashing all four attack
buttons two frames down and two frames up, and standing still:

| the fall | meter | input | taps to the feet | meter after | health after | opponent |
|---|---|---|---|---|---|---|
| the first | 19 | mash | 101 | 19 | 164, a fresh fall | 164 |
| the first | 19 | still | 127 | 19 | 164, a fresh fall | 164 |
| the second | 19 | mash | **8** | **0** | **1, the same fall** | 164 |
| the second | 19 | still | never | 0 | 0, the match lost | 0 |
| the first | 0 | mash | 107 | 0 | 164, a fresh fall | 164 |
| the first | 0 | still | 133 | 0 | 164, a fresh fall | 164 |
| the second | 0 | mash | 200 | 0 | 164, a fresh fall | 164 |
| the second | 0 | still | never | 0 | 0, the match lost | 0 |

Both conditions are needed. On the first fall a full meter buys nothing:
the fall is lost, the health comes back at 164 and the meter is untouched.
On the fall that would lose the match, an empty meter buys nothing either.
With both, eight taps put the player back on his feet **inside the same
fall** with 1 health and an opponent still at 164, and the meter is spent
to 0 -- which is the proof the rise is what spent it. The referee never
reaches his count.

Mashing is worth something at every pin even without the meter: 107 taps
against 133 standing still, about a hundred frames. `comboline.Player`
mashes whenever `$64(a0)` reads `0x9` and counts a rise whenever the meter
crosses from full to empty while it does.

### Bret Hart's super combos, proved and refuted

Run `examples/wwf/provesupers.py`: the tape to VERY HARD, `climb.Climb`
fills the meter with a search (14 decisions, 22 trials each, meter 19 by
the last), the fall is played out so the next bell starts with both
wrestlers at 164 and the meter kept, and each row is played from a hold
taken at that bell. `Chain` reads the animation word and both healths
every frame, so a hit is a frame the opponent's health falls on.

The run is `_runs/2026-09-14T15-05-03Z-wwf-supers`: seventeen verdicts, of
which the two F,F,SK rows are recorded as the failures they are, because a
refutation with its evidence is the verdict.

| row (published) | fires | evidence at VERY HARD |
|---|---|---|
| `F,F,P` then `SK,SP,P,K,SK` twice, four taps each -- BOdom's 16-hit shape doubled | yes, `0x310a` | meter 21 to 3, **9 hits for 81 damage**, 0 taken, hits at 17, 34, 51, 65, 77, 94, 135, 179, 255 |
| `F,F,P` then `SK,SP,P,K,SK` -- BOdom's "(grab), FF PK PP P K PK" | yes, `0x310a` | meter 21 to 1, **7 hits for 50 damage**, 0 taken, hits at 17, 34, 51, 65, 77, 94, 135 |
| `F,F,P` alone | yes, `0x310a` | meter 21 to 0, 5 hits for 30 damage, 27 taken |
| `F,F,SK` then the published branch | **no** | meter 21 unchanged, 0 hits, 57 taken; `0x310a`/`0x3d96` never appear |
| `F,F,SK` alone | **no** | meter 21 unchanged, 0 hits, 42 taken |
| `F,F,SP` (the reversal button, from the hold) | not a combo | `0x70da`, 1 hit for 40 damage, **meter 21 to 22** -- ungated, and it spends nothing |
| `F,F,K` | **no**, and the ROM agrees | meter unchanged, 0 hits, 57 taken; Bret's coroutine list has no `K` listener |
| `F,F,P` + branch at meter 1 | **no**, refused by the gate | `0x4748`, the ordinary Head Slam, 3 hits for 41 damage and 66 taken; `0x310a` absent |
| `F,F,SK` at meter 1 | **no**, refused by the gate | nothing at all, 0 hits, 87 taken |

`F,F,SK` is the one disagreement between the ROM and the cartridge as
played. Its listener at `$2acee` is real and it fired six times out of six
at VERY EASY in an earlier probe (5 hits, 44 damage, meter 21 to 0); from
the VERY HARD holds above it never took, in six attempts over two rows.
The refutation is the measurement, not a claim about the listener.

The branch matters and the rhythm matters. Sweeping both from one hold --
six branches, three tap counts, three cadences, forward held or not, 242
trials -- the best is `F,F,P` and the published branch played twice, ten
taps a button at three frames down and three up, forward held: 100 damage.
Four taps at three and three, forward let go, is 81 damage for no health
lost, and that is what `supers.FOLLOWS[0]` and `supers.CHAIN_TAPS` are.
Every branch is worth more than the bare initiator and no branch fires
anything the initiator does not.

### The reversal, and why the player always uses it

`$2add6` accepts state `0x11`, asks no gate, and answers F, F, SP with
`0x70da`, taking the hold back. `provesupers.prove_reversal` waits at the
VERY HARD bell for the opponent to take a hold, checkpoints the frame the
state word turns `0x11`, and plays that frame twice -- once reversing,
once standing in the hold for the same 260 frames:

| how the hold was taken | held after | reversed | stood still |
|---|---|---|---|
| standing after the approach | 47 frames | **lost 16**, dealt 40, `0x70da` | lost 70, dealt 0 |
| holding BLK after the approach | 736 frames | **lost 0**, dealt 27, `0x70da` | lost 43, dealt 0 |

At VERY EASY the same A/B over two holds (`counter.py`) reads lost 8
against 43 and lost 0 against 20, with the opponent going 164 to 124 both
times. So the counter is worth 20 to 54 health and 27 to 40 damage each
time it lands, and it costs no meter.

Which hold it is matters: a sweep of six ways to provoke one at VERY HARD
(`provoke.py`) found four, and F,F,SP answered two of them -- the two
above. Walking in and pressing the head grab also end in the opponent's
hold, and from those two `0x70da` never came out, so the reversal is not
unconditional. `comboline.Player.react` plays it on the first frame
`$64(a0)` reads `0x11`, which is why the line's reversal count is larger
than its combo count.

### Sources for the published lists

GameFAQs refuses fetches from this box (403), so both guides were read
through search snippets:

- BOdom, *Move List and Guide*, arcade and SNES
  (`gamefaqs.gamespot.com/arcade/563194-wwf-wrestlemania/faqs/1176`):
  Bret Hart's 16-hit combo as "(grab), FF PK PP P K PK", read as
  grab, the hard-kick initiator, then kicks, punches, a slam, two more
  slams and three back breakers; and the general shape
  "(punches/kicks) (more punches/kicks) (slam) (more slams) (special)".
- Goh_Billy, *Guide and Move List* v8.0, arcade and SNES
  (`gamefaqs.gamespot.com/arcade/563194-wwf-wrestlemania/faqs/42348`):
  the universal grapple `f, f, HP`; "combo meter will always fill for
  Yokozuna, Undertaker, Shawn, Bret, and Lex"; "Pausing in between taps
  will cause the string to end early"; "strings do not require a combo
  meter"; "Strings can also be reversed".

The cartridge agrees on the shape and disagrees on the size; the tables
above are the measurement.

### The hunt for a hit counter

**Corrected by the third line:** there is no hit counter, and there did not
need to be one -- the meter counts moves started, not hits landed.

Decision 4 of the plan gives the first registered combo to `run.hunt` to find
the counter. No combo registered, so the hunt was run against the closest
thing there is -- the grab string, three drops in 50 frames -- two ways:

- a differential hunt (`equal` while idle, `increased` on each drop, `equal`
  for 120 frames after) narrows 32,768 words to 13, none of which reads a hit
  count: `0x8614` 25, `0x8634` 30, `0x8636` 91, `0x863a` 50, and nine more in
  the same `0x85xx`--`0x88xx` block the recon's `0x857c` attack tally lives in.
- a value hunt (`value 0` while idle, then `value 1`, `value 2`, `value 3` on
  the three drops) leaves 29 words after the first drop, one after the second
  (`0xfff0`, stack) and **none** after the third.

So there is no word that counts these hits, and `profile.yaml` gains no
`p1_hits` watch. The picture says the same: the top 40 rows of the HUD were
filmed every second frame through the whole string (names, bars, clock, and
nothing else appears), and the apron banner prints `FIRST ATTACK!` on the
first drop and `2X DAMAGE` later, neither of which is a count -- `2X DAMAGE`
shows up after single punches too.

### The table as code

`combos.py` is the table as code -- a row is a name, its inputs as
`(symbol, hold frames, gap frames)` triples, the frames to its first hit, its
damage, its hits and its action id -- plus `approach(run)` and
`perform(run, combo)`, which reads forward out of `cpu_x - p1_x` on the frame
each input goes in. The symbols are the six buttons (`P`, `SP`, `K`, `SK`,
`BLK`, `RUN`), the four directions (`F`, `B`, `D`, `U`), the diagonal `DF`
(down and forward held together), `-` for a wait, and any of them joined with
`+`. Nineteen rows, every number from
`_runs/2026-09-14T11-14-32Z-wwf-published`:

| move | inputs | frames | damage | hits | action |
|---|---|---|---|---|---|
| Punch | P | 9 | 12 | 1 | 0x3268 |
| Kick | K | 10 | 21 | 1 | 0x3d1e |
| Super Punch | SP | -1 | 0 | 0 | 0x378c |
| Super Kick | SK | -1 | 0 | 0 | 0x4062 |
| Rolling Uppercut | D, DF, F+SP at a gap of 3 | 20 | 32 | 1 | 0x7de2 |
| Eye Rake | D, DF, F+P at a gap of 2 | 43 | 26 | 1 | 0x7a3a |
| Quick Uppercut | D, D+P at a gap of 3 | 22 | 32 | 1 | 0x32d8 |
| Super Flying Kick | B, B, SK at a gap of 6 | 32 | 45 | 1 | 0x3fa4 |
| Arm Drag | B, B, P at a gap of 2 | 60 | 22 | 1 | 0x5bae |
| DDT | RUN and forward held 20 frames, then RUN+SP | 57 | 33 | 1 | 0x6c84 |
| Head grab | F, F+SP | -1 | 0 | 0 | 0x649e |
| Head Slam | the grab, 32 frames, P | 47 | 20 | 1 | 0x4748 |
| Slam | the grab, 32 frames, F+P | 53 | 12 | 1 | 0x4a22 |
| Uppercut in the hold | the grab, 32 frames, D+SP | 50 | 16 | 1 | 0x3428 |
| Face Slam | the grab, 38 frames, D, DF, F+P, P, P, P | 85 | 29 | 1 | 0x74d4 |
| post-grab string | the grab, 20 frames, F+P x5, SK | 53 | 17 | 2 | 0x4a22 |
| combo string | the grab, 26 frames, P and K tapped eight times | 47 | 34 | 1 | 0x4748 |
| grab string | F+P tapped six times at a gap of 7 | 9 | 22 | 3 | 0x3268 |
| punch string | P, P, K, SK at a gap of 4 | 9 | 24 | 2 | 0x3268 |

    tash run --profile examples/wwf/profile.yaml \
             --scenario examples/wwf/prove.py --bundle _runs

plays all nineteen from the VERY HARD bell, 38 verdicts, all passed
(`_runs/2026-09-14T11-16-40Z-wwf-prove`, 20,418 frames): each row reaches its
own action id and reproduces its frame, its damage and its hits to the
number. Every row but the DDT is played from point blank; the DDT is the one
move that wants room, and it is played from the bell, where the wrestlers
stand 207 apart -- RUN and forward held 20 frames carry the whole gap, and a
longer hold arrives, stops, and the super punch comes out bare.

The dash attack the reconnaissance measured at 46 is **not** in the table:
launched from the bell checkpoint with a back-step long enough to make room
(20 to 60 frames) and the attack button at every point of the run-up, it
never did better than 20, so its 46 is not reproducible from this checkpoint
and no row can claim it.

## What an input costs

From a checkpoint at the bell, one button at a time, watching `p1_x` and
the roster cursor:

| held for | registers |
|---|---|
| 1 frame | never, anywhere: not on the roster, not in the ring |
| 2 frames | yes; the walk moves 14 pixels |
| 3 frames | yes; 22 pixels |
| 4 frames | yes; 29 pixels |

A tap is therefore two frames, and the tape holds every button for four to
leave margin.

## The tape

`tapes/title-to-first-bell.yaml`, four segments:

| segment | anchor | waited | transitions |
|---|---|---|---|
| `title` | perceptual hash `59c80a81f17e6796` within 4 | 837 | start |
| `roster` | template of the roster's right column, `x 128 y 60 30x90`, at 0.98 | 25 | down x3, C |
| `title-select` | exact hash `f570a00716868e82` | 150 | C |
| `first-bell` | watch `timer_ones` equal 8 | 666 | none |

The title's backdrop keeps moving, so the exact hash never settles and the
perceptual one does; the roster's big portrait animates and the right-hand
panel blinks, so only a crop of the portrait grid is still, and it is taken
from the right column because the cursor sits in the left one. The title
menu is the one screen that holds an exact hash. Start is ignored for the
first twenty frames the title is up, which cost one debugging round: a tap
at the anchor frame does nothing and the tape sits on the title until it
times out, so the segment waits thirty frames before pressing.

Played twice from power on:

    tape   examples/wwf/tapes/title-to-first-bell.yaml
           (4 segments, 1781 frames, 12 transitions, 0 retries)

both times, with the same watches at the end (`p1_health` 164, `cpu_health`
164, `timer_tens` 9, `timer_ones` 8, `match` 1, `opponent` 2, `p1_x` 989,
`cpu_x` 1196) and the same last frame: the two `--shot` PNGs are
byte-identical, md5 `0a7e084b7c2ddde7db7a13e2ef8bb306`.

### The same walk at VERY HARD

`tapes/title-to-first-bell-hardest.yaml` is that tape with a visit to
OPTIONS in the middle, five segments:

| segment | anchor | frames | transitions |
|---|---|---|---|
| `title` | perceptual hash `59c80a81f17e6796` within 4 | 40 | start, twice |
| `roster` | template `title-to-first-bell.anchors/roster.png`, `x 128 y 60 30x90`, at 0.98 | 110 | down x4, C, right x2 |
| `skill-set` | watch `skill` equal 10 | 80 | B, down x3, C |
| `title-select` | exact hash `f570a00716868e82` | 12 | C |
| `first-bell` | watch `timer_ones` equal 8 | 1 | none |

The two `right` taps on the roster move the cursor back to Bret Hart from
Doink, where leaving OPTIONS puts it; the `skill-set` segment anchors on the
skill word itself rather than on a picture, because the OPTIONS panel's text
is the only thing that changes when the setting does. Played twice from power
on:

    tape   examples/wwf/tapes/title-to-first-bell-hardest.yaml
           (5 segments, 1915 frames, 28 transitions, 0 retries)

both times to the same last frame, exact hash `419e442e82ae16d1`, with
`p1_health` 164, `cpu_health` 164, `timer_tens` 9, `timer_ones` 8, `match` 1,
`p1_x` 989, `cpu_x` 1196 and `skill` 10
(`_runs/2026-09-14T12-08-21Z-wwf-hardest-tape` and
`_runs/2026-09-14T12-08-23Z-wwf-hardest-tape`). The opponent is `1` (Razor Ramon)
rather than the `2` (Undertaker) the plain tape draws: the title screen is
confirmed on a different frame, and the draw follows the frame.

## Determinism

From one checkpoint at the bell, the same seven inputs over 132 frames,
three times, then the same inputs started one, two and three frames later:

| run | exact hash of the last frame | cpu health | p1 health |
|---|---|---|---|
| as recorded | `472698d5810aa870` | 146 | 143 |
| again | `472698d5810aa870` | 146 | 143 |
| again | `472698d5810aa870` | 146 | 143 |
| one frame later | `a9ee9422a1d40bbd` | 164 | 99 |
| two frames later | `5653e16647047a8e` | 146 | 143 |
| three frames later | `28a2a7e31092a645` | 164 | 143 |

So the core is exactly reproducible from a checkpoint, and the opponent is
not a fixed script: a single frame of delay is the difference between
landing two attacks and landing none while taking 44 damage. The opponent
reacts to where the player is, and a plan cannot hold a schedule of frames
-- it has to read the state back every decision.

The ladder's first opponent is not fixed either. Three boots that reached
the title menu at three different frames drew Lex Luger, Bret Hart and
Undertaker as match 1. The tape draws Undertaker both times it is played,
so it is a function of the frame the title is confirmed on and not of a
free-running seed.

## A match played crudely

`scenario.py`: play the tape, then walk toward whoever is standing and kick
whenever the gap is 85 pixels or less, which is the reach a punch or a kick
connects at. That is the whole player.

    crude match: 5128 frames, 2 falls, match watch 0
    ran 6909 frames in 5.49 s, 1258.7 fps, 21.01x real time

Two falls in 5,128 frames, both of them the player's, so mashing **loses**
match 1 at the default `MEDIUM` skill. Four other crude loops lose the same
way: kick-mashing, hit-and-run with SK, hit-and-run with a motion super
punch, and blocking between attacks all ended with `p1_health` 0, the
longest of them after 17 exchanges and 1,759 frames.

A match is best two falls of three: the ring overlay says so ("MATCH n /
BEST 2 out of 3 FALLS"), a fall is one wrestler's health reaching 0 and the
other pinning him, and the game prints "FIRST FALL AWARDED TO:" and then
"MATCH AWARDED TO:". Losing a match drops back to the roster with a ten
second continue countdown, so a loss does not end the run on its own, but
`match` reads 0 from there and the ladder starts again.

The bundle of that match is `_runs/2026-09-14T01-56-52Z-wwf`.

## Search wins where mashing loses

`tash.search` over 22 candidates -- nineteen motions and three positioning
steps (step in, step out, guard) -- scored
`(164 - cpu_health) - 1.2 * (164 - p1_health)`, wins the first fall 164 to 0
while losing one point of health, in 13 decisions. A decision costs 0.9 to
1.1 s of wall and the fall costs 15,842 emulator frames of trials for about
600 frames kept.

Run as a ladder loop, 400 decisions carried the run through four and a half
matches: 236,176 emulator frames tried, **6,810 frames actually played**, so
a match at this standard is about 1,500 played frames and a decision is
about 590 tried.

## The ladder

The Intercontinental Championship is seven matches, and the player won all
seven with Bret Hart at `VERY HARD`. The shape of each is read from the
wrestler records at the bell: the opponents a match engages are the records
that fall, and a match ends when every one of them is 0.

| match | opponent at the bell | shape | decisions, fall 1 and fall 2 |
|---|---|---|---|
| 1 | Razor Ramon (1) | one on one | 13, 10 |
| 2 | Undertaker (2) | one on one | 16, 18 |
| 3 | Bam Bam Bigelow (5) | one on one | 16, 16 |
| 4 | Lex Luger (8) | one on one | 15, 12 |
| 5 | Undertaker (2) and a partner | two on one | 26, 20 |
| 6 | id 6, not in the name table, and a partner | two on one | 18, 26 |
| 7 | Lex Luger (8) and two partners | three on one | 28, 37 |

Four singles, two two-on-one and one three-on-one: the shape the published
guides give, measured here rather than assumed. The opponent drawn for a
match varies with the frame the title is confirmed, so the names are this
run's, not the ladder's.

The run's last three lines, from
`_runs/2026-09-14T11-48-20Z-wwf-ladder` (the first is one line in the log,
wrapped here):

```
done: belt: 7 matches won, 14 falls, 271 decisions, last match 7;
landed 19/19 (unlanded: none); frame 670801, tried 627027,
idle 3084/24466 (12.6%), 644 s
ran    670801 frames (43406 kept) in 644.52 s, 1040.8 fps, 17.37x real time
bundle _runs/2026-09-14T11-48-20Z-wwf-ladder (667482 encoded, 3319 dropped)
```

Every one of the nineteen rows of the move table was played with a verdict
of its own, judged by the animation the player held, not by the damage it
did. Thirty-one verdicts, none failed: seven for the animation word found
again at each bell, the falls, and the belt. The 627,027 tried frames are
the search's; the 43,406 kept are the film.

The calibration probe needs the player standing at the instant it looks, and
in the three-on-one that is not given: an earlier run of the same player
failed to find the word at match 7's bell and judged the rest of that match
through match 6's address. The probe now retries up to six times, twenty
frames apart; match 7 answered on the second try, 36 frames after the
bell.

The tape the run wrote replays frame for frame:

```
tash tape replay --bundle _runs/2026-09-14T11-48-20Z-wwf-ladder \
    --record _runs --name wwf-ladder-film
replayed 43406 frames of the 43406 the tape keeps, hash 8dca80be3644b82b,
watches match
```

and the film is `_runs/2026-09-14T12-00-07Z-wwf-ladder-film` (43,406 frames
at 1280x896, 348 s).

### Standing still, before and after

An idle frame is a played frame where neither wrestler's health moves and
the player's animation is the standing one (`0x2aee`). The same player with
the previous per-hit score and the unfiltered candidate list spends
**905 idle frames of 4,051 played, 22.3%**, over three falls
(a copy of the player run made outside the tree, `2026-09-14T11-27-34Z-wwf-old-score`). The ladder above spends **3,084 of 24,466, 12.6%**, fall by fall:

| match | fall 1 | fall 2 |
|---|---|---|
| 1 | 10.3% | 7.1% |
| 2 | 9.2% | 10.5% |
| 3 | 10.5% | 10.6% |
| 4 | 11.3% | 11.3% |
| 5 | 12.3% | 13.1% |
| 6 | 12.8% | 12.5% |
| 7 | 12.4% | 12.6% |

Three changes account for it: the score is damage a frame of the decision
rather than damage a hit, so a slow move has to earn its length; `wait` and
`guard` leave the candidate list unless the last decision cost health, and
the dash replaces the walk when the gap is beyond reach; and a decision
stops once the player has stood still for 12 frames instead of running a
fixed 80-frame tail, which is where most of the old idle sat.

The v3 ladder, at `MEDIUM` and scored by damage a hit, won the same belt in
283 decisions and 42,085 kept frames. This one is 271 decisions and 43,406
kept at the top skill with every published row that registers exercised, so
the film is 3% longer against much harder opposition and holds a little
under half as much standing still.

## The third line: the combo, the counter and what they cost

`examples/wwf/comboline.py` is the same searching player with a per-frame
answer under it. `Player.advance` steps one frame at a time and `react`
answers that frame from `$64(a0)`:

| the state word reads | what the player does that frame |
|---|---|
| `0x11`, the opponent has him | F, F, SP -- the reversal at `$2add6` |
| `0x10` and the meter is full | F, F, P -- the combo at `$2ac06`, then the branch |
| `0x9`, on the mat | mash P, K, SP, SK; a full meter buys the rise at `$392d2` |

Above that, one decision is still a `tash.search` over the nineteen rows
and the seven movements, 22 candidates wide, scored on damage a frame with
the meter gained weighted in. Two lines were run from the same tape, and
they differ only in how badly the player wants the meter:

| | second line (12:10) | search-led | combo-led |
|---|---|---|---|
| meter weight below full | -- | 2.0 | 12.0 |
| candidates when the meter is full | -- | all 26 | the seven hold rows and the steps |
| decisions | 271 | **220** | 313 |
| kept frames | 43,406 | **39,014** | 47,657 |
| frames tried | 627,027 | 472,057 | 703,030 |
| idle share of the kept line | 12.6% | 13.6% | 12.8% |
| combos fired on the kept line | 0 (none exist for it) | 1 | **8** |
| reversals on the kept line | 0 | 14 | 17 |
| wall clock | 644 s | 515 s | 738 s |

`comboline.py` as committed is the combo-led one; the search-led control
is the same file with `CHARGE_WEIGHT` set to `METER_WEIGHT` and the pool
line in `candidates` left at `CANDIDATES`, which is how it was run. Both
win the belt in seven matches and fourteen falls, and neither loses a
fall. The bundles are `_runs/2026-09-14T15-05-01Z-wwf-searchline` (search-led)
and `_runs/2026-09-14T15-04-59Z-wwf-comboline` (combo-led); twelve verdicts
each, none failed.

Decisions a match, and the frames the tape keeps for it:

| match | search-led, fall 1 and 2 | kept | combo-led, fall 1 and 2 | kept |
|---|---|---|---|---|
| 1 | 8, 11 | 1,647 | 13, 8 | 1,878 |
| 2 | 9, 11 | 1,759 | 11, 6 | 1,622 |
| 3 | 12, 9 | 1,856 | 17, 13 | 2,556 |
| 4 | 14, 10 | 1,866 | 12, 12 | 2,089 |
| 5 | 20, 24 | 3,486 | 38, 23 | 4,813 |
| 6 | 20, 12 | 3,162 | 14, 30 | 7,835 |
| 7 | 30, 30 | 5,944 | 38, 38 | 7,566 |

Both tapes replay frame for frame:

```
tash tape replay --bundle _runs/2026-09-14T15-04-59Z-wwf-comboline \
    --record _runs --name wwf-comboline-film
replayed 47657 frames of the 47657 the tape keeps, hash 353d7c2c145433e5,
watches match

tash tape replay --bundle _runs/2026-09-14T15-05-01Z-wwf-searchline \
    --record _runs --name wwf-searchline-film
replayed 39014 frames of the 39014 the tape keeps, hash e6899e52bc3c9cd3,
watches match
```

The films are `_runs/2026-09-14T15-17-38Z-wwf-comboline-film` (47,657
frames, 1280x896, 313 s) and
`_runs/2026-09-14T15-17-39Z-wwf-searchline-film` (39,014 frames, 262 s),
0 dropped in each.

Both lines were run three times: on the harness at `00ce8d4`, on the one
the first rebase brought (v6 step 1, which adds `colours` and probes every
checkpoint rather than only the first), and on the one the last rebase
brought (the anchor rewrite). The kept line is the same to the frame every
time -- the same 220 and 313 decisions, the same 39,014 and 47,657 frames,
the same two replay hashes -- and only the frames tried move, from 419,257
to 472,057 and from 611,590 to 703,030, because every checkpoint is probed
now.

### Chasing the meter is slower, and the arithmetic says why

The combo is the most spectacular thing the player can do and it is not
the fastest. Measured from the hold, `F,F,P` with the published branch
played twice is 81 damage over 255 frames -- 0.32 damage a frame, and it
costs the sixteen moves that filled the meter. A decision of the searching
player at VERY HARD deals about 20 damage in about 45 frames -- 0.45 a
frame -- and gains a point of meter while it does. So the user's "fastest
way to a win" is true of a human holding a pad, where a combo is one input
and sixteen moves are sixteen chances to be hit, and false of a search that
never misses: the combo-led line spends 8,643 more kept frames and 93 more
decisions than the search-led one to fire seven more combos.

What the combo is worth is the 0 damage taken. Every one of the nine hits
lands with the opponent held, so the combo-led line is the one that wins
falls without losing health, and the reversal -- free, ungated, and worth
20 to 54 health a hold -- is worth taking on every hold either way. The
search-led line is the fastest line yet measured on this cartridge: 39,014
kept frames against the second line's 43,406, 10% fewer, with one combo and
fourteen reversals in it.

## Facts a plan rests on (checked 2026-09-14 on dev)

- Speed: the game runs at 1,250 to 1,650 fps headless (21x to 27x real
  time), and `checkpoint`/`restore` in memory are cheap enough that
  `tash.search` is a usable player -- 22 candidates cost about one second a
  decision.
- A move is read by its animation, not by its damage: the player's current
  animation is a word at `0x8548` and the opponent's at `0x87b8`, one object
  record on. The address is not fixed across matches -- the object slots are
  handed out in the order a match loads them -- so the player finds the word
  again at every bell by the two values it must take, standing (`0x2aee`)
  and punching (`0x3268`), and judges coverage through whatever address it
  finds. Before that calibration was added, every row read as unplayed from
  match 2 onwards.
- The ladder is seven matches -- four singles, two two-on-one, one
  three-on-one -- and the searching player wins all seven at `VERY HARD`
  in 271 decisions, 670,801 frames tried and 43,406 kept, with every one
  of the nineteen rows played and 12.6% of the kept line spent standing
  still (`_runs/2026-09-14T11-48-20Z-wwf-ladder`).
- Eleven of Bret Hart's published moves have an animation of their own on
  this build and eight do not; `combos.COMBOS` is the nineteen rows that
  play, and `README.md`'s "move by move" table is the proof of each with
  what was tried for the refusals.
- The third line wins the same ladder at VERY HARD in 220 decisions and
  39,014 kept frames with the per-frame reader under the search -- 10%
  fewer kept frames than the second line -- and the variant that chases
  the meter to fire eight combos needs 313 decisions and 47,657. The combo
  is 0.32 damage a frame and a searched decision is 0.45, so the combo is
  a way to take no damage, not the fastest way to a win.
- The combo meter is a gauge and the last line misread it: `0xb31c` is
  record 0's +10, it is cumulative, it survives falls and bells, and one
  ROM subroutine reads it -- `$3ef52`, which compares it with `#$10` and
  whose 33 callers are the eight characters' combo listeners and the
  opponent's brain. Sixteen is "100%": it is the value at which the bar is
  replaced by the flashing `COMBO!`. Each character has two initiators,
  forward, forward and an attack button from a hold; Bret Hart's are
  `F,F,P` and `F,F,SK`, and firing one empties the meter.
- OPTIONS offers five skills -- VERY EASY, EASY, MEDIUM, HARD, VERY HARD --
  as a word at `0xf4f0` reading 2 to 10 in twos, which survives to the bell.
  At VERY HARD standing still for 600 frames costs 143 of 164 health and a
  bare super punch or super kick from point blank is blocked for nothing.
- The watches are health, bar and combo meter for the player (`0xb312`,
  `0xb316`, `0xb31c`) and for the standing opponent (`0xb32e`, `0xb332`,
  `0xb338`), the clock's two digits (`0xaae6`, `0xaae8`), the match number
  (`0xab14`), the opponent's id (`0xb37a`) and the two x positions
  (`0xaabe`, `0xaade`). All are 16-bit little-endian words in byte-swapped
  work RAM. The wrestler records are a table of six at a stride of 14 from
  `0xb312`, and a handicap match uses records past the second.
- Health is 164 at the bell for both wrestlers and the bar is health times
  0.635. A fall is health reaching 0 and a pin; a match is best two of
  three; losing drops to the roster with a continue countdown and `match`
  reads 0.
- `tapes/title-to-first-bell.yaml` reaches the first bell in 1,781 frames
  and plays twice to a byte-identical frame. It picks Bret Hart and the
  Intercontinental title, and it draws Undertaker as match 1.
- The core is exactly reproducible from a checkpoint -- three identical
  runs, one hash -- but the opponent is live: starting the same inputs one
  frame later turns two landed attacks into 44 damage taken. A plan reads
  the state every decision and holds no schedule.
- Mashing loses match 1 at `MEDIUM` (5,128 frames, two falls conceded).
  Search over 22 candidates wins a fall 164 to 0 for one point of health in
  13 decisions. About 1,500 played frames a match, about 11,000 for the
  ladder if it is seven.
- A tap is two frames; one frame never registers. A special move needs its
  diagonal (`HCF + SP` is 32 damage, `D, D, F + SP` is a bare 8). `F` tapped
  then `F` held is a dash, and a dash attack is 46.
- **No combo registers, and the search that says so is finished** -- wrong
  twice over, and the third line says how: no *string* of bare buttons
  registers as one animation, but the gated super combos do, and the search
  never tried them because it never held a full meter in a hold.
  57,600
  trials over every string of two to four inputs from the six buttons and the
  four directions, at two spacings, tapped and with the first input held,
  from four checkpoints, produced no string whose damage beats the best single
  move and no string whose hits are closer than 13 frames apart; the published
  combos do not come out at any spacing from 2 to 12; no word in work ram
  counts hits; and no hit-count text appears anywhere on the screen. The
  character has eight moves *that a search finds*; reading the published
  list by animation adds eleven more, and `combos.py` now holds nineteen
  rows.
- Point blank is the band 70 to 90 in `cpu_x - p1_x`; inside 70 the wrestlers
  pass through each other and nothing lands. At `MEDIUM` the opponent blocks
  from the frame after a landed hit onwards, which is why every chained string
  reads as one hit and a whiff.
