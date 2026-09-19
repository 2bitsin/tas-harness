# tash/tash/

The cli: one binary, one command per verb, and the run every verb is built
on. `main.cpp` declares the commands; each is a struct whose members are its
options, and the comment beside a member is what `--help` prints.

| command | what it does |
|---|---|
| `run` | opens a profile, plays a tape or a scenario or counts frames, and closes the bundle |
| `serve` | the same run behind the MCP endpoint (`sources/tash/mcp/`) |
| `session` | one tool call against a running `serve`, from a shell |
| `replay` | plays a bundle's tape back and compares hashes and watches; `--record <dir>` leaves the clean playthrough as a bundle of its own |
| `tape` | reads, checks and cuts tapes |
| `report` | renders `report.html` for a bundle that already exists |
| `mem` | the memory search, scripted |

A recorded replay has no live producer to stall, so its encoder queue blocks
the core instead of dropping the newest picture: the film holds every frame
the tape played, and the summary's `dropped` count reads 0. It is a film
rather than a search record, so it is also upscaled by whole pixels to at
least 1280 wide and encoded at x264's `slow` preset and crf 16, and the
summary states the size it came out at. A run's video is every frame a
keyframe so the report can cut a clip out of the middle of it; a film pays
a keyframe every two seconds instead, and `clip` is refused on a run that
records one.

## The scanlines a film gets

The upscale gives every source row as many output rows as it scales by, and
the last of them carries a scanline. The user asked for it after watching
the upscaled Columns film -- "oh the columns video now looks fabulous
quality wise", and "well if you go over resolution, you can apply some
scanlines filter I guess" -- so the pass exists to spend the rows the
upscale invented on the look of a CRT rather than on copies of the same
line.

`FILM_SCANLINE_DARKENING` is how far towards black that row is taken, on the
luma plane alone: the chroma planes are untouched, so the line darkens
without draining the colour. It is 0.35 because that reads as a line at the
four-times upscale a 320-wide core gets without dimming the picture the user
just called fabulous -- a mid-grey row comes back at 83 where its untouched
neighbours are 129. Only `Quality::FILM` asks for the pass: a search record
is never darkened, and neither is an FFV1 recording, which carries packed
RGB and has no luma plane to darken.

`RunHost` is the seam the MCP module is given: it owns the open run, the one
embedded interpreter, and the detached python job. `OpenedRun` assembles a
run -- session, journal, bundle, recording, verdicts, watches -- for `run`
and `serve` alike, so the two never drift.

## The frames a run keeps

A run that restores folds its line: the frames after the checkpoint are
gone from the tape, though the trace keeps all of them and the frame
counter never goes back. The two numbers are printed together -- `ran N
frames (M kept, probed P checkpoints, Q parted, U unprobed)` on exit, `N
recorded, M kept, probed P checkpoints, Q parted, U unprobed` on the report
-- and `run.yaml` carries the second as `kept`, which is the sum of
`tape.yaml`'s segments and what `tash replay` plays, and the last three as
`probed`, `parted` and `unprobed`. Those three are the restore probe
(`sources/tash/python/README.md`): a named checkpoint is probed, one a
search takes and consumes is counted as `unprobed`, and a run with `parted`
above zero kept a line no tape can replay.

## The time-lapse video

`--video-stride N`, on `run` and on `serve`, encodes one frame in N at the
profile's own fps: a stride of 60 turns an hour of play into a minute of
film. The default is 1, and above 1 the audio is dropped rather than
stuttered. Everything else is untouched -- the trace keeps every frame, the
tape replays frame for frame, shots stay full-rate stills -- and `run.yaml`
records the stride so the report can say it.

## The detached python job

`python`/`python_file` with `detach` run the source on a worker thread that
owns the run, while the endpoint's thread keeps answering `python_status`,
`python_output` and `report`; everything else refuses until the job is over.
`sources/tash/mcp/README.md` has the rules. From a shell:

    tash session python_file --port 7784 -- --path player.py --detach true
    tash session python_status --port 7784
