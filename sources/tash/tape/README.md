# tash/tape/

A tape is a run's inputs written down: a header and a list of segments,
each one waiting for something on screen and then holding the pads frame
by frame. `tash tape check <file>` reads one and says what it holds,
`tash run --tape <file>` plays one into a fresh session, `run.play(tape)`
and the `restore_or_play` mcp tool play one into a live one, and `tash
tape replay --bundle <dir>` plays the tape a recorded run left behind and
checks it comes out where that run did.

The file is yaml a human reads and edits. It carries no rom and no core
to run: the profile does that, and the tape only names the profile it was
cut against. `examples/homebrew/tapes/demo.yaml` is a small whole one and
`examples/dune/tapes/mission-1-to-mission-2.yaml` a long one.

    header:
      name: demo
      profile: examples/homebrew/profile.yaml
      core: genesis_plus_gx
      time_base: frame
      timeout_frames: 600
      on_timeout: fail
    segments:
      - name: first-frame
        anchor:
          kind: exact_hash
          hash: d90a8b4b09bd710f
        frames: 30
        transitions: |
          0  down p1.start
          2  up   p1.start

## `header`

| field | default | what it is |
|---|---|---|
| `name` | -- | the tape's own name, which `tape check` prints and a refusal names |
| `profile` | -- | the run profile the tape was cut against; written by a recording and printed, never resolved -- what a replay runs is the profile its bundle's `run.yaml` names |
| `core` | -- | the core the profile asked for, carried the same way |
| `time_base` | `frame` | `frame` or `wall`; only `frame` plays in v1 and a `wall` tape is refused on read |
| `timeout_frames` | `600` | how long a segment waits for its anchor when it does not say; ten seconds at sixty frames |
| `on_timeout` | `fail` | `fail` or `retry`, the default for every segment |
| `retries` | `1` | how many times a `retry` segment goes back to where it started and waits again, so such a segment gets two tries in all |

## A segment

| field | default | what it is |
|---|---|---|
| `name` | -- | required, and unique within the tape |
| `anchor` | `none` | what the segment waits for before it plays a frame; `none` plays at once, which is how a tape starts from power on |
| `timeout_frames` | the header's | frames this segment waits for its anchor |
| `on_timeout` | the header's | `fail` refuses the moment the timeout runs out; `retry` goes back to the state the segment started from and waits again |
| `frames` | -- | how long the segment runs once its anchor held |
| `transitions` | -- | the button changes, one per line |
| `pointer` | -- | where a pointer went, one line per frame it moved |

A segment runs for the greater of its `frames` and one frame past its
last transition, so `frames` may be left out when the transitions
themselves say how long it is, and a segment that only waits for
something carries no transitions at all. A recorded segment carries both.
The frames a segment waits and the frames it plays both count into the
session's frame number; a transition's own frame is counted from the
segment's start, after the anchor held.

### `transitions`

A block of `<frame> <down|up> <channel>` lines. Blank lines and lines
starting with `#` are ignored; anything else is three words or the line
is refused by number. Frames must not go backwards -- the player walks
the list once -- though two transitions may share a frame.

A channel is `<device>.<button>`: a pad is `p1` or `p2` and a pointer is
`m1` or `m2`. A pad's button is one of the sixteen libretro joypad names,
in the order of the bits they hold.

    b  y  select  start  up  down  left  right  a  x  l  r  l2  r2  l3  r3

A pointer's is one of five, in the same way:

    left  right  middle  wheel_up  wheel_down

A wheel notch is a button held for the frame it turned in. `down` holds
the button from that frame and `up` lets it go. Only the bits a
transition names move, so a tape played into a live session starts from
whatever that session already holds, and leaves held whatever its last
transition left down.

### `pointer`

A block of `<frame> <device> <x>,<y>` lines, read the way `transitions`
is read: blank lines and `#` lines ignored, three words or the line is
refused by number, frames never going backwards. One line per frame the
pointer moved, in the segment's own frames; a position holds until the
next line moves it.

    pointer: |
      0      m1   128,96
      1      m1   130,97
      2      m1   133,99

A position is not a transition -- it moves on most frames of a drag --
which is why it is not in `transitions`, where those lines would bury the
handful of pad changes a human reads (grilling 17). A tape of a target
with no pointer carries no such block.

