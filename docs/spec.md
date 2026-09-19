# Specification

## 1. Purpose

tas-harness turns "start the app, take a screenshot, press a key, take another
screenshot" into continuous, evidence-producing play. It drives a real-time
graphical target the way a human tester would, watches its video, audio,
memory and events the whole time, judges what it sees against expectations,
and records everything so a verdict can be re-examined later.

The central use is the **differential run**: an original game running in an
emulator is the reference, its native rebuild (a decompilation or port on
SDL) is the candidate, both receive the same inputs frame by frame, and the
harness reports where and how their video, audio and state diverge. Every
other capability (tapes, anchors, perception, controllers, the agent
surface) exists first to make that run fast to author, fast to execute and
exact to judge; single-target playtesting is the same machinery with one
side.

It is agent first. The primary user is a coding agent that is developing or
testing a game and needs to get through game scenarios quickly, verify
outcomes, and leave evidence a human can check without replaying anything. The human is the consumer of that evidence and the author
of the hard parts of a scenario; the agent is the one running the loop many
times a day. Every design choice is judged by whether it makes the agent's
loop faster, more exact, and cheaper in tokens.

Secondary users:

- a developer running a regression playthrough of a rebuild after every
  change, in CI, against the original as reference
- a person building a scripted or model-driven playthrough of a single
  target, native or emulated

## 2. Goals and non-goals

Goals:

- A rebuild and its original are driven in lockstep from one tape and
  compared per frame; the first divergence is found, checkpointed on both
  sides, and reported with the frame pair, the difference image, the audio
  window and the state difference.
- An agent can drive a target from its own tool calls, with observations that
  are cheap to read (text, numbers, hashes, small crops) and a screenshot only
  when it asks for one.
- An agent can get to any point in a game in seconds: checkpoints cached at
  anchors, save states where the target supports them, tapes that replay the
  way there.
- Full or partial playthroughs at human pace, or faster than real time when
  the harness controls the target's clock.
- Works with targets that link the harness library and with black-box
  targets, with the same scenario API on top.
- Every run produces a self-contained bundle: video, audio, inputs, events,
  memory watches, screenshots, verdicts, report.
- A per-target Python scenario adapts the harness to a game; the harness
  itself stays generic.
- Perception and control are tiered by cost, so cheap mechanisms carry the
  bulk of a run and the agent tier is the exception, not the loop.
- Headless and CI-capable: a run in a container with a virtual display and a
  virtual audio sink is a first-class case.

Non-goals for v1:

- Frame-exact input into black-box targets that the harness cannot step.
  Wall-clock injection with anchor-based resynchronisation is the offer there.
- Being a general reinforcement-learning environment. Dataset export for
  imitation learning is in scope; a reward-and-policy training loop is not.
- Cheating or bypassing anti-tamper in third-party software. Memory access to
  a process is a debugging facility for software the user is entitled to
  inspect.
- A GUI. The CLI and the run report are the interface.

## 3. Vocabulary

These names are used throughout the documents and are the intended names in
code.

- **Target** — the program under test, one process (or an emulator process
  hosting a guest).
- **Adapter** — the bridge between harness and target. It supplies input
  injection, frame capture, audio capture, memory access, events, and, when
  possible, clock control. Kinds: *linked* (the target links the harness
  library), *emulator* (the emulator exposes an automation API or links the
  library on the guest's behalf), *hooked* (the harness interposes on the
  target's graphics and input calls without source), *external* (OS-level
  injection and capture, nothing inside the target).
- **Observation** — one timestamped bundle from the target: a frame, an audio
  chunk, watch values, events. Each carries harness monotonic time, wall
  time, and the target's frame index when the adapter knows it.
- **Time base** — how a moment is addressed: *wall* (milliseconds of real
  time), *frame* (target frame index), *anchored* (relative to an anchor
  event).
- **Determinism level** — what the adapter guarantees:
  - D0: nothing; the target runs free (external adapter)
  - D1: frames are observable and countable (hooked, some emulators)
  - D2: the harness steps the target frame by frame and owns its clock
    (linked, emulator with step API)
  - D3: D2 plus seeded randomness and no other nondeterminism; a tape replays
    to bit-identical frames
- **Input** — a change to the virtual input devices the target sees: keys,
  mouse, gamepad axes and buttons, text, and adapter-specific channels.
