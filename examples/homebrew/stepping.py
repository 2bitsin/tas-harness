"""Step in blocks and print each one: what a detached python job looks like."""

import tash

BLOCK_FRAMES = 100
BLOCKS = 6

run = tash.run

for block in range(BLOCKS):
    run.step(BLOCK_FRAMES)
    print(f"stepped {(block + 1) * BLOCK_FRAMES} frames,"
          f" now at {run.frames()}")

run.judge("stepped-every-block", run.frames() >= BLOCK_FRAMES * BLOCKS,
          f"frame {run.frames()}")
print("stepping is done")
