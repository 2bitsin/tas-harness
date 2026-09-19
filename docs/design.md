# Design

This document says how the harness is built. Names are the ones in spec.md
section 3. Where a mechanism is chosen, the choice is the one a domain expert
would make for this program at its scale; the alternatives that were
rejected are not recorded here.

## 1. Shape

```
                    a coding agent                      human
                     │  MCP / CLI                          ▲ report, clips
                     ▼                                     │
 ┌───────────────────────────────────────────────────────────────────┐
 │ tash process (C++, embedded Python runtime, plugins)              │
 │                                                                   │
 │  scenario ──► arbiter ◄── controllers: tape, script, algorithmic, │
 │     │            │                     reflex (ONNX), agent       │
 │     │            ▼                                                │
 │     │      input queue ──────────────────────────┐                │
 │     ▼                                            │                │
 │  oracles, triggers ◄── perception ◄── observation bus (shm ring)  │
 │                            │                    ▲                 │
 │                            ▼                    │                 │
 │                     recorder: video+audio, trace, shots, report   │
 └──────────────────────────────────────────────────┼────────────────┘
                                                    │ adapter
                          ┌─────────────────────────┼──────────────────┐
                          │ linked │ emulator │ hooked │ external       │
                          └─────────────────────────┬──────────────────┘
                                                    ▼
                                             target process
```

Two processes at minimum: tash and the target. tash is a C++ program that
does as much of the heavy lifting as possible: bus, clocks, hashing,
encoding, capture, adapters, perception, recorder, the linked library. It
embeds the Python runtime. The customisable bits for a concrete game are a
shared library plugin, a Python module, or a combination of the two,
whichever the game calls for. Python has a full set of bindings to the
entirety of tash's internal functionality, so anything the C++ side can do
a script can do; the bindings are designed as an API, not as a mirror of
the classes. Python exists for speed of implementation and experimentation;
when a piece settles, it can move to C++ or to a plugin.

The agent talks to a *session*: a tash process that holds a launched
target. The interface is MCP, served over HTTP or through the CLI. Both
are front ends on the same session API, and the same API is what a Python
script sees from inside tash.

## 2. Observation bus

The bus is a set of ring buffers in shared memory owned by the harness and
mapped by the adapter side (the linked library inside the target, or the
capture threads of the other adapters):

- **frame ring**: N slots (default 8) of frame descriptors plus pixel
  storage. A descriptor carries frame index, harness time, wall time, width,
  height, stride, pixel format, and the storage reference: an offset into
  the ring's pixel area, or a DMA-BUF file descriptor on Linux when the
  target renders on the GPU and can export its swapchain image. The slot is
  free again once every consumer has released it; a slow consumer causes a
  counted drop, never a stall of the target, except in stepped mode where
  the step does not complete until the frame is consumed.
- **audio ring**: interleaved PCM at the target's rate, chunked, with the
  frame index of the chunk's first sample when the adapter knows it.
- **watch table**: one fixed-size record per registered watch, written by the
  adapter each frame, read by the harness. Types: integer widths, float,
  double, fixed byte arrays. An integer says which way round its bytes read:
  `big`, `little`, or `swapped` -- little-endian 16-bit words in big-endian
  order, which is how a 68000 longword reads out of byte-swapped work ram.
- **event stream**: a single-producer ring of typed events (name id, frame
  index, up to a few typed fields). Names are interned once through the
  control channel.
- **control channel**: a Unix domain socket (named pipe on Windows) for the
  slow path: handshake, capability negotiation, watch registration, step
  commands, state save and load, shutdown.

Frame hashing runs in a harness worker thread on the raw slot before
release: xxh3 over the pixels for the exact hash, dHash and pHash over a
downscaled grey copy for the perceptual hashes. These three numbers go into
the trace for every frame, so anchors and change detection are cheap and
after-the-fact analysis needs no video decode.

## 3. Clocks and determinism

The adapter reports a determinism level (D0 to D3). The harness exposes one
`Clock` abstraction with three modes:

- **free**: the target runs on its own; the harness observes at whatever
  rate frames arrive (D0, D1).
- **paced**: the harness releases frames at a target rate, which can be
  real time, a multiple, or unlimited (D2).