- **Tape** — a recorded or authored input sequence, split into segments. Each
  segment has a start anchor, a time base, the inputs, a timeout, and a
  recovery policy. Tapes are recorded from a human, an agent, or a
  controller, and edited as text.
- **Anchor** — a predicate over observations that marks a moment: an exact
  frame hash, a perceptual hash within a distance, a template match in a
  region, a watch value, an event, an OCR result, an audio cue. Segments start
  at anchors; expectations are checked at anchors.
- **Controller** — a source of inputs. Tiers, cheapest first: *tape*,
  *script* (Python code), *algorithmic* (a function of watches and
  observations), *reflex* (a trained vision model), *agent* (a language
  model with tools). The **arbiter** chooses which controller's decision
  reaches the target each frame.
- **Oracle** — a check that yields a **verdict**: pass, fail, or uncertain,
  with evidence (frame ids, crops, values, a rationale). Oracles run on
  observations continuously or at anchors.
- **Trigger** — a predicate that fires an action (screenshot, start or stop
  recording, mark, escalate to a higher controller tier, abort).
- **Recorder** — writes the run bundle: video, audio, trace, screenshots.
- **Trace** — the append-only log of every observation summary, input,
  verdict, trigger and controller decision in one time line.
- **Run bundle** — the output directory of one run, self-describing.
- **Fidelity level** — the claim a scope makes about reference and
  candidate: F3 pixel, F2 state, F1 behaviour (same events, same order,
  within a frame tolerance), F0 outcome. Defined in archetypes.md.
- **Reference trace** — a recording of the reference side (frame hashes and
  downscaled frames, watches, derived events, audio fingerprints, inputs, by
  logic frame) that stands in for a live reference in a differential run.
- **Scenario** — a Python module for a target that composes adapters, tapes,
  controllers, oracles and triggers into a run.
- **Profile** — per-target static configuration: how to launch it, adapter
  kind, memory watch definitions, input mapping, screen regions, fonts.

## 4. Functional requirements

### 4.1 Integration

- R1. A target that links the harness library gets input, frame and audio
  submission, watches, events and clock control through one C++ header
  (grilling 18). With no harness attached the calls are no-ops and the
  target runs normally.
- R2. A target with no source is driven by the external adapter (OS-level
  input devices, screen and audio capture, process memory) with no changes to
  the target.
- R3. A hooked adapter interposes on the target's present and input calls to
  obtain frame-exact capture and frame counts for black-box targets on Linux.
- R4. Emulators the user controls (emuex) integrate through the linked
  library and expose guest RAM, guest frames and the guest clock. Third-party
  emulators with an automation API get an adapter that maps that API.
- R5. The scenario API is the same over all adapters; a scenario asks the
  adapter for its determinism level and capabilities and degrades explicitly.

### 4.2 Input

- R6. Keyboard, mouse (relative and absolute), gamepad (buttons, axes,
  triggers), and text entry.
- R7. Inputs are addressed in any time base. Frame-addressed inputs require
  D2 or better; the harness refuses rather than approximates silently.
- R8. Every injected input is logged in the trace with the moment it was
  requested and the moment the adapter delivered it.
- R9. Inputs can be recorded live from real devices into a tape, with the
  observations that anchor them.

### 4.3 Capture

- R10. Frames are captured at the target's native resolution and pixel
  format, with the frame index when known; otherwise sampled at a configured
  rate with wall timestamps.
- R11. Audio is captured as PCM per run, isolated from other programs.
- R12. Frame delivery from a linked target costs the target no more than a
  copy into shared memory, and zero copies when the platform allows handle
  passing.
- R13. Frame hashes (exact and perceptual) are computed over raw frames, never
  over encoded video.

### 4.4 Time and speed

- R14. In D2, the harness steps the target: one frame per step, N frames per
  step, or free-run until a predicate. Faster than real time is the normal
  case when nothing throttles.
- R15. In D0 and D1, the harness paces itself to the wall clock and reports
  drift.
- R16. A budget applies to every controller and oracle. In D2, budgets are in
  wall time while the target waits; in D0 and D1, budgets are in frames or
  milliseconds and an overrun falls back to the hold policy.

### 4.5 Perception

- R17. Image helpers: exact hash, perceptual hashes (dHash, pHash), raw
  difference and change amount, SSIM, template match, colour masks, region
  stability, motion estimation, all over regions of interest.
