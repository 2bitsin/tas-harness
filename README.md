# tas-harness

A tool-assisted-speedrun style test harness for games, emulators and other
real-time graphical applications. `tash` plays a target at human pace or
faster, records video, audio, inputs and events, and judges what it sees; a
run leaves a bundle a human can watch and a machine can diff. It is built for
a coding agent to drive — over MCP, over a CLI, or from Python — so that
"does the game still get from the title screen to the first level" is a test
that runs like any other.

A target reaches the harness two ways: as a libretro core the harness hosts
and steps frame by frame, or as an application that links `libtash`
(`sources/tash/linked/README.md`) and reports its own frames, inputs and
events where no harness can attach.

## What a run is made of

- A **profile** (`profile.yaml`) names the core, the ROM, the input ports and
  the memory **watches** — named values read out of the target's RAM.
- A **tape** (`tape.yaml`) is a recorded input sequence cut into segments,
  each of which waits for an **anchor** (an exact frame hash, a perceptual
  hash, a template match in a region, a watch value, an event) before it
  plays. That is what makes a replay survive a timing change instead of
  desynchronising.
- A **scenario** is Python: it plays tapes, steps, observes, searches memory,
  takes checkpoints and restores them, and states **expectations** that
  become **verdicts**.
- A **bundle** is the output directory: `video.mkv`, `trace.bin`, `run.yaml`,
  `verdicts.jsonl`, `report.html` and the screenshots.

`docs/spec.md` is the vocabulary and the requirements, `docs/design.md` how
it is put together, `docs/archetypes.md` three worked differential harnesses.

## Requirements

Linux on x86_64.

- **gcc 16 or newer.** The sources are C++26 and oxbox uses library features
  that libstdc++ 15 and older do not have.
- **Python 3.14 with the embeddable runtime** (headers and
  `libpython3.14.so`): the scenario interpreter is embedded in the binary,
  so CMake's `Development.Embed` component has to be satisfiable.
- **[buildutil](https://github.com/2bitsin/buildutil)**, CMake 3.25 or newer,
  `ninja`, `git`, and a real `clang` on `PATH` — buildutil's reflect
  extension parses with libclang, whose wheel ships no builtin headers.
- **Two conan packages that are not on any public remote**:
  [oxbox](https://github.com/2bitsin/oxbox) and
  [libretro-cores](https://github.com/2bitsin/libretro-cores). Clone each and
  run `./buildutil publish --release --no-upload` in it; that builds it and
  leaves it in the conan cache this project then resolves from. Point both at
  the same cache as this project with `CONAN_HOME`.

Everything else — OpenCV, ffmpeg (x264), libzip, SQLite, xxHash, GoogleTest —
comes from Conan Center, and buildutil builds its own `_pyvenv/` beside the
checkout for conan and pybind11.

## Cartridges

The harness plays ROMs; it ships almost none. `roms/` at the repository root
is where a machine's own cartridge library goes — put it there or symlink it
— and the example profiles name their game under it, for instance
`roms/Genesis/Columns (USA, Europe).zip`. `roms/` is gitignored, because the
cartridges the examples use are commercial software nobody may redistribute.
The tests that need one skip themselves when it is absent.

The one exception is `examples/homebrew/zsenilia.bin`, a 256 KiB Mega Drive
demo under LGPL-3.0 that is committed with its licence beside it, so the
determinism tests have something to play on a machine with no library at all.

## Build and test

    ./buildutil build --release
    ./buildutil test --release

The tests are the C++ suites, the Python suites under `tools/` and two of the
examples, and the conan package test. Everything the harness needs at run
time — the libretro core included — is installed next to the binary under
`_install/`.

## Play something

    ./buildutil run --release -- run \
      --profile examples/homebrew/profile.yaml \
      --scenario examples/homebrew/scenario.py \
      --bundle _runs

`_runs/<timestamp>-zsenilia/` then holds the video, the trace, the verdicts
and `report.html`. `tash replay --bundle <dir>` plays the bundle's tape back
and compares hashes and watches frame for frame; `tash report --bundle <dir>`
re-renders the page.

The other verbs are `serve` (the same run behind an MCP endpoint), `session`
(one tool call against a running `serve`, from a shell), `tape` (read, check
and cut tapes) and `mem` (the memory search, scripted).
`sources/tash/tash/README.md` documents them, `sources/tash/mcp/README.md`
the MCP surface and the protocol revision it follows.

## Driving it from an agent

    tash serve --profile examples/columns/profile.yaml --port 7784

opens `http://127.0.0.1:7784/mcp`, and every MCP tool on it drives the same
run object the Python scenario API drives — a verb is implemented once and
called from both. The tool schemas are generated from the structs the tool
bodies decode their arguments out of, so a schema cannot drift from its
implementation.

## The examples

Each directory under `examples/` is one game worked through end to end: the
profile, the memory hunt that found its watches, the tape from power on, and
a scenario that plays it. They are the best documentation of what the harness
can be asked to do.

| example | game | what it shows |
|---|---|---|
| `homebrew` | Zsenilia (demo, committed) | determinism, tapes, the smallest scenario |
| `columns` | Columns | the memory hunt, a tape to the first column, a model of the well |
| `wwf` | WWF WrestleMania | combos, special moves, a match played to the bell |
| `monopoly` | Monopoly | dice, deeds, a whole game against the computer |
| `themepark` | Theme Park | money, the chart ranking, a park built and sold |
| `dune` | Dune: The Battle for Arrakis | credits, buildings, two missions |
| `caesars` | Caesars Palace | the wallet, every game in the casino |
| `scooby` | Scooby-Doo Mystery | the room graph, the script VM, a route |

## Release notes

**0.1.0 — the first public release.** The harness as it stands: libretro
sessions, tapes with anchors, the Python scenario API, memory watches and
search, perception (hashes, template matching, colour counts, region
stability), planning over grids and graphs, the recorder and its report, the
trace, the MCP server, and `libtash` for targets that link instead of being
hosted.

## Licence

MIT, see [LICENSE](LICENSE). Copyright (c) 2026 Aleksandr Ševčenko.

Two things in the tree are not:

- `examples/homebrew/zsenilia.bin` is *Zsenilia* by Resistance (2015), under
  LGPL-3.0; the licence is at `examples/homebrew/zsenilia-license.txt` and
  the provenance in `examples/homebrew/README.md`. It is committed
  unmodified, and `tools/vendor-homebrew-rom.py` reproduces it.
- `sources/tash/libretro/libretro.h` is the libretro API header, MIT,
  copyright (c) 2010-2020 The RetroArch team, vendored byte for byte from the
  pinned Genesis Plus GX commit by `tools/vendor-libretro-header.py`.

The Genesis Plus GX core the harness loads is built by `libretro-cores` and
carries the Genesis Plus GX licence, which forbids selling a redistribution
or using one commercially. The cartridges the examples play are not
distributed here at all.
