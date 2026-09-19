# tash/python/

The scenario API. `tash run --profile <p> --scenario <file.py>` opens the
session, sets `tash.run` and runs the file; the same bindings build
`tash.cpython-*.so`, which an outside python can import (with no `run`).

One interpreter serves a whole run: the mcp `python` and `python_file` tools
evaluate in it turn after turn, so a name defined in one call is there in the
next and a module imported in one is cached in `sys.modules` -- a file edited
on disk between calls is the old one until `importlib.reload(<module>)`.

## `tash.run` — the run the cli opened

| call | what it does |
|---|---|
| `step(frames=1)` | runs that many frames |
| `run_until(predicate, timeout_frames)` | steps until `predicate()` is true, or until the anchor predicate string holds; returns the frames it took, raises on the timeout |
| `anchor(predicate)` | whether that anchor predicate holds on the latest frame, the bool the `anchor` mcp tool answers; nothing is recorded and nothing steps |
| `pace(rate)` | paces what follows at that multiple of real time; `0` runs as fast as the core does |
| `hold(port, buttons)` | holds those buttons on that port (1 or 2), on top of what is held |
| `release(port, buttons=[])` | releases those buttons, or all of them |
| `pad(port)` | the button names held on that port |
| `frames()` | frames produced so far |
| `line()` | the frames the tape's line holds, which is what a replay runs: a restore folds the trials away and `frames()` still counts them; `None` when the run lost track of its line |
| `harness_time()` | emulated seconds produced so far |
| `fps()` | the core's frame rate |
| `observe()` | `frame`, `time`, `width` and `height` of the latest frame, which is the box a `region` crop has to sit inside, `exact`, `difference`, `perceptual` as the sixteen-digit hex strings a predicate, a tape and a report spell them in, `change` since the last `observe()`, `pad`, `watches` |
| `look(path=None, region=None)` | writes a PNG, into the bundle's `shots/` when no path is given and into a scratch directory of the run's own when there is no bundle; `region` crops it, in the `x,y,width,height` the `look` tool takes; returns the path |
| `shot(region=None)` | writes a PNG where the mcp `shot` tool writes one -- the bundle's `shots/`, or a scratch directory of the run's own when there is no bundle -- and returns the path; `look(path=...)` is for a file of the caller's own choosing |
| `colours(region, rgb, within=0)` | how many pixels of that crop are within `within` of `rgb` per channel, an `(r, g, b)` of eight-bit numbers; the crop is spelled as `look` spells it |
| `bundle()` | the bundle's directory, or `None` |
| `report()` | writes the manifest, renders the bundle's `report.html` and returns its path |
| `tape()` | writes the bundle's `tape.yaml` as the line stands, folded to this frame the way a restore folds it; writes the manifest beside it, so `tash tape replay --bundle <the run's bundle>` films the line before the run closes; returns `file` and `frames` |
| `checkpoint(name, scratch=False)` | saves a state under that name, in memory and on disk beside the bundles, where the next run over the same root finds it; probes that restoring it is exact (below). `scratch=True` keeps it in memory alone, unprobed and uncached, the way `tash.search` takes its own: it restores and forgets by name like any other, and `run.yaml` counts it under `unprobed` |
| `restore(name)` | loads the state saved under that name, falling back to the checkpoints on disk when this run never took it; the copy this run took wins over a file another process has written since, and `forget(name)` is what asks for the file; the tape folds back to where the checkpoint was taken, so a `checkpoint` right after a `restore` needs no frame between them |
| `forget(name)` | drops the state saved under that name; a name that was never taken is not a refusal |
| `reset()` | powers the run on again, opening the core afresh and loading the content, then runs one frame so the next thing judged is the new machine's; the frame count goes on and counts that frame, the tape folds back to nothing and the checkpoints stand |
| `play(tape)` | plays a tape, reporting each segment as `--tape` does, its watch anchors reading the profile's watches; returns `segments`, `frames`, `waited`, `transitions`, `retries`. A tape plays from power on, so a run that has stepped since it was powered on is a refusal naming the frame it stands at: `reset()` gives it a machine a tape can play on, and `restore(name)` is how it goes back to a state it has already been in |
| `mark(name, group=None)` | a mark record in the trace; marks of one group collapse to a row per fifty on the report's time line |
| `clip(label, from_mark=None, from_frame=None)` | the window the report cuts a clip from, running to the current frame; one of the two starts it, and a mark this run never made is a refusal |
| `expect(name, condition, text="", raises=True)` | a verdict record, and an `AssertionError` when the condition is false; `condition` is a bool or an anchor predicate string, whose own wording becomes the verdict's text when `text` is empty |
| `judge(name, passed, text="")` | a verdict record, which never raises |
| `watch(name)` | that watch's current value, or `None` |
| `memory(region, address, count)` | that many `bytes` of that memory region from that offset; refuses a region this run has not got and a range past the region's end |
| `string(region, address, length)` | that many bytes of that region read in the byte order the core lays it out in, cut at the first null and decoded latin-1 |
| `word(region, address)` | the unsigned 16-bit number at that address as the guest reads it, in the region's own byte order |
| `long(region, address)` | the unsigned 32-bit number at that address as the guest reads it, in the region's own byte order |
| `memory_stable(region, address, count, frames, timeout_frames=600)` | steps a frame at a time until that block has held still for `frames` frames in a row; returns the frames it stepped, raises when it never settles |
| `hunt(region, width=2, endian="little", stride=0)` | a `Hunt` over that libretro memory region: the classic differential search, narrowed from the session, with the region's bytes never crossing into python; `endian` is `big`, `little` or `swapped` (little-endian 16-bit words in big-endian order, which is how a 68000 longword reads out of byte-swapped work ram) |

A button is `start` or `p1.start`; a port is `1` or `2`. Every reader that
names a region names it first, then the address, then the count.

`string`, `word` and `long` carry the region's byte order; `memory`,
`memory_stable` and `hunt` hand over the bytes as they lie and carry no swap.
Genesis Plus GX's `system` is byte-swapped work ram -- the byte at address R
sits at offset R xor 1 -- so `run.word("system", at)` is the 68000's own word
where `run.memory("system", at, 2)` is the two bytes in storage order. Every
other region, `cartridge` included, reads as it lies.

The regions are the ones the core hands over -- `system`, `save`, `video`,
`rtc`, whichever of them it has -- and one it does not: `cartridge` is the ROM
file the profile names, mapped read-only at launch and never changing, in the
file's own byte order. A libretro core exposes no cartridge, so the tables a
game keeps in its rom are otherwise measured on the emulator a room at a time:
`examples/scooby/world.py` has 178 object names and 21 room names read out of
three tables in one pass. Every reader takes it -- `memory`, `string`,
`memory_stable`, `hunt`, a profile's watches, `peek --region cartridge`, `tash
mem` -- because it is a region of the same map. A zipped ROM is the member the
session extracted, which is the cartridge the core was handed.

| raised | when |
|---|---|
| `tash.BudgetExceeded` | a `step`, `run_until`, `memory_stable` or `play` in an mcp `python` job whose frames would pass the job's `frame_budget`, or which the `python_cancel` tool stopped; a scenario run by `tash run --scenario` has no budget and never sees it |

Catching it is how a player ends its own loop on its own terms; the frames
the job ran are in the refusal the tool answers with either way.

## The restore probe, at every named checkpoint

Every named `checkpoint` is probed: the harness plays `PROBE_FRAMES` frames
from it, restores it, plays the same frames again and compares the frame
hashes, then restores so the run stands where the checkpoint was taken. A
run that never parts carries one passing verdict, "a restored state
continues as the straight line", from its first probe; a probe that parts
writes a failing verdict of that name for every checkpoint it happens at,
naming the checkpoint and the frame the two lines part at. `run.yaml` counts
them as `probed` and `parted`, the exit line and the report's frames row say
them beside `kept`, and a run with `parted` above zero kept a line no tape
can replay -- v4 won the WWF belt three times on a core whose save state
lost the YM2612's busy cycle, and only the first checkpoint was ever read.

A checkpoint `tash.search` takes is not probed: the search takes it,
restores it a candidate and drops it inside the one call, so a probe could
only prove a restore the call is about to make anyway -- and it would cost
`2 * PROBE_FRAMES` frames a level a call, on every decision a player
searches. `checkpoint(name, scratch=True)` is the same bargain for a
scenario that scouts with checkpoints of its own. `run.yaml` counts both as
`unprobed`, beside `probed` and `parted`. Either kind records where it sits
on the line, so a restore folds the tape back to it.

The probe costs `2 * PROBE_FRAMES` frames a checkpoint, which is what a
player pays for the check. Measured on this build (`PROBE_FRAMES` 120, so
240 frames a checkpoint):

| player | probed | unprobed | probe frames | run frames | share |
|---|---|---|---|---|---|
| `examples/wwf/player.py` | 279 | 337 | 66,960 | 737,521 | 9.1% |
| `examples/caesars/player.py` | 565 | 0 | 135,600 | 250,892 | 54.0% |

Bundles `_runs/2026-09-14T15-50-45Z-wwf` and
`_runs/2026-09-14T16-03-43Z-caesars`, both `parted: 0`, each the
playthrough its own README documents -- the WWF belt in 271 decisions and
43,406 kept frames, the Caesars wallet at $9,999,999 in 85,584. The WWF
player searches every decision, so most of its checkpoints are the search's
and it pays for 279 of 616; probing the other 337 as well would have cost
it 80,880 frames more. The Caesars player never calls `tash.search`: it
scouts with `checkpoint`, `restore` and `forget` of its own, every one of
them named, so it pays the probe on all 565 and better than half its run
is the check.

## Anchor predicates

`run_until`, `anchor` and `expect` take the same predicate string the
`run_until`, `anchor` and `expect` mcp tools take, read by
`tape::AnchorFrom()` and judged by the same `AnchorCheck` a tape's segments
wait on -- `sources/tash/mcp/README.md` has the grammar, `or` and `not`
included, and `sources/tash/tape/README.md` has every kind. A session line
transcribes into a scenario line unchanged:

    run.run_until("watch money greater 2000", 600)
    run.expect("rich", "watch money greater_or_equal 999999999")
    run.run_until("colour 0,248,0 within 24 over 96,64,64,64 at least 300",
                  120)
    run.run_until("colour 248,0,0 within 24 over 96,64,64,64 at least 300"
                  " or colour 0,248,0 within 24 over 96,64,64,64 at least"
                  " 300", 900)

A watch predicate reads the profile's watches, so a run without them
refuses rather than answering. A colour predicate is `colours()` as
something to wait on: the same count, over the same crop, held against the
number after `at least`.

## `tash` — the module, over the latest frame

| call | what it does |
|---|---|
| `exact_hash()` | the frame's xxh3 digest |
| `difference_hash()` | its dHash |
| `perceptual_hash()` | its pHash |
| `change_from(previous)` | `changed` and `mean`, against a PNG an earlier `look()` wrote |
| `find(pattern, region=None)` | `score`, `x`, `y` of the best match of that PNG |
| `watch(name)` | that watch's current value, or `None` |
| `watches()` | every watch the profile names, by name |
| `search(candidates, apply, score, depth=1, restored=None)` | the best candidate, judged in the emulator |

## `tash.search` — the emulator as the model

`search(candidates, apply, score, depth=1, restored=None)` checkpoints the
run, and for each candidate restores that checkpoint, calls
`apply(run, candidate)` and then `score(run)`. At `depth` above one it
searches the same candidates again from the applied state and the candidate
is worth the best of its children, so a lookahead of one is `depth=2`.
`candidates` may be a callable, in which case `candidates(run)` is asked at
every state rather than once.

`restored(run)` is called after every restore to a level's seed, the last one
included, which is the only moment a scenario can drop what it read from the
game before the trial that moved it: a rewind takes the emulator back and
leaves python's own caches as the trial left them.

The run is left where it started, whether the search finished or a callback
raised, and it keeps the line it stood on, so a `tape()` after a search
writes it and a `checkpoint` after one carries it. The checkpoints the search
takes are its own, are neither probed nor cached, are named apart from any
other search's so one may run inside another's `score`, and are dropped
afterwards. The answer is a `SearchResult`:

| field | what it is |
|---|---|
| `best` | the first candidate with the highest score, `None` when there were none |
| `score` | what `score` answered for it |
| `trials` | how many candidates were applied, every level counted |
| `frames` | the frames the search ran |
| `seconds` | the wall seconds it took |

`examples/homebrew/search.py` is a whole one.

## `tash.plan` — the searches, over no frame at all

A cost grid is `bytes`, one byte a cell, row by row: the byte is what
entering that cell costs and `tash.plan.IMPASSABLE` (255) is a wall. Neither
call touches the run, so a scenario builds the grid out of whatever it reads
— a map array in work ram, a board on screen — and plans over it as often as
it likes.

| call | what it does |
|---|---|
| `plan.route(costs, width, height, start, goal, diagonal=False)` | A* from `start` to `goal`, both `(x, y)`; answers `cells`, the tiles walked from start to goal inclusive, and `cost`, the entry cost of every cell after the start |
| `plan.distances(costs, width, height, sources, diagonal=False)` | multi-source Dijkstra; answers one cost a cell, row by row, from the nearest of `sources` |

`diagonal` is the neighbourhood: four sides when it is false, eight with the
corners when it is true, and the A* heuristic is the step count for that
neighbourhood times the grid's cheapest passable cell, so the route is the
cheapest either way.

`tash.plan.UNREACHED` is what a cell no passable source reaches answers, and
what `route` answers as its `cost` — with no cells — when there is no route,
when the goal is a wall, or when the start is. A grid whose byte count is not
`width * height`, and a cell outside it, are refusals. An impassable source
is no source.

    costs = bytes(...)                      # 255 for rock, 1 for sand
    reach = tash.plan.distances(costs, 32, 32, spice_tiles)
    pad = min(pads, key=lambda seat: reach[seat[1] * 32 + seat[0]])
    walked = tash.plan.route(costs, 32, 32, harvester, nearest_spice)
    order(walked["cells"][-1])

`examples/dune/player.py` builds its grid from the game's own map array and
ranks every candidate refinery pad by how far a harvester would travel.

A world whose moves are not a grid is a graph instead: `edges` is a list of
`(from, to, cost)` with node ids a caller chooses -- the game's own room word,
an action's index -- and both calls take the edges themselves, so a scenario
builds the list out of what it reads and plans over it as often as it likes.

| call | what it does |
|---|---|
| `plan.graph(edges, start, goal)` | Dijkstra from `start` to `goal`; answers `nodes`, the ids walked from start to goal inclusive, and `cost`, every edge the walk takes |
| `plan.graph_distances(edges, sources)` | Dijkstra from each source in turn; answers `{source: {node: cost}}`, every node of the graph in each row |

An edge leads one way, so a room a door lets out of is two edges. The nodes
are both ends of every edge, so a node no edge touches is not in the graph:
`start`, `goal` and a source must each be one that is, and each refuses by
name when it is not. A goal nothing leads to refuses as well -- an order a
tour cannot take is not an order that costs a lot -- while an unreachable node
in a distances row is `tash.plan.UNREACHED`, because a row is a table and a
table has an entry for every node. A negative cost is a refusal.

    rooms = [(16, 9, 64), (9, 16, 64), (16, 13, 176)]
    between = tash.plan.graph_distances(rooms, list(world.ROOMS))
    order = min(tours, key=lambda tour: sum(between[a][b] for a, b in pairs))

`examples/scooby/world.py` is 21 rooms and about 75 actions, which is what
the room word and the guide's ordered list come to; the tour search costs an
order out of one `graph_distances` table and walks it with `graph`.

## `tash.Hunt` — the memory hunt, from the session

`run.hunt(region)` answers one. Every address in the region that a number of
that width fits at is a candidate; `stride` is how far apart those addresses
sit, and `0` steps by the width.

| call | what it does |
|---|---|
| `step(how, value=None)` | the first call seeds the region; every call after it drops the candidates that did not move `how` since the call before, and returns how many are left |
| `candidates(limit=20)` | up to that many survivors, as `(address, value)` in address order |
| `len(hunt)` | how many survived |
| `reset()` | forgets the candidates and seeds again on the next `step` |

`how` is `equal`, `changed`, `increased`, `decreased`, or `value` with the
number the screen shows; a `value` step needs that number and the others
refuse one. A value in `candidates()` is what the step that kept it read, so
frames run since then are not in it.

    hunt = run.hunt("system", width=2, endian="little")
    hunt.step("changed")            # 32768 candidates
    run.step(1)
    hunt.step("increased")          # 50
    run.step(1)
    hunt.step("increased")          # 41

The engine is the one `tash mem search` drives from the command line, so a
hunt written either way narrows the same.

A refusal is a `RuntimeError`; the cli answers a raised exception with the
traceback. `examples/homebrew/scenario.py` is a whole one.