- R18. Text: OCR through a general engine, and exact bitmap-font matching for
  targets with known fonts.
- R19. Audio: level and silence detection, fingerprint match against a
  reference clip, speech-to-text with keyword triggers.
- R20. Reflex models: an ONNX model that maps recent frames (and optionally
  watches) to an input decision, run under a per-frame budget on the GPU when
  present.
- R21. Agent: a language model with tools to observe (frames, crops, OCR,
  watches, events, trace tail), act (submit a tape fragment or a script
  call), judge (a verdict against a rubric) and annotate.

### 4.6 Control

- R22. The arbiter takes one decision per frame from the highest-priority
  controller that has a fresh decision, and applies the hold policy when
  none has.
- R23. Escalation: a controller, an oracle or a trigger can hand control to a
  higher tier for a bounded scope (until an anchor, a timeout, or a frame
  count), after which control returns.
- R24. Hold policy per scenario: neutral input, repeat last, or a named
  "safe" tape (for example, open the pause menu).

### 4.7 Oracles

- R25. Image oracles (anchor predicates used as checks), memory oracles
  (equals, in range, monotonic, changed within N frames), event oracles
  (occurred, ordered, within), audio oracles, text oracles, timing oracles
  (frame time, stall detection), agent oracles.
- R26. Verdicts carry evidence and appear in the trace and the report. A
  scenario decides which verdicts fail the run.

### 4.8 Recording

- R27. Video with the audio track, encoded on the GPU when present, with a
  lossless option. Frame index and trace time are recoverable from the
  container timestamps.
- R28. Screenshots on demand and on trigger, as lossless PNG, with the crop
  region if any.
- R29. The trace is a typed append-only file. A tool dumps it to text or
  loads it into SQLite for queries.
- R30. The run bundle includes the profile, the scenario revision, the tapes
  used, the harness version and the target's version or hash.
- R31. A report (static HTML) lays the video, verdicts, screenshots and trace
  on one time line.

### 4.9 Scripting

- R32. The per-game customisable parts are a shared library plugin, a
  Python module, or a combination of the two. Python scenarios are async,
  with an API of awaitable predicates and actions. They live in the
  target's own repository.
- R32a. The Python runtime is embedded in tash, and Python has a full set
  of bindings to the entirety of tash's internal functionality. API design
  of the bindings is a first-class deliverable.
- R33. A scenario can be run whole, from a named checkpoint, or as a single
  segment, for iteration while authoring.
- R34. Tapes are editable text files; a tape can embed anchor images by
  reference.

### 4.10 Memory

- R35. Linked targets register watches by name, address and type. External
  targets get watches from a profile: module-relative addresses and pointer
  chains. Emulators expose guest RAM by address.
- R36. Watches are sampled per frame (D1 and up) or at a configured rate, and
  logged in the trace.

### 4.11 Agent surface

- R37. The harness exposes an agent tool surface (MCP over HTTP, and the
  same tools through the CLI) over a live session: launch, step, observe,
  act, anchor, checkpoint, judge, record, report, python. Every observe
  call returns text first (watches, OCR, events, hashes, change amount) and
  pixels only on request, as a crop or a downscaled frame, so the agent
  spends tokens on judgement rather than on looking.
- R37a. The python tool loads a Python script into the running tash and
  evaluates it there, against the live session, without reloading any
  state.
- R38. Checkpoints: reaching an anchor can snapshot the target's state and a
  later run resumes from it instead of replaying from launch. The mechanism
  is the best one the adapter has: save state (emulator, linked targets with
  state hooks), in-process memory snapshot (hooked), sandbox checkpoint with
  CRIU (external, containerised, software rendering), VM snapshot (external
  in a VM, any guest OS), or tape-prefix replay as the universal fallback.
  The scenario API is the same over all of them and the adapter reports
  which one applies.
- R39. The agent can author and edit scenarios, tapes and profiles as files,
  and run any part of them in isolation, without restarting the target when
  the adapter can rewind.
- R40. Evidence for the human: every run yields a bundle whose report can be
  read in a browser in under a minute: the verdicts, the moments that matter
  as screenshots and short clips, and a link into the full video at each of
  them.

### 4.12 Differential runs

