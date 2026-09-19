# tash/libretro/

`libretro.h` is third-party, vendored unmodified.
`tools/vendor-libretro-header.py` is what reproduces it.

## libretro.h -- vendored, unmodified

| | |
|---|---|
| what | the libretro API: the core entry points, the environment callback and the video, audio and input contracts `Core` and `Session` drive |
| upstream | [Genesis Plus GX](https://github.com/libretro/Genesis-Plus-GX), `libretro/libretro-common/include/libretro.h` |
| commit | `c2838c7dc4236fc2fe94e5dbd08b41486067918e` (2026-09-12) |
| sha256 | `3e05e66cdfa2ddb8e9ac4eee6bb38ff4d735ed82e8563ef116b710089c732512` |
| bytes | 212 482 |
| licence | MIT, in the file's own header comment (the RetroArch team) |

    tools/vendor-libretro-header.py --verify   # is this file the pin?
    tools/vendor-libretro-header.py            # fetch the pin and write it

The header comes from the core's tree and not from RetroArch's `libretro-common`:
what a core answers is what its own copy declares, and a newer header would
offer environment calls this build of Genesis Plus GX never implements. The pin
is the commit the core package is built from, so the two move together.

The file is never edited -- a newer core is a new `COMMIT` and `SHA256` in the
tool and a re-run. `--verify` hashes what is here against the pin and needs no
network; the `tools-pytest` ctest entry (buildutil.toml `[test] python`) runs
it on every test run.

This directory is a group until a `Core` module joins the header beside
it; buildutil exports the header as
`include/tash/libretro/libretro.h`, so a consumer writes
`#include <tash/libretro/libretro.h>`.