- **stepped**: nothing advances until the scenario says `step(n)` or
  `run_until(predicate)` (D2).

In stepped and paced modes the linked library's `frame_begin` blocks the
target's main thread until the harness releases the frame and hands over the
input state for that frame. Waiting is the target's only cost; the harness
can spend seconds on an agent call between frames and the game cannot tell.

D3 adds what the target must do for bit-exact replay: take its time step from
the library (`clock_dt`), take its random seed from the library, avoid
reading wall time and other outside state during play. The library documents
this contract and the sample target proves it with a replay test.

## 4. Adapters

All adapters implement one C++ interface: capabilities, launch, attach,
inject, step, capture streams, read and write memory, save and load state,
shutdown.

### 4.1 Linked

The target links `libtash` and calls it at three points
per frame: begin (get input, block if stepped), submit video, submit audio.
Optional: register watches, emit events, take dt and seed from the harness,
implement save and load state callbacks. One C++ header, `tash.hpp`, and
nothing else: the same conventions as the rest of the tree, and a `Harness`
that closes itself (grilling 18). Every call is an inline test of one
pointer before the out-of-line half, so an unattached frame costs a test
and a jump and never a call.

The library has three states. **Attached**: the bus above, over the control
channel an environment variable names -- the harness drives the frames and
hands the input over. **File**: nothing drives the target, and the library
writes down what the target hands it -- the inputs as `tape.yaml`, the
frames, watches and events as `trace.bin`, the run as `run.yaml`, in a
directory another environment variable names, or into memory for a host
with no filesystem (the browser), where the target fetches the same bytes
and saves them its own way. **Unattached**: every call is a no-op.

File mode is what a target that runs where no harness can reach it -- a
player's Windows box, a browser tab -- brings back: the harness replays the
tape frame by frame and reads the trace beside it. It records; it does not
drive, so the target keeps its own dt, its own seed and its own pacing, and
the library writes down which it used.

Input arrives as a full device state per frame (key bitmap, mouse position
and buttons, gamepad axes and buttons), not as events, so the target does
not depend on event ordering. A helper generates the events for engines that
want them.

Video submission takes a pointer, size, format and stride and copies into
the ring slot, or takes a DMA-BUF descriptor and copies nothing. Audio
submission takes a PCM chunk. In file mode the video is hashed where it is
submitted -- xxh3 over each row's visible bytes, the digest the harness
computes for the same frame -- and dropped; the audio is dropped
with it, since no format a recording writes holds a sample and the encoder
lives in the harness.

A tape written this way carries the pointer the target sampled: mouse
buttons as `m<port>.<button>` transitions, positions in a `pointer` block
of its own (grilling 17).

State hooks: the target registers `save(buffer)` and `load(buffer)`; the
harness calls them at checkpoints. A target that cannot do this still gets
tape-prefix checkpoints.

### 4.2 Emulator: a libretro host

The harness is its own emulator front end: the emulator adapter loads a
libretro core (`.so`) in-process and drives it through the libretro API.
That gives, for every system a core exists for: one `retro_run` per logic
frame (stepped, D2), video and audio delivered by callback straight into
the bus, the input state supplied per frame from the harness, guest memory
through `retro_get_memory_data` (system RAM, save RAM, video RAM where the
core exposes it), and save states through `retro_serialize`. Cores with no
hidden nondeterminism are D3. Genesis Plus GX and BlastEm cover the Mega
Drive, DOSBox-Pure and dosbox-core cover DOS, and the same adapter covers
the rest of the catalogue unchanged.