- R43. A run can hold two sessions, reference and candidate, on different
  adapters (typically emulator and linked), and step them in lockstep from
  one tape in the game's logic-frame base. Both sides must be D2 or better;
  the harness refuses otherwise.
- R44. One logical input map feeds both sides: a tape says `A`, the
  reference gets the emulated pad's A and the candidate gets whatever its
  profile binds.
- R45. Frame comparison is a pipeline the scenario assembles from
  documented building blocks (scaling, filtering, palette or pixel-format
  conversion, regions of interest, exact hash, fuzzy hash, per-pixel
  difference scoring, perceptual distance, OpenCV operations, OCR). What
  counts as a divergence is goal, scenario and application dependent; the
  harness ships the tools and their documentation, and the testing agent
  plans and assembles the final pipeline. There is no harness-wide default
  strictness.
- R46. State comparison: tash provides an API for memory and variable
  readout on both sides (reference addresses with type and endianness,
  candidate watches); the pairing and any conversion are written by the
  agent or tester case by case, in the scenario, and evaluated every frame
  or at anchors.
- R47. Audio comparison over aligned windows: sample-exact when both sides
  produce the same rate, otherwise spectral distance with a threshold.
- R48. Divergence handling: on the first frame that fails a comparison the
  harness records the pair, checkpoints both sides, and either stops,
  continues while counting, or resyncs the candidate to the reference's
  state through the state map where the scenario allows it.
- R50. The reference can be live or a reference trace recorded once from a
  live run; a reference trace supports every comparison except full-frame
  pixel diff images beyond the stored downscale and `resync`.
- R51. Each scope claims a fidelity level; comparators for F1 derive
  reference events from watch transitions declared in the profile and
  compare event sequences with a per-event frame tolerance.
- R49. The report for a differential run shows the two videos side by side,
  the difference curve over the run, and each divergence with frame pair,
  difference image, state diff and audio window.

### 4.13 Tooling

- R41. One CLI: run a scenario, record a tape, play a tape, dump a trace,
  render a report, export a dataset, list adapter capabilities, serve the
  agent surface.
- R42. The first choice is a run inside a headless container with a
  virtual display and a virtual audio sink, with GPU access when available,
  emitting the bundle as a CI artifact; the host display is the fallback
  when the container does not perform reasonably or the target needs it.

## 5. Non-functional requirements

- N1. Overhead on a linked target at 1080p60: under 0.5 ms per frame on the
  target's thread for submission; hashing and encoding happen off that
  thread.
- N2. External capture keeps up with 60 Hz at 1080p on the dev box; drops
  are counted and logged, never silent.
- N3. Reflex tier decision latency under one frame at 60 Hz on the dev GPU.
- N4. A D3 tape replays to bit-identical frame hashes across runs; the
  harness has a test proving it on the sample target.
- N5. Linux first, running in a container on a headless box (X11 and
  Wayland through kernel-level input and the compositor's capture path);
  Windows second; macOS third.
- N6. As much as possible in C++. Python is embedded for the customisable
  and experimental parts and binds everything; it is never a second
  implementation of something the core does.
- N7. All harness code follows the site conventions (buildutil, naming,
  comment rules, no globals).

## 6. Traceability from the brief

| Brief item | Where it lands |
|---|---|
| OpenCV helpers | R17, perception module |
| fine-tuned vision model for reflexes | R20, reflex tier, dataset export |
| agent intervention | R21, R23, agent tier |
| memory inspection, watches | R35, R36 |
| frame hashing as anchors | R13, anchors, tapes |
| agent vision for complex scenarios | R21 observe tools |
| Python scripting | R32, R33 |
| video/audio recording, screenshots on trigger | R27, R28, triggers |
| input injection, from OS events to self-injection | R1, R2, R3, R6 |
| raw diff and fuzzy hash comparison | R17 |
| agentic screenshot comparison | R21 judge tool, agent oracle |
| OCR scanning for points of interest | R18 (R19 for audio only dialogue) |
| frame change amount detection and triggering | R17, triggers |
| memory-based expectation verification | R25 memory oracles |
| wall / frame / smart timed sequences | time bases, anchors, R7 |
| algorithmic input from state | algorithmic tier, R22 |
| near real time or faster | R14, R15, determinism levels |
| agent first: speed through, verify, capture evidence | R37 to R40 |
| original in emulator vs native rebuild | R43 to R49 |
