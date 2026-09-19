# tash/mcp/

The harness as an MCP server. `tash serve --profile <p> --port <n>` opens one
endpoint, `http://127.0.0.1:<port>/mcp`, and every tool on it drives the same
`python::ScenarioRun` the scenario API drives: the tools are the scenario
verbs, so a verb is implemented once and called from two places.

## The spec this follows

Model Context Protocol, revision **2025-06-18**
(<https://modelcontextprotocol.io/specification/2025-06-18>), which is the
revision after the `2025-03-26` Streamable HTTP transport landed. Sections
cited below are that revision's.

| what | section |
|---|---|
| one endpoint path, POST and GET | Transports 2.1, 2.2 |
| a POST body is one request, notification or response | Transports 2.2 |
| a notification or response answers `202 Accepted` with no body | Transports 2.2 |
| a request answers `application/json` or `text/event-stream` | Transports 2.2 |
| GET opens a stream the server may speak on, or `405` | Transports 2.3 |
| `Mcp-Session-Id` assigned on initialize, required afterwards | Transports 2.5 |
| `400` without it, `404` for one that ended, `DELETE` to end it | Transports 2.5 |
| `MCP-Protocol-Version` on every later request | Transports 2.7 |
| bind the loopback, do not bind every interface | Transports, security warning |
| `initialize`, then `notifications/initialized` | Lifecycle 2.1, 2.2 |
| `tools` capability, `listChanged` | Lifecycle 2.1.2, Tools 2.2 |
| `tools/list`, `tools/call`, `inputSchema` | Tools 2.3, 2.4 |
| text and image content blocks | Tools 2.5 |
| an execution failure is `isError`, not a protocol error | Tools 2.6 |
| `ping` | Utilities / Ping |
| envelope, ids and the `-32xxx` codes | JSON-RPC 2.0 4, 5.1 |

## The tools

`tools/list` generates each schema from the very struct the tool's body reads
its arguments out of (`tool-arguments.hpp`), so the schema and the decode
cannot drift apart. A member that may be absent is optional; everything else
is `required`.

| tool | what it does |
|---|---|
| `launch` | opens a profile: core, ROM, bundle, journal, as `tash run --bundle` does; answers the open run when it is already that one |
| `shutdown` | closes the run and finishes what it was recording; the server exits once the last session has gone, so nothing has to free the port by pid |
| `step` | runs frames, then the observation |
| `run_until` | steps until a predicate holds, or the timeout, then the observation |
| `pace` | paces what follows at a multiple of real time |
| `observe` | the dict `observe()` gives, as text; the frame line ends with `size <width>x<height>`, the box a `region` crop has to sit inside; answers the latest completed frame while a python job runs |
| `peek` | a block of the guest's memory as a hex dump, sixteen bytes a line with the address in front; the address is written as a profile writes one (`0xc800` or decimal) and the region defaults to the system ram, as a watch's does; `--region cartridge` reads the ROM file itself |
| `look` | the latest frame as a PNG image block, optionally cropped; answers while a python job runs |
| `act` | hold, release or tap buttons by name, then run frames, then the observation |
| `anchor` | evaluates a predicate against the latest frame |
| `reset` | powers the run on again, opening the core afresh and loading the content, then runs one frame so the observation it answers with is the new machine's: the frame count goes on and counts that frame, the trace records the reset at the frame before it, the tape being recorded folds back to nothing and the reset's frame is the new line's frame 0, and the run's checkpoints stand; the server stays up |
| `checkpoint` / `restore` | a named save state, in memory and under `_checkpoints/`, with the frame it was taken at; `restore` serves the copy this session holds and names the file on disk when another process has written it since, so a stale answer is never a silent one; `restore` then the observation |
| `restore_or_play` | restores the checkpoint if it is cached, else plays the tape and checkpoints, then the observation; a tape plays from power on, so a run that has stepped since then refuses rather than playing into the middle of a game |
| `expect` | evaluates a predicate and records the verdict |
| `judge` | records the caller's own answer |
| `mark` | a mark record in the trace, optionally in a `group` the report's time line collapses |
| `shot` | writes a PNG into the bundle's `shots/`, or into a scratch directory of the run's own when the server opened no bundle |
| `clip` | records a window from a mark or a frame to now |
| `python` / `python_file` | evaluates source with `tash.run` set, state persisting, then the observation; `python` takes `source`, `python_file` takes `path`; both take `frame_budget` and `detach`; waited for, the budget is 360,000 frames, detached it is none, and every answer ends with the frames the job ran against the budget it began under |
| `python_cancel` | stops the running job at its next `step`, `run_until` or `play`, whatever budget it has left; nothing running is a refusal |
| `python_status` | whether the job runs, the frames it has made against its budget, whether `python_cancel` stops it, and the tail of both its streams |
| `python_output` | everything it wrote to stdout and to stderr, and once it is over what it answered or how it failed |
| `report` | writes the manifest, renders `report.html` and answers its path |
| `tape` | writes the bundle's `tape.yaml` as the line stands, folded to this frame the way a restore folds it, and the manifest a replay reads the profile from, and answers its path and the frames it holds; `tash tape replay --bundle <the run's bundle> --record <dir>` then films the line so far, and the run goes on. A running python job is a refusal: the tape is read off the trace this flushes |

Every path a tool answers is absolute: the client has its own working
directory and nothing in an answer is relative to the server's. A refusal
answers in the tool's own name, whichever module the verb it forwards lives
in.

### The observation, without asking for it

Every tool that moves the run -- `step`, `run_until`, `act`, `reset`,
`restore`, `restore_or_play`, `python`, `python_file` -- answers its own
sentence, a newline, and then exactly what `observe` would answer. A run with
no frame yet answers the sentence alone rather than a refusal.

A core renders nothing until it runs, so a restore whose state came without
its picture answers the frame from before it; the next `step` draws the new
one. A reset runs one frame of its own for exactly that reason, so what it
answers -- and what `run_until`, `anchor` and `look` judge after it -- is a
frame the new machine drew.

`observe` reports the change since the observation before it, so the
baseline is now the last moving answer rather than the last explicit
`observe`: `change` is what the last thing the caller did moved.

### Launching

Every `launch` argument is optional. What the caller leaves out comes from
the run `tash serve` was started with, so an agent driving a pre-launched
server can `shutdown` and `launch` again with no arguments and get the same
profile, bundle root, name and pace. `launch` refuses only when neither the
call nor the server names a profile.

A server started with `--profile` has the run open before the first call, so
a bare `launch`, or one naming the profile already open, answers that run's
description and changes nothing: an agent can always open with `launch`. A
`launch` naming another profile refuses until the open one is shut down.

### The detached python call

A run of hours does not fit in a tool call: the client's HTTP deadline
passes while the server is still working, and the answer it finally writes
has nowhere to go. `python` and `python_file` therefore take `detach`
(false by default). Detached, the tool answers at once with the job's name
-- `python-1`, `python-2`, ... -- and the source keeps running.

    python_file {"path": "player.py", "detach": true}   python-1 is running
    python_status                                       progress, then
    python_output                                       the whole of it

There is one job at a time. While it runs, every tool that would touch the
run -- `act`, `step`, `peek`, `memory`, `checkpoint`, a second `python`,
detached or not -- refuses with `python-1 is running; python_status` rather
than interleaving with it. Five answer anyway: `python_status`,
`python_output`, `report`, which is rendered from the trace already on
disk (no manifest, no flush: the job owns both writers, and the trace
reader is built to end short of a record the writer has not finished), and
`observe` and `look`, which read the latest completed frame rather than the
run. The step thread copies each finished frame -- its pixels, both pads and
every watch value sampled for it -- into the run under a mutex of its own,
and those two take a copy from under that mutex, so a job stepping at full
rate is watchable from outside. What they answer is the last frame the job
finished, not the one it is making, and `observe` measures its change
against the last frame `observe` itself answered. `peek` and `memory` are
still refused: they read the core's live memory, which the job is writing
between one step and the next. `shutdown` waits for the job and says so;
nothing ever kills one.

The job runs on a worker thread that owns the run while it lasts, and the
endpoint's thread goes on answering. The interpreter is lent to the worker
for the job's lifetime and taken back the moment the job is waited for, so
python is only ever on one thread. `python_status` reads what the job has
printed through its own lock, and the frames it has made through a counter
written where a second thread may read it.

A `python_file` call names its file with `--path`, and the two arguments
every python call takes come after it in the same way: `--frame_budget` in
frames (`0` for no limit) and `--detach true`. Detached, the budget is
already none, so a long job names neither.

    tash session python_file --port 7784 -- --path player.py --detach true
    tash session python_status --port 7784

A synchronous call is the same job with the caller waiting on it, so a
client whose HTTP deadline passes loses the answer and nothing else: the
job goes on as a detached one would, and `python_status` and `python_output`
answer its output and its result afterwards. Nothing printed is discarded.

`tash run --scenario` needs none of this: it is not behind a deadline.

### The frame budget

A loop with a wrong bound used to take the server with it: the run went on
stepping, and a synchronous `python` holds the one thread the server answers
on, so nothing could be asked and nothing could be told to stop. Every
`python` and `python_file` call the caller waits for therefore runs under a
budget of frames, `frame_budget` frames or `DEFAULT_FRAME_BUDGET`, 360,000,
when the caller names none: a hundred minutes of video at sixty a second,
not a day's runaway loop.

A detached job defaults to `python::UNLIMITED`, no budget at all, because
the reason for the default is not true of it: it holds nothing the server
answers on, `python_status` says where it has got to and `python_cancel`
stops it at its next moving call. A detached job is the run of hours the
tool exists for, and a budget it never chose would end it at 100 minutes
with a `tash.BudgetExceeded` that reads as the source's fault. `frame_budget`
still names one for a detached job that wants one.
It is counted on the run's own frame counter, which never goes backwards,
so every path that steps -- `step`, `run_until`, `play`, a search, a probe
-- is counted; and it is asked before a moving call rather than inside one,
so the frames a call may run (`step`'s count, `run_until`'s timeout) have to
fit in what is left. Past it the call raises `tash.BudgetExceeded`, which a
job may catch, and the refusal ends with the frames the job ran.

The refusal names the budget, the frames spent and the frames left, and so
does every answer the three tools write, because a job that dies on a
budget nobody chose otherwise reads as the source's fault.

`python_cancel` is the same stop by another hand: it has the budget refuse
the job's next moving call. It reaches a detached job, because the loop is
free while the worker thread runs it and busy inside a synchronous one.

Both streams come back. What a job writes to stderr is captured beside what
it writes to stdout and answered after it, under a `stderr:` line, so a
reader knows which stream wrote which.

### The time-lapse video

`tash serve --video-stride N` and `tash run --video-stride N` encode one
frame in N, at the profile's own fps, so a run of millions of frames is a
film somebody can watch: at 60, an hour of play is a minute of footage.
The default is 1. Above 1 the audio is dropped rather than stuttered
against a video sixty times its own speed, and `run.yaml` says
`video_stride: 60` so the report's header can read `video: one frame in
60, no audio`.

The trace, the tape, the replay, `look`, `shot` and `clip` are untouched:
every frame is still recorded, judged and replayable, and a shot is still
a full-rate still. Only the encoder sees fewer frames.

### The predicate grammar

`run_until`, `anchor` and `expect` take an anchor predicate in the text form
`AnchorCheck::Wording()` prints and `tape::AnchorFrom()` reads:

    none
    exact_hash <hex>
    difference_hash <hex> within <n>
    perceptual_hash <hex> within <n>
    template_image <path> at <score>
    watch <name> equal|not_equal|less|less_or_equal|greater|greater_or_equal <value>
    colour <r,g,b> within <n> over <x,y,w,h> at least <count>

Two of those join with `or`, and any of them turns over with `not`:

    colour 248,0,0 within 24 over 96,64,64,64 at least 300 or colour 0,248,0 within 24 over 96,64,64,64 at least 300
    not watch level equal 0

`or` binds loosest and `not` tightest, there are no parentheses, and every
alternative is judged against the same frame, so which of them holds first
never changes the answer. A tape's anchor writes the same thing as
`kind: any_of` over an `any_of:` list, and `negated: true` is `not`.

One grammar for the three tools, the same one a tape's anchors are written
in, and the same one the scenario API's `run_until` and `expect` take, so a
session line transcribes into a scenario line unchanged.

`colour` counts the pixels of the crop whose channels are all within `n` of
`r,g,b`, and holds at `count` of them or more: the ghost a strategy game
draws red where a building is refused and green where it is allowed is a
`run_until` away, with no picture leaving the session. The channels are the
eight-bit ones a PNG of the same crop holds, so a colour read off a shot is
the colour to name here; the frame is RGB565, so its brightest red is 248
and `within` covers the rest. `run.colours()` answers the count itself.

### Checkpoints on disk

`checkpoint` keeps the state in the run and also writes
`<checkpoints>/<rom hash>/<name>.state`. The key is the ROM's content rather
than its path, so a later `launch` of the same ROM restores what an earlier
session left behind, and a different ROM never restores into this one.
`restore` reads the file when the run has no such state in memory. When it
has one, that copy wins even if the file is newer: it is the state this run
took, probed on this run, and it carries the place on this run's line, so
serving a file another process wrote would reseed the tape under a session
that thinks it is still on its own line. The answer names the newer file --
`restored plan from this run; <file> is newer and was not read, shutdown and
launch again to take it` -- and that relaunch is what closes a session's
hold on the name.

The store is `python::CheckpointStore` and the tools call
`ScenarioRun::Checkpoint` and `::Restore` for it, so `run.checkpoint` writes
the same files with the same probe and the same fold; the tools only say
where they went. `tash run` writes it too, and both resolve where through
the one `CheckpointsUnder`: `_checkpoints` beside the bundles, or the
`--checkpoints <dir>` either was given. So a checkpoint a scenario run took
restores in a serve session over the same `_runs`, and the other way round.

Beside the state goes `<name>.png`, the frame the checkpoint was taken at: a
core does not render on unserialize, so a restored run would have no frame
until something stepped. Both restores put that frame back on the bus, and
`observe`, `look`, `anchor` and `expect` answer straight away.

### Clips

Design 11 cuts a clip out of the recorded video; the raw frame ring is
bounded and only the last frames are still in it, so `clip` records the
window rather than the pixels: a trigger record naming the label, with
`<from> <to>` in frames. The recorder's mkv has one frame per PTS at the
run's fps, so the window is enough to cut from later, and a clip is never
limited to what the ring still holds.

## The transport

`McpServer` sits on `oxbox::http::Server`. Both POST and GET are registered
as *stream* handlers, because the content type of a POST answer is chosen at
runtime -- `text/event-stream` when the client's `Accept` names it,
`application/json` otherwise -- and `ResponseStream::Begin` is where that
choice is made. `Run()` is the endpoint's thread: the core, the embedded
interpreter and the handlers all live on it, which is what makes one process
one session.

v1 is one live Core per process, but not one session: `initialize` assigns
an `Mcp-Session-Id` and the server keeps every id it has minted, so an agent
holding a conversation and a one-shot `tash session` can share the port. A
later request without an id, once any session lives, is `400`; one carrying
an id the server never minted is `404`; `DELETE` ends that one id. The
dispatcher's lifecycle gate is the server's, so it resets only when the last
session has gone and the next caller has to `initialize` again rather than
walk into a tool on an id-less POST.

A session with no request for `SESSION_IDLE` -- thirty minutes -- is dropped
as its own `DELETE` would drop it, and its next call is the `404` that asks
for a fresh `initialize` (transports 2.5). A stream a client is still
reading counts as a request on every beat, so the timer measures a client
that has gone rather than one that is waiting. Thirty minutes is longer than
any pause an agent leaves between two calls and short enough that a client
that died holds neither the port nor the run for an afternoon.

`shutdown` ends the server as well as the run: once a run has been open on
this server, and it is closed, and no session is left, the endpoint stops
and the process ends. So `tash serve --profile <p>` followed by `tash
session shutdown` frees the port by itself -- the one-shot `DELETE`s its own
id, which is the last one -- and nothing has to be killed by pid. A session
that shuts down and `launch`es again never trips it: its own id is still
there. Nor does a server started without `--profile` and never launched:
there was no run to close.

## The cli mirror

    tash serve --profile <p> --port <n> [--bundle <root>] [--video-stride <n>]
    tash session <tool> [--port <n>] -- --arg value ...

`tash session` connects to a running server, sends the same JSON, and prints
the result's text. It reads the port from `--port` or from `TASH_MCP_PORT`.
One call is one session: it `DELETE`s its own id when the call is done, so a
run of them leaves nothing behind on a server another client is talking to.
The tool's own arguments come after a bare `--`: the cli parser refuses an
option it does not declare, and these are the tool's, not the command's.
They are typed from the tool's own schema, so `--frames 10` is a number and
`--hold a,start` is an array.