Nothing in v1 plays a pointer: a libretro session is held by pads, so the
player refuses a segment that moves one, by name. What writes such a tape
is libtash's file mode (`sources/tash/linked/README.md`) and what replays
it is the target that wrote it.

## Anchors

An anchor is one predicate over the frame the session last produced, or
over a watch the profile names. The same predicate has two spellings: the
fields in the file, and the one line `tape::AnchorFrom()` reads -- which
is what `expect`, `run_until` and `anchor` take, in python and over mcp,
and what a timeout writes back into its refusal.

| kind | the line | the fields |
|---|---|---|
| `none` | `none` | -- |
| `exact_hash` | `exact_hash <hash>` | `hash` |
| `difference_hash` | `difference_hash <hash> within <n>` | `hash`, `max_distance` |
| `perceptual_hash` | `perceptual_hash <hash> within <n>` | `hash`, `max_distance` |
| `template_image` | `template_image <path> at <score>` | `image`, `minimum_score` |
| `watch` | `watch <name> <comparison> <value>` | `watch`, `comparison`, `value` |
| `colour` | `colour <r,g,b> within <n> over <x,y,w,h> at least <count>` | `colour`, `max_difference`, `region`, `count` |
| `any_of` | `<predicate> or <predicate>` | `any_of` |

| kind | what holds |
|---|---|
| `none` | everything; the only anchor a segment does not step for |
| `exact_hash` | the frame's xxh3 digest is that number |
| `difference_hash` | its dHash is within `max_distance` bits, hamming |
| `perceptual_hash` | its pHash is within `max_distance` bits, hamming |
| `template_image` | the best normalised cross-correlation of that PNG over the frame is at least `minimum_score` |
| `watch` | that watch's value compares that way to `value` |
| `colour` | at least `count` pixels of the crop are within `max_difference` of that colour, per channel |
| `any_of` | any one of its alternatives does, all of them asked of the one frame |

`hash` is up to sixteen hex digits, `0x` allowed and leading zeros not
needed; it is the spelling `observe()`, a trace dump and the report all
print a hash in, so an anchor is a line somebody copied. A comparison is
`equal`, `not_equal`, `less`, `less_or_equal`, `greater` or
`greater_or_equal`. A `template_image` path is resolved against the
tape's own directory unless it is absolute; the convention the examples
and `WriteAnchorImage()` keep is `<tape stem>.anchors/<segment>.png`.

Defaults: `max_distance` 4, `minimum_score` 0.9, `comparison` `equal`,
`value` 0, `max_difference` 0, `count` 1.

`region` is `x`, `y`, `width`, `height`, and every kind that reads the
frame takes one; without it the whole frame is read. Only `colour`
spells its crop on the line, so a hash or a template anchor cropped to
part of the screen is written in the file. `colour` in the file is a map
of `red`, `green` and `blue`, eight bits a channel, the `<r,g,b>` of the
line written out:

    anchor:
      kind: colour
      colour: { red: 248, green: 0, blue: 0 }
      max_difference: 24
      region: { x: 96, y: 64, width: 64, height: 64 }
      count: 300

    anchor:
      kind: perceptual_hash
      hash: 30b14b4e6e34b8cb
      max_distance: 6
      region: { x: 96, y: 64, width: 64, height: 64 }

A watch anchor asks whoever is sampling the profile's watches, and
refuses -- it does not fail -- when nobody is or when the name is not one
of theirs.

### `not` and `or`

`negated: true` in the file turns a term's answer over, and `not` does it
on the line. Terms join with `or`, which is what an `any_of` is: two or
more alternatives, each one asked of the frame.

    not watch lives equal 0
    template_image menu.png at 0.95 or template_image pause.png at 0.95
    colour 248,0,0 within 24 over 96,64,64,64 at least 300
      or colour 0,248,0 within 24 over 96,64,64,64 at least 300

`or` binds loosest and `not` tightest, there are no parentheses, and two
`not`s cancel. An `any_of` written on a line is never typed by name: a
single term stays a single term and two or more become one `any_of`, and
the spelling `any_of` in a line is refused with that advice. In the file
an `any_of` holds its alternatives under `any_of:` and needs at least
two, since one says nothing a single term does not.

In the file an `any_of` may carry `negated: true`, which is "none of
these"; the line has no way to say it, since `not` binds to a term. Such
an anchor words itself as `not <first> or <rest>`, which reads back as a
first term negated -- the one predicate the two spellings do not agree
on.