Hosting the core beats scripting a third-party front end (RetroArch's
network commands, BizHawk's Lua) because there is no process boundary, no
frame pacing to fight, no wall clock, and no second copy of every frame.
An emulator that only exists as an application (or the user's own emulator
without a libretro build) can still be driven through the hooked or external
adapters, at the determinism level those give.

### 4.3 Hooked (Linux, no source)

An interposer library injected with `LD_PRELOAD` hooks the present call
(`glXSwapBuffers`, `eglSwapBuffers`, `vkQueuePresentKHR`, `SDL_RenderPresent`
and `SDL_GL_SwapWindow`) and the input read paths (SDL event pump, X11
`XNextEvent`). At present it reads back the frame (or exports the image) into
the bus, counts frames, and fetches the input state for the next frame from
the harness. This gives D1 with frame-exact capture and D2 if the harness
also gates the present call. It is libTAS's approach, limited to what this
harness needs.

When no hookable present or input path exists, the same adapter falls back
to the debugger route: attach with the native debugging API (`ptrace` on
Linux, the debug API on Windows), analyse the binary enough to find the
frame boundary and the input read, and insert breakpoints or patched calls
there. The aim is the minimal set of control the task at hand needs, such
as stepping frames and inspecting memory, not full instrumentation.

### 4.4 External (Linux)

- Input: virtual devices through `uinput` (keyboard, mouse, gamepad). The
  kernel delivers them to X11, Wayland and raw-evdev consumers alike, and the
  target cannot tell them from hardware. XTest is the fallback when
  `/dev/uinput` is unavailable.
- Video: the compositor path. On Wayland, PipeWire screen capture of the
  target's window through the portal; on X11, XComposite redirect and XShm
  readback of the target's window. Frames are sampled at the configured rate
  with wall timestamps (D0).
- Audio: a dedicated PipeWire null sink per run, the target's stream routed
  to it, and the monitor captured. Exact per-target audio, no bleed.
- Memory: `/proc/<pid>/mem` with module-relative addresses and pointer
  chains from the profile. Requires ptrace permission over the target, which
  the launcher has because it is the parent.

### 4.5 External (Windows, second)

`SendInput` for keyboard and mouse, ViGEmBus for gamepads, DXGI desktop
duplication or a graphics-hook DLL for frames, WASAPI process loopback for
audio, `ReadProcessMemory` for watches. Same interface, same scenario.

### 4.6 Launcher and sandbox

The harness launches the target itself so it owns the process, the
environment (session socket, preload), the display and the audio sink. The
first choice, in CI and on the headless server alike, is a container with
a nested X server (Xvfb, or a headless Wayland compositor when the target
needs it) and a PipeWire instance, offscreen; GPU passes through with the
nvidia runtime. The host display is the fallback when the container does
not perform reasonably or the target needs it. Attaching to a target
that is already running is supported for the external adapter only.

## 5. Checkpoints

A checkpoint is "the target exactly as it was at anchor X", restorable in
much less time than it took to get there. One API (`checkpoint(name)`,
`restore(name)`) over a ladder of mechanisms; the adapter reports which it
has and the harness uses the best one:

| mechanism | adapters | cost to restore | fidelity | limits |
|---|---|---|---|---|
| save state | emulator, linked with state hooks | milliseconds | exact | target must implement it |
| in-process memory snapshot | hooked | under a second | memory and threads exact | GPU-resident state is not restored; works for software rendering, 2D, and GL that re-uploads |
| sandbox checkpoint (CRIU) | external in a container | seconds | whole sandbox exact, display server included | software rendering only (llvmpipe); Linux |
| VM snapshot (QEMU/KVM) | external in a VM | tens of seconds | exact | software or virtual GPU only; any guest OS, so this is the Windows answer |
| tape prefix replay | all | the play time, fast-forwarded when steppable | as good as the tape's anchors | the only universal one |

The in-process snapshot is libTAS's savestate design: the interposer parks
every thread at a frame boundary (it owns the present call), a helper reads
the process's writable private mappings and each thread's register set
through ptrace, and stores them with the file offsets of open descriptors.
Restore writes the pages and registers back and resumes. The interposer
knows the GL and audio calls, so it can re-issue the ones that matter
(context current, viewport, audio device reopen) after restore.

The sandbox checkpoint uses CRIU on the run container (Xvfb or a headless
Wayland compositor, PipeWire, the target, all children). Docker exposes this
as `docker checkpoint` on the dev box's nested daemon; the harness drives it
directly through CRIU's API where possible. GPU device handles are the reason
it is software rendering only; that is acceptable for the retro and DOS
targets in this workspace.

Checkpoints are named, stored under the run's `_checkpoints/` (or a shared
cache keyed by target hash, tape hash and anchor), and invalidated when the
target binary changes. `restore_or_play(name, tape)` is the scenario call
that makes an agent's iteration loop start mid-game.

## 6. Differential runs

