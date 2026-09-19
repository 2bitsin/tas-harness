# Archetypes: three fidelity harnesses

Worked examples of the differential run for the three kinds of game the
harness must serve. Each answers the same questions: how the reference runs,
how the candidate runs, what fidelity is claimed, which inputs, which tiers
carry the run, what the state map looks like, and what the human reads.

## Fidelity levels

What "the rebuild matches" means is a claim the profile makes per region,
per scope, or for the whole run. From strict to loose:

- **F3 pixel**: frames equal after normalisation. For a faithful
  decompilation on the same logic.
- **F2 state**: the state map agrees every frame; frames may differ where
  the rebuild renders differently on purpose.
- **F1 behaviour**: the same events in the same order, each within a frame
  tolerance of its counterpart. Events on the candidate come from libtash
  calls in the rebuild; events on the reference are derived from watch
  transitions (a memory value changing from X to Y is the event).
- **F0 outcome**: the run ends in the same place with the same result, as
  judged by oracles and, where needed, the agent.

A scope claims a level; a divergence is a failure of that claim. A run can
claim F3 for a battle screen, F2 for the overworld, F1 for the whole run.

## Live or recorded reference

The reference does not have to run alongside the candidate. A **reference
trace** (frame hashes and downscaled frames, watches, derived events, audio
fingerprints, inputs, all by logic frame) is recorded once, and the
candidate is compared against it in lockstep as if the reference were
live. This makes a slow or non-steppable reference (a Windows 95 game under
Wine) usable, and it makes candidate iteration cheap: record the original
once per tape, compare the rebuild a hundred times.

A live reference is required for `resync` and for anything that needs full
frames on demand; a recorded one covers F3 by hash, F2, F1 and F0.

## A. Atomic Bomberman (Windows 95, action, 1997) versus a rebuild

- **Reference**: lightest possible version first: the original under Wine,
  or on the last Windows version it runs on, with the hooked or debugger
  route (D1, real time, frame-exact capture) recorded into a reference
  trace. If that does not work well, step up to emulating the entire OS
  (DOSBox-Pure with a Windows 95 install: stepped, D2 or D3, slow but
  exact); inside an emulated OS the harness may need to run from inside,
  or an era appropriate debugger jerry rigged in between. Trial and error,
  case by case.
- **Candidate**: the rebuild on SDL linking libtash, D3.
- **Fidelity**: F1 behaviour for play (bomb placed, fuse expired, blast
  reached cell, tile destroyed, power-up dropped and collected, player
  killed, round ended), F2 state for the grid (the arena as a byte array
  on both sides), F3 only for static screens (menus, the arena at round
  start). Pixel matching of the play itself is not the claim; the original's
  frame timing under Wine is not stepped.
- **Randomness**: power-up drops and AI are random. The rebuild takes its
  seed from libtash; the reference's RNG state is a watch, and the profile
  either seeds the rebuild from the reference's observed sequence, or the
  scope claims F1 with tolerance on which power-up appears.
- **Inputs**: keyboard for one to two players, gamepad optional. The tape is
  in logic-frame base against the recorded reference.
- **Tiers**: tape for menus and set-piece rounds; algorithmic controller
  (walk to a target cell, drop, retreat, on the grid watch) for play that
  must not be hand-authored; reflex model later for survival; agent for
  judging round outcomes when the state map is incomplete.
- **State map**: arena grid, player positions and speeds, bomb list with
  fuse counters, power-up inventory per player, round timer, RNG state.
  Reference addresses from the decompilation's symbols; candidate watches
  from the rebuild.
- **The human reads**: the event diff (the first event the rebuild emits
  out of order or late), with the frame pair and the grid diff beside it.

## B. A Sega Genesis RPG versus a decompiled port

- **Reference**: the ROM in Genesis Plus GX or BlastEm through the libretro
  host, D3. Live or recorded; live is cheap here.
- **Candidate**: the decompiled port on SDL linking libtash, D3.
- **Fidelity**: F3 pixel for everything the port renders through the same
  logic (menus, dialogue, battles, tiles), F2 state for the whole run:
  party stats, inventory, flags, map position, RNG state, step counter.
- **Randomness**: encounters and battle rolls come from the game's own RNG,
  which both sides carry in state; F2 on the RNG state catches a divergence
  the instant it happens and before its visible effect.
- **Inputs**: the pad. Menus and dialogue are long input sequences; the
  tape records a human or the agent once, anchors on screens by frame hash.
- **Tiers**: tape for nearly everything; anchors on dialogue boxes and menu
  screens by exact hash; the bitmap-font matcher reads dialogue and menu
  text so a scope can wait for "a specific line appears" instead of a frame
  count; agent for authoring new routes (it reads the text and chooses menu
  paths) and for judging when a divergence is cosmetic.
- **State map**: generated from the decompilation's symbol map paired with
  the port's watch registrations; a few hundred entries; conversions rare.
- **The human reads**: the first frame that differs, side by side with the
  difference image, and the state diff at that frame; the difference curve
  shows whether it is one glitch or a drift.

## C. A DOS point-and-click adventure versus a reimplementation

- **Reference**: the game in DOSBox-Pure through the libretro host, D3
  (DOS timers are emulated deterministically). Recorded reference is
  natural: an adventure route is long and the original never changes.
- **Candidate**: the reimplementation on SDL linking libtash, D3. Its
  interpreter's variables (script globals, room, inventory, actor
  positions) are the watches.
- **Fidelity**: F2 state on the script variables and room, F1 behaviour on
  script events (room entered, item taken, dialogue line shown, flag set),
  F3 pixel per room at rest (the room drawn and idle) and for dialogue
  frames when the reimplementation uses the original font path. Animation
  timing between the sides is compared at F1 with tolerance because
  reimplementations commonly retime it.
- **Inputs**: mouse in game coordinates. The input map translates one
  logical click at (x, y) in the game's native resolution to the emulated
  mouse and to the rebuild's window. Walk-to and use-item actions are
  clicks with waits; the tape's anchors are "cursor idle and room stable".
- **Tiers**: tape for the route, with anchors by region stability and by
  text (the font matcher reads verbs, inventory names, dialogue); agent for
  authoring (it reads the room, picks the next puzzle step) and for judging
  that a room looks right when pixels are allowed to differ.
- **State map**: script globals by index on both sides (the reimplementation
  usually keeps the original numbering), room id, inventory, actor
  positions and animation frames.
- **The human reads**: the room where the runs first diverge, both frames,
  the script variable that differs, and the event timeline around it.

## What the three have in common

- The candidate always links libtash and is always D3; the harness's demands
  on a rebuild are fixed and small: take dt and seed from the library, submit
  frames and audio, register watches, emit events, optionally save state.
- The reference is exact where the libretro host applies (B, C) and recorded
  where it does not (A).
- The state map is the centre of value in all three; the generator from
  symbol maps and watch registrations pays for itself on the first game.
- The agent authors routes and judges cosmetic differences; it never has to
  play frame by frame.
