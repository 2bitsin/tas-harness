# examples/homebrew/

The Genesis ROM the tests run when no cartridge library is present,
vendored unmodified. `tools/vendor-homebrew-rom.py` is what reproduces it.

## zsenilia.bin -- vendored, unmodified

| | |
|---|---|
| what | *Zsenilia*, a 256 KiB Mega Drive demoscene intro (Resistance, 2015): code Fra, 2D art Grass, music Nainain, text 4Play ([pouet](https://www.pouet.net/prod.php?which=66742)) |
| upstream | [ResistanceVault/demo-Zsenilia](https://github.com/ResistanceVault/demo-Zsenilia), `release/ZSENILIA-BY-RSE-2015.bin` |
| commit | `9e035b7d5361f308bdfe8809cbc5068f3221a821` (2016-01-08) |
| sha256 | `5c4fa4d3b16fe94c9feb707e3cbb3ff6de04380c277dfa6155419228f887c988` |
| bytes | 262 144 |
| licence | LGPL-3.0, `zsenilia-license.txt` beside it, the repository's own copy; its source is the same repository at that commit |

    tools/vendor-homebrew-rom.py --verify   # is this tree the pin?
    tools/vendor-homebrew-rom.py            # fetch the pin and write it

A demo rather than a game: it animates from its first frames without input,
so `session-tests` can prove that two runs of 600 frames hash the same
without also proving that a title screen holds still. Commercial ROMs cannot
be committed, and the Columns tests skip themselves when `roms/` does
not hold the cartridge; this one makes the determinism tests CI's.

The files are never edited -- a different demo is a new `COMMIT`, `MEMBER`
and `SHA256` in the tool and a re-run.

## tapes/demo.yaml -- what the tape tests replay

The demo takes no input, so the tape proves the path and not the game: the
first segment waits for the first frame by its exact hash and presses start,
the second waits for the logo by template match and holds right. The crop in
`demo.anchors/logo.png` is the logo's bounding box (x 64, y 83, 192x28) cut
out of the frame `tash run --profile examples/homebrew/profile.yaml --frames
120 --shot` leaves behind, and `tash trace dump` is where the hash came from.

    tash tape check examples/homebrew/tapes/demo.yaml

## profile.yaml -- the `beat` watch

The demo counts its own frames at work ram `0x068e`, a 16-bit counter that
reads little-endian because Genesis Plus GX keeps work ram byte-swapped. It
was narrowed to that one address by

    tash mem search --profile examples/homebrew/profile.yaml --width 2 \
      --endian little --steps "run 120; snapshot; run 1; increased; \
      run 1; increased; run 1; increased; run 1; increased; list 30"

It does not track the frame count exactly -- the demo holds it still over
some transitions -- but it never goes backwards, so it measures how far a
run got.

## scenario.py -- what a python scenario looks like

The cli opens the session and the scenario drives it: play `tapes/demo.yaml`,
step past it, observe, take a look into the bundle, then expect the tape's
own counts and judge the frame by the exact hash the run settles on. It marks
both halves, so the trace reads as the scenario does.

Then it reads work ram: the byte at `0x068e` of the system region is a
counter the demo advances once a frame, so ten frames later it reads ten
higher, and that is the third verdict. The address came out of `memory()`
too -- dump the whole region, step ten frames, dump again, step ten more,
dump again, and keep the bytes that moved by exactly ten both times. One
byte of 65 536 does, and Genesis Plus GX hands that ram byte-swapped, so
`0x068e` here is the 68000's `0xff068f`.

Then it waits on a block twice. `memory_stable` over 256 bytes at `0x1c14`,
which the demo never writes, answers exactly the window it was given, because
the block was already still when it was first read; over the ticking byte it
never settles, and the refusal names the region, the address and the frames
it stepped. Those are the fourth and fifth verdicts, and together they pin
what the call promises: it steps until the block holds still, and it stops
rather than running forever.

Last it hunts for the tick the profile watches, which is the hunt at the top
of this file done from the session rather than from the command line:
`run.hunt("system", width=2, endian="little")`, one step to seed the 32 768
words of work ram and three more a frame apart that a candidate must have
risen across. Around forty words rise every frame at that point in the demo
and `0x068e` is one of them, which is the sixth verdict.

`scenario-runner.test.cpp` plays this file and asserts all six verdicts
pass, which is where CI reads the guest's memory.

    tash run --profile examples/homebrew/profile.yaml \
             --scenario examples/homebrew/scenario.py --bundle /tmp/runs

## search.py -- what a tash.search run looks like

Five input strings, each a run of taps; the search restores the same state
for each, plays it, and scores the `beat` watch, so the string that carries
the run furthest wins. The demo ignores the pad, which is the point of the
homebrew here: the search is proved, not the game. Every trial is marked in
the `trial` group, so the report's time line collapses the five into one
row, and the scenario reads its own report back to say so.

    tash run --profile examples/homebrew/profile.yaml \
             --scenario examples/homebrew/search.py --bundle /tmp/runs
