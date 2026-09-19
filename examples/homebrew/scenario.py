"""What the homebrew demo run does: play the tape, then judge the frame."""

import pathlib
import struct

import tash

HERE = pathlib.Path(__file__).resolve().parent
LOGO_HASH = 0x282F2A8E4033B423
LOGO = f"exact_hash {LOGO_HASH:016x}"
SETTLE_FRAMES = 30
# The demo ticks this byte of work ram once a frame; README.md says how it
# was found, and Genesis Plus GX hands that ram byte-swapped.
FRAME_TICK = 0x068e
TICK_FRAMES = 10
TICK_WRAP = 256
# The demo never writes the top of work ram, so that block is still by the
# frame memory_stable first reads it; the tick below never is.
STILL_BLOCK = 0x1c14
STILL_BYTES = 256
STILL_WINDOW = 20
TICK_TIMEOUT = 60
# README.md narrows work ram to the tick with `tash mem search`; this is the
# same hunt from the session, a frame between the steps it must have risen
# across.
HUNT_ROUNDS = 3
CROP = "16,8,64,32"
CROP_SIZE = (64, 32)
# A PNG's IHDR puts the two dimensions at this offset, big endian.
PNG_SIZE_AT = 16

run = tash.run

run.mark("tape")
played = run.play(HERE / "tapes" / "demo.yaml")

settled = run.run_until(LOGO, SETTLE_FRAMES)
seen = run.observe()
shot = run.look()
cropped = run.look(region=CROP)

run.mark("judged")
run.expect("tape-played-every-segment", played["segments"] == 2,
           f"{played['segments']} segments,"
           f" {played['transitions']} transitions")
run.expect("logo-is-on-screen", LOGO)

with open(cropped, "rb") as png:
    size = struct.unpack(">II", png.read(PNG_SIZE_AT + 8)[PNG_SIZE_AT:])
run.expect("look-crops-to-the-region-it-is-given", size == CROP_SIZE,
           f"{size[0]}x{size[1]} for {CROP}")

was = run.memory("system", FRAME_TICK, 1)[0]
run.step(TICK_FRAMES)
now = run.memory("system", FRAME_TICK, 1)[0]
run.judge("work-ram-ticks-once-a-frame",
          (now - was) % TICK_WRAP == TICK_FRAMES,
          f"0x{FRAME_TICK:04x}: {was} -> {now} over {TICK_FRAMES} frames")
still = run.memory_stable("system", STILL_BLOCK, STILL_BYTES, STILL_WINDOW)
run.judge("still-ram-settles-in-the-window-it-is-given",
          still == STILL_WINDOW,
          f"0x{STILL_BLOCK:04x}: {still} frames for {STILL_WINDOW}")

started = run.frames()
try:
    run.memory_stable("system", FRAME_TICK, 1, STILL_WINDOW, TICK_TIMEOUT)
    refused = ""
except RuntimeError as raised:
    refused = str(raised)
run.judge("a-byte-that-ticks-every-frame-never-settles",
          bool(refused) and run.frames() - started == TICK_TIMEOUT,
          f"{refused or 'it settled'},"
          f" {run.frames() - started} frames for {TICK_TIMEOUT}")

hunt = run.hunt("system", width=2, endian="little")
left = [hunt.step("increased")]        # the first step seeds
for _ in range(HUNT_ROUNDS):
    run.step(1)
    left.append(hunt.step("increased"))
survivors = dict(hunt.candidates(len(hunt)))
run.expect("the-hunt-finds-the-counter-the-profile-watches",
           FRAME_TICK in survivors,
           f"{left[0]} words, then {left[1:]};"
           f" 0x{FRAME_TICK:04x} reads {survivors.get(FRAME_TICK)}")

print(f"frame {seen['frame']} at {seen['time']:.2f}s,"
      f" change {seen['change']:.4f}, settled after {settled} frames,"
      f" shot {shot}")