A differential run is two sessions under one scenario: the **reference**
(the original, in the libretro host) and the **candidate** (the rebuild,
linked). The scenario steps them in lockstep: for logic frame N the arbiter
resolves one input state from the tape, the input map translates it for
each side, both sides step once, both frames land in their buses, and the
comparators run before frame N+1 is released. Because both sides are
stepped, the comparison costs nothing in game time and the run goes as fast
as the slower side renders.

Comparators, all declared in the profile and all producing trace records:

- **video**: the candidate's frame is normalised to the reference's
  geometry (scale, crop to the game's active area, convert pixel format,
  optionally quantise to the reference palette), then compared per region:
  exact hash, changed-pixel ratio under a threshold, or perceptual distance
  under a bound. Regions the rebuild draws differently on purpose are
  excluded by the profile.
- **state**: the memory readout API exposes reference addresses (from the
  core's memory regions, with type and endianness) and candidate watches
  (registered by the rebuild through libtash). The scenario pairs them
  case by case; each pair has a comparison: equal, equal after a
  conversion (fixed-point to float, a table index to an enum), or within a
  tolerance. Sampled every frame or at anchors.
- **audio**: aligned windows of the two audio rings; sample-exact when the
  rates match, spectral distance otherwise.

- **events (F1)**: the candidate emits events through libtash; the
  reference's events are derived from watch transitions the profile
  declares (`when bomb_count rises: bomb_placed(player, cell)`). The
  comparator aligns the two sequences (an edit-distance alignment over
  event names and fields) and reports missing, extra, reordered and late
  events, with a per-event frame tolerance.

The reference may be a live session or a **reference trace**: a recording
of one live run of the reference (frame hashes, downscaled frames, watches,
derived events, audio fingerprints, inputs, by logic frame), replayed into
the same comparators. Recording once and comparing many times is the
normal loop for a rebuild under development, and it is the only way to use
a reference that cannot be stepped, such as a Windows 95 game under Wine
seen through the hooked adapter.

Divergence policy per scenario: `stop` (checkpoint both sides at frame N-1,
record the pair, end the scope), `count` (record and continue, for a
"how far off are we" run), or `resync` (write the reference's state through
the state map into the candidate where the rebuild exposes writable
watches, so a run can continue past a known-different routine and find the
next divergence in one pass).

The bundle of a differential run has both videos, one trace with both
sides' records tagged, `divergences.jsonl`, and a report with the two
videos side by side, the difference curve, and for each divergence the
frame pair, the difference image, the state diff and the audio window.
This report is the thing the human reads when a rebuild does not match.

The three archetypes in archetypes.md (a Windows 95 action game under Wine
or DOSBox-Pure, a Genesis RPG in Genesis Plus GX, a DOS adventure in
DOSBox-Pure) exercise every branch of this section; the design is judged
against them.

## 7. Tapes

A tape is a text file with a header and segments; the text format is left
open (the example below is one rendering of the content, not a decision).
A particularly large tape is a custom binary file instead:

```toml
[tape]
target = "bosdox3"
input_map = "profiles/bosdox3/input.toml"
created = 2026-09-12T23:10:00Z
determinism = "D3"

[[segment]]
name = "skip-intro"
anchor = { image = "anchors/title.png", roi = [0, 0, 320, 40], max_distance = 4 }
time_base = "frame"
timeout_frames = 600
on_timeout = "escalate"        # or "fail", "retry", "skip"
inputs = """
0     down ENTER
2     up   ENTER
40    down RIGHT
72    up   RIGHT
"""
```

The inputs block is one line per transition: time in the segment's base,
transition, channel. Held keys are explicit down and up pairs; axes carry a
value. A pointer's buttons are transitions like any other; where it
pointed is a block of its own beside them, one line per frame it moved
(grilling 17). A particularly large tape (a full human play) is a custom binary
file with the same record shape, referenced from a text segment or used
on its own, for efficiency.

Playback: wait for the anchor (or fail the segment on timeout), then replay
in the time base. In wall base the harness schedules each transition at
anchor time plus offset and logs the delivery lag. In frame base it hands
the state for each frame to the adapter. Recovery policies decide what
happens on timeout: fail the run, retry from the segment's checkpoint,
escalate to the next controller tier with the segment's goal as its brief,
or skip.

Recording: the harness listens to the agent's action calls (the preferred
source: the agent, with tash, works out the fastest route that hits the
right scenarios) or, when that fails, to real devices (evdev on Linux)
while the user plays; it writes transitions with the current time base and
inserts an anchor whenever the scenario or the agent marks one. Anchor
images are cropped from the frame at that moment and saved next to the tape.