## `tash tape check <file>`

Reads the file, refuses it or prints it. What it judges is what any
reader judges, `tape::Checked()`:

- the time base is `frame`;
- there is at least one segment;
- every segment has a name and no name is used twice;
- every anchor answers the kind it claims -- a hash that parses, an
  image named, a watch named, a colour with a count and a crop and every
  channel within 0 to 255, an `any_of` with two alternatives or more;
- every transitions block parses, every line of it, and every pointer
  block with it.

The first thing wrong is the refusal, named by segment and, inside a
transitions block, by line number. What `check` does not do is open a
`template_image`: a missing or unreadable PNG prints as a bare
`template_image` here, and is refused when a player is built for the
tape.

    tape    demo (2 segments)
    core    genesis_plus_gx
    profile examples/homebrew/profile.yaml
      first-frame      exact_hash d90a8b4b09bd710f              timeout 600    2 transitions
      logo             template_image demo.anchors/logo.png at 0.99 timeout 600    2 transitions

A segment that moves a pointer says how many moves it holds after its
transitions.

## Playing one

`tash run --tape <file>` plays the tape from the session's start and then
runs on for `--frames`. Each segment reports as it ends, and the run
prints one line for the whole tape:

    core   Genesis Plus GX v1.7.4 c2838c7
    rom    examples/homebrew/zsenilia.bin
    first-frame     anchor after 1      2 transitions over 30 frames
    logo            anchor after 42     2 transitions over 60 frames
    tape   examples/homebrew/tapes/demo.yaml (2 segments, 133 frames, 4 transitions, 0 retries)

`anchor after` is the frames that segment spent looking for its anchor,
and the tape's `frames` is every frame it ran, the waiting included.
`run.play(tape)` answers the same five numbers as `segments`, `frames`,
`waited`, `transitions` and `retries`, and the `restore_or_play` mcp tool
plays one the same way when the checkpoint it wants is not there yet.

A segment that never sees its anchor -- after its retries, if it has any
-- refuses with its own name, the predicate written back out and the
timeout it used, which is the sentence to read before touching anything
else.

## `tash tape replay --bundle <dir>`

A recorded run leaves its own tape in its bundle, cut into a segment per
mark, each waiting on the exact hash of the frame before its mark; a
restore or a reset folds the line back -- the frame a reset runs to draw
the new machine's picture is the folded line's frame 0 -- and a run that
cannot be written as one line refuses to write a tape at all. A run that made no marks
leaves one segment named `start` waiting for nothing.

`tape replay` opens the profile that bundle's `run.yaml` names, plays its
`tape.yaml` into a fresh session, and checks the replay against what
`trace.bin` says the run came out as: the last frame's exact hash is the
hash the run ended on, and every watch the profile names reads what the
run recorded. The first of those that parts is the refusal, and it names
both sides -- the hash and the frame count the run recorded, and the hash
and frame count the replay produced. Each segment is also marked on the
replay's own time line, and a mark the run refuses is reported in its
place.

    core   Genesis Plus GX v1.7.4 c2838c7
    rom    examples/homebrew/zsenilia.bin
    ran    133 frames (133 kept, probed 0 checkpoints, 0 parted) in 0.08 s, 1743.1 fps, 29.09x real time (59.9227 fps emulated, D2)
    replayed 133 frames of the 133 the tape keeps, hash 4e0233e99c9ba964, watches match

`--record <root>` records the replay as a bundle of its own -- video,
trace, verdicts and `report.html` -- whose manifest carries `replay_of`,
the bundle it came from, and an `outcome` of `completed` or the refusal.
`--name` names that bundle and `--video-stride <n>` encodes one frame in
n, audio dropped, so a long replay's video is a time-lapse. Recorded or
not, the check is the same and the exit code is the answer.

## The tape in a bundle

| file | what it is |
|---|---|
| `run.yaml` | the manifest: the core, the rom, the profile, the watches, how it ended, and `tape` when one was written |
| `tape.yaml` | the inputs, this file's shape |
| `trace.bin` | every frame's hashes, the watches, the marks and the verdicts |
| `video.mkv`, `shots/`, `clips/` | what it looked like |
| `verdicts.jsonl`, `report.html` | what it decided, and the page |

`tape.yaml` and `trace.bin` are the two halves a replay needs: the tape
says what to do and the trace says what should come of it. The rest is
for a reader.