Replaying into a bundle: a run that searches saves and restores its way to a
line, so its video rewinds at every trial and its report is a record of the
search rather than of the play. A checkpoint folds the line back to the
position it was taken at, not to the frame the run had reached, because the
run's frame counter keeps its maximum across a restore. The tape is what
survived that folding -- `run.yaml` carries its length as `kept` -- and
`tash tape replay --bundle <run> --record <dir>` plays it from power on into
a run opened the way `tash run --bundle` opens one, so the clean
playthrough leaves a bundle of its own -- video, trace, report -- beside the
search that found it. Each segment's name becomes a mark as the player
reaches it, which puts the marks the tape was cut on back on the report's
time line; `run.yaml` names the bundle it replays in `replay_of` and carries
the hash-and-watch verdict as its outcome. A failed check still closes the
bundle, so the film of the divergent line can be watched.

## 8. Controllers and the arbiter

Each controller produces decisions: a full input state plus a validity
window (until frame N or until time T) and a confidence. The arbiter keeps
the highest-priority controller that has a valid decision as the *active*
controller and applies its state each frame. Priority is the scope order set
by the scenario, not the tier order: a tape segment that is playing owns the
frame until it ends or times out; an escalation gives a higher tier the
frame until its scope ends.

Budgets: every controller call has a budget. In stepped mode the clock waits,
so the budget only bounds wall time and cost. In free mode a controller that
misses its budget yields to the hold policy for that frame and the miss is
logged; the reflex tier is designed to fit in a frame and the agent tier is
only used in free mode with a hold policy that keeps the game safe (the
pause menu, or a neutral state in a menu screen).

Tiers:

- **tape**: section 7.
- **script**: the scenario's own Python, which can act directly (`press`,
  `hold`, `move`) between awaits. Runs on the harness thread; the bus and
  perception run on workers so a slow script does not drop frames, it only
  delays decisions.
- **algorithmic**: a Python or C++ function of the watch table and recent
  observations. Typical: "hold RIGHT while player_x < 2000 and no enemy in
  region".
- **reflex**: an ONNX model, input a stack of the last K frames downscaled to
  the model's size plus optional watches, output a discrete action or axis
  values. Run with onnxruntime on CUDA on the dev box, CPU elsewhere.
  Trained from run bundles (section 13).
- **agent**: the coding agent through the tool surface (section 12). Its
  decisions are tape fragments or script calls with a scope, so once it has
  decided the loop runs without it until the scope ends.

## 9. Perception

A C++ module over OpenCV with Python bindings, operating on bus slots
without copies where possible:

- hashes (xxh3, dHash, pHash), Hamming distance, per-region hashes
- raw difference: changed-pixel ratio, mean absolute difference, per-region;
  the *change amount* signal used by triggers
- SSIM, template match (normalised cross-correlation), feature match (ORB)
  for scaled or shifted content
- colour masks and blob detection; region stability (unchanged for N frames)
- motion: optical-flow magnitude over a region, for "is anything moving"
- text: bitmap-font matcher for targets with a known font (the VGA ROM font
  for DOS targets, a game's own sprite font from the profile), exact and
  fast; Tesseract behind the same call for everything else
- audio: RMS and silence, spectral fingerprint match against a reference
  clip, speech-to-text with faster-whisper on the recorded track with keyword
  triggers (runs on a worker with a lag of a few seconds, so it marks points
  of interest rather than driving input)

Everything returns plain values that the trace can store and the agent can
read as text.

## 10. Oracles, triggers, verdicts

An oracle is a predicate over the observation stream with a schedule: every
frame, at an anchor, at the end of a scope. It yields a verdict record:
outcome (pass, fail, uncertain), the oracle's name, the frame range, the
evidence (values, hash distances, crop references, an agent rationale). The
scenario declares which oracles are blocking.

A trigger is a predicate with an action; predicates are the same expressions
as anchors (so a change-amount threshold, a watch condition, a text match or
an audio cue can all fire a screenshot, a clip, a mark, an escalation or an
abort). Triggers are evaluated on the perception worker so they do not wait
for the scenario.

## 11. Recorder and run bundle

- **video**: the frame ring encoded by libav, NVENC (H.264 or HEVC) on the
  GPU, x264 fallback, FFV1 for lossless when asked; the audio ring as the
  second track; container MKV with PTS equal to harness time so the trace
  maps onto the video without a lookup table. Encoding never blocks the bus;
  a slow encoder drops encoded frames and counts them.
- **film**: what a recorded replay leaves, as against a search record. It
  waits on its encoder rather than dropping, upscales by whole pixels to at
  least 1280 wide, codes at x264 `slow` and crf 16 with a keyframe every two
  seconds, and darkens the last output row of every source row towards black,
  so the rows the upscale invented read as a CRT and not as four copies of
  the same line. A film is watched from its start, so no clip is cut from one.
- **screenshots**: PNG from the raw slot, full frame or crop, on demand and
  on trigger.
- **clips**: a trigger or the scenario can mark a window; the report cuts a
  short clip from the video for it.
- **trace**: an append-only binary file of typed records (little-endian,
  versioned header, one record kind per bus event, input, decision, verdict,
  trigger, mark). `tash trace dump` prints it, `tash trace sqlite` loads it for
  queries.
- **bundle layout**:

```
_runs/2026-09-12T23-10-00-bosdox3-level1/
  run.toml          harness version, target hash, profile, scenario revision, tapes, outcome
  video.mkv
  trace.bin
  shots/            0001-level1-start.png ...
  clips/            death-at-2m13s.mkv ...
  verdicts.jsonl    one line per verdict, for tools that want text
  report.html       self-contained
```

- **report**: one page, static, no server: verdict list with outcomes,
  each linking to the video at its time; the screenshots and clips in
  order; a strip of the change-amount and watch curves; the trace tail on
  failure. The human reads this and nothing else.

## 12. Agent surface

An MCP server (`tash serve`, over HTTP) exposes one session; `tash session`
is the same tool set as CLI subcommands. Tools, with the observe side kept
text-first:

- `launch(profile, adapter, mode)`, `shutdown()`
- `step(frames)`, `run_until(predicate, timeout)`, `pace(rate)`
- `observe()`: frame index, time, exact and perceptual hashes, change amount
  since last observe, watch values, events since last observe, OCR of the
  regions the profile names. Small, always.
- `look(region, scale)`: pixels, as a downscaled or cropped PNG. Only when
  asked.
- `act(script)`: a tape fragment in the text format above, or named actions
  (`tap`, `hold`, `release`, `move`, `text`), applied with a scope.
- `anchor(name, predicate)`, `checkpoint(name)`, `restore(name)`
- `expect(oracle)`: register an oracle for the current scope and get its
  verdict at scope end.
- `judge(question, rubric, frames)`: the harness prepares the evidence and
  the calling agent answers; the answer is stored as an agent verdict with
  the evidence references. The agent that runs the loop is the judge; no
  second model is required unless the scenario asks for one.
- `mark(label)`, `shot(label, region)`, `clip(from, to, label)`
- `python(source)` / `python_file(path)`: load a Python script into the
  running tash and evaluate it there, against the live session, with the
  full bindings and without reloading any state. Definitions persist in the
  session's interpreter, so a script can define a helper and a later call
  can use it. The result and stdout come back as text.
- `report()`: renders and returns the bundle path.

The same operations exist as CLI subcommands against a running session
(`tash session ...`) and as Python calls inside a scenario. A skill in the
agent's config explains the loop: launch, get to the checkpoint, act in
scopes, observe as text, look only to decide, expect and judge, report.

## 13. Reflex model pipeline

Every run bundle is a dataset: frames (or their downscaled copies), watch
values and the applied input per frame. `tash dataset export` writes a
training set from a set of bundles filtered by scope name. A PyTorch
training script in `python/tas/train/` fits a small convolutional policy
(frames stacked over K steps, optional watch vector, discrete action head or
continuous axes) and exports ONNX. The reflex controller loads the ONNX
file with onnxruntime, and the scenario gives it a scope and a budget. The
first models are per-scope, per-game; nothing general is attempted.

## 14. Python API sketch

```python
from tas import Scenario, keys, image, mem, audio, text

async def level_one(s: Scenario):
    await s.launch("bosdox3", adapter="emulator", mode="stepped")
    await s.restore_or_play("after-intro", tape="tapes/skip-intro.toml")

    s.expect(mem.watch("level") == 1, name="starts-in-level-1")
    s.trigger(image.change_amount(roi=s.roi("playfield")) > 0.6, s.shot, "big-change")
    s.trigger(text.appears("GAME OVER", font="vga"), s.escalate, tier="agent")

    async with s.scope("run-to-exit", budget=s.seconds(120)):
        await s.tape("tapes/level1-route.toml", on_timeout="escalate")
    s.expect(mem.watch("level") == 2, name="reached-level-2", blocking=True)

    verdict = await s.judge(
        "Did the player reach the exit without the health bar dropping below half?",
        frames=s.frames_in("run-to-exit", every=60))
    s.shot("level-2-start")


async def match_level_one(s: Scenario):
    ref = await s.launch("game-md", adapter="libretro", core="genesis_plus_gx", mode="stepped")
    cand = await s.launch("game-rebuild", adapter="linked", mode="stepped")
    d = s.differential(ref, cand, profile="profiles/game/diff.toml")
    await d.restore_or_play("after-title", tape="tapes/title-to-level1.toml")

    async with d.scope("level-1", on_divergence="stop"):
        await d.tape("tapes/level1-route.toml")
    # d.divergences() lists frame pairs with state diffs; the report renders them
```

## 15. Repository layout

A buildutil project, conventions per the site rules:

```
tas-harness/
  buildutil.toml
  sources/
    bus/          rings, descriptors, hashing worker, control channel
    clock/        clock modes, pacing, determinism level
    tape/         format, record, play, checkpoints
    trace/        record types, writer, reader, sqlite export
    adapters/     interface; linked, hooked, external.linux, external.win32, libretro host
    differential/ lockstep driver, normalisation, comparators, state map, divergence records
    linked/       libtash: the one C++ header a target links against
    hooked/       the LD_PRELOAD interposer
    perception/   opencv helpers, hashes, fonts, ocr, audio
    recorder/     libav encoding, screenshots, clips, bundle
    python/       nanobind module
  python/tas/     scenario api, controllers, arbiter, oracles, agent server, cli, train
  profiles/       example profiles
  examples/
    sample-target/  a tiny SDL game that links libtash and proves D3 replay
    differential/   the sample target versus a deliberately broken copy of itself
    external/       a scenario against a black-box binary
  docs/
```

Dependencies: OpenCV, libav (ffmpeg libraries), xxhash, nanobind, onnxruntime
(optional, GPU when present), Tesseract (optional), faster-whisper (Python,
optional), libpipewire and libevdev on Linux. All through conan as buildutil
expects. oxbox supplies the command and endpoint base if its shape fits the
control channel; that is a grilling item.

## 16. Prior art and what is taken from it

- **libTAS**: LD_PRELOAD interposition, time control, savestates for
  black-box Linux games. Taken: the hooked adapter design. Not taken: the
  whole-process determinism machinery; the linked library gets that cheaper
  by cooperation.
- **libretro**: the core API that makes one adapter cover every emulated
  system: per-frame run, callbacks for video and audio, input polled per
  frame, memory regions, serialisable state. Taken whole.
- **BizHawk, RetroArch, DOSBox-X**: emulator automation APIs, Lua scripting,
  RAM watch, movie files. Taken: the tape idea, watches, save-state
  checkpoints. Their movie formats are not reused; tapes need anchors.
- **Decompilation matching tools (asm-differ, decomp.me, objdiff)**: they
  compare code; this harness compares behaviour, frame by frame, with the
  same "first divergence" mindset.
- **Gym Retro / stable-retro**: RAM-based state as ground truth for a
  learning agent. Taken: memory watches as the cheapest oracle.
- **Playwright**: the API shape of "await an observable condition, then
  act", tracing, and a report with a time line. Taken: the scenario API
  style and the report.
- **Cheat Engine**: pointer chains and module-relative addresses for
  watches in black-box processes.
- **apitrace / RenderDoc**: present-call hooking for frame-exact capture.
