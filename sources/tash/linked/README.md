# tash/linked/

libtash: the library a target links so the harness can drive it, record it,
or not be there at all. `tash.hpp` is the whole api -- one C++ header, the
same conventions as the rest of the tree, and a `Harness` that closes
itself. A target linking this links nothing else of tash: the component is
`tash::linked` and the only package under it is xxhash, the digest a frame
is hashed with.

    tash::linked::Harness tash{ tash::linked::Options{
      .target = "scoobvm", .fps = 60.0 } };

    tash.Register("room", &room, tash::linked::Number::U8);
    for (;;)
    {
      tash::linked::Input input{ };
      if (!tash.Begin(input))
      {
        input = SampleTheDevices();
        tash.Submit(input);
      }

      StepTheGame(input);
      tash.Submit(frame);
      tash.Submit(chunk);
    }

`Frame(input)` is those five lines in one: it begins the frame and submits
the input the target sampled when the harness did not supply one.

## Three states

| state | when | what the calls do |
|---|---|---|
| attached | the harness is on the other end of `TASH_SESSION` | `Begin` blocks until the harness releases the frame and hands over the input; video and audio go into the bus; watches and events go over the control channel. **Not in this build** -- the constructor says so through the note and leaves the handle empty |
| file | a directory (`TASH_RECORD`, or `directory` in the options) or `Sink::MEMORY` | nothing is driven; what the target hands over is written down: the inputs as `tape.yaml`, the frames, watches and events as `trace.bin`, the run as `run.yaml` |
| none | neither | every call is a branch on a null pointer and nothing else |

The third state is a `Harness` holding nothing, and every call in the
header is an inline test of that one pointer before the out-of-line half.
A frame of an unattached target -- `Frame`, then video, then audio -- is
one compare and one branch for the lot (gcc 16.1, `-O2`):

    0:  cmpq $0x0,(%rdi)
    4:  je   80
    ...
    80: ret

That is the reason the handle is a pimpl and not an interface: a virtual
call cannot be elided by a branch the caller can see through.

## The calls

| call | attached | file | none |
|---|---|---|---|
| `Harness{ Options }` | connects, or notes why it cannot | opens the directory or the buffers | holds nothing |
| `~Harness` | disconnects | closes the frame in progress, writes the artifacts | nothing |
| `Mode` | `Mode::ATTACHED` | `Mode::FILE` | `Mode::NONE` |
| `Attached` | true | true | false |
| `Begin(Input&)` | blocks, fills the input, answers true | ends the frame before it and starts this one, answers false | answers false |
| `Frame(Input&)` | `Begin`, and nothing more | `Begin`, then submits what the target sampled | nothing |
| `Submit(Input)` | nothing: the harness supplied it | the buttons that moved become transitions, the pointer that moved a move | nothing |
| `Submit(Video)` | into the frame ring | hashed, and the digest goes into the frame's record | nothing |
| `Submit(Audio)` | into the audio ring | dropped: no format a recording writes holds a sample, and the encoder is the harness's | nothing |
| `Register(Watch)` | registers in the watch table | sampled every frame into a watch record; the names go into `run.yaml` | answers nothing |
| `Register(State)` | the harness calls these at a checkpoint | ignored: a recording never goes back | nothing |
| `Event(name, text)` | onto the event stream | a trigger record on the frame it was emitted in | nothing |
| `Dt`, `Seed` | the harness's, for a D3 target | the target's own, written down once as a decision record | the target's own |
| `Flush` | nothing | the frame in progress is ended and every artifact is whole | nothing |
| `Bytes(Artifact)` | empty | the bytes of a memory sink; empty for a directory, which has them on disk | empty |

`Input` is the whole of the devices for one frame -- two pads with sixteen
buttons and four axes each, two pointers with a position and five buttons,
and 256 keys as a bitmap -- and not the events that changed them, so a
target reading it does not depend on the order they arrived in (design
4.1). `Bit(Button::START)` and `Bit(MouseButton::LEFT)` are the bits of
`Pad::buttons` and `Mouse::buttons`.

A `Watch` is a name, an address and a `Number` width, or a name and a
`reader` that yields the value when the number is not somewhere an address
can say. The trace records watches by index; `run.yaml` carries the names
in that order.

The three hooks a target hands over -- the watch `reader`, the `note` a
refusal goes to, and `State`'s `save` and `load` -- are `std::function`
rather than a function reference or a template parameter: the library
stores them past the call that registered them, a target's hook is usually
a lambda over its own state, and none of them runs in the frame's hot path
(the note fires at most once per kind, the reader once a frame per watch,
the state hooks only when a checkpoint calls). A target with no free store
at all passes none of them and registers watches by address.

Nothing here throws into a target and nothing here writes to a stream of
the target's: a refusal goes to the `note` the options carry, and a
recording that runs out of memory stops growing while the target plays on.
None of it is thread safe: the calls come from the frame's own thread.

## What file mode writes

    <directory>/tape.yaml    the inputs, frame base, one segment
    <directory>/trace.bin    a frame record per frame, the watches, the events
    <directory>/run.yaml     the manifest, so the directory is a bundle

`tash tape check <dir>/tape.yaml` reads the first, `tash trace dump
<dir>/trace.bin` the second, and `tash report` or any other reader of a
bundle opens the directory as it stands. The formats are the ones the
harness already writes: `tash/tape/README.md` and `tash/trace/README.md`.

- **The tape** is one segment named `start`, as long as the run. A button
  that moved is a transition, a pointer that moved is a line in the
  `pointer` block (grilling 17). A key that moved is neither -- the tape has
  no key channels -- and a target that holds one is told so once, through
  the note.
- **The trace** holds the exact hash of every frame submitted: xxh3 over
  each row's visible bytes, which is the digest `perception::ExactHash`
  computes for the same picture, so a hash out of a recording and a hash out
  of a harness run are the same number. The two perceptual hashes and the
  change amount are zero: they want a downscaled grey copy, which is opencv,
  which a target linking this does not get. There are no input records --
  the tape is where a recording's input lives.
- **The manifest** carries the target's name, the frames, the watch names
  and `determinism: D1`: the library watches, it does not step, and the dt
  the target used is the target's own.

Both the tape and the manifest are written again from scratch every 600
frames and at every `Flush`, so a target that stops dead leaves the last
whole pair of them behind; the trace is append-only and flushed every sixty
records.

## The memory sink

A host with no filesystem -- a browser tab -- opens with `Sink::MEMORY`,
plays, calls `Flush` and fetches the same bytes the directory would have
held:

    tash.Flush();
    SaveItSomewhere(tash.Bytes(tash::linked::Artifact::TAPE));

The span points into the recording and lives until the next call that grows
it, and until the `Harness` goes. Fetch before it does: a closed handle's
bytes are gone.

## What attached mode will add

The header is the whole api and attached mode adds no call to it. What it
adds is behaviour behind the same names: the constructor connects to the
socket `TASH_SESSION` names and the handle is in `Mode::ATTACHED`; `Begin`
blocks there until the harness releases the frame, fills the input from the
frame's slot and answers true; video and audio go into the shared-memory
rings instead of being hashed and dropped; watches are registered over the
control channel and written into the watch table each frame; events go onto
the event stream; `Dt` and `Seed` answer the harness's numbers rather than
the target's; and the `State` hooks are what a checkpoint calls. The note
is where a version or a socket that will not answer is reported. No target
has to be recompiled for any of it.

## Shipping it to a consumer

The module is a conan component of the tash package already: a release
build writes `share/buildutil/buildutil-components.json`, and the entry
for this one is what a consumer wants to see -- no module of tash's, one
package under it.

    {"path": "tash/linked", "lib": "linked", "needs": [],
     "external": ["xxHash::xxhash"]}

`test_package/` at the repo root is the consumer: it asks for
`tash::linked` and nothing else, compiles against the shipped header,
links the shipped archive and runs. What a consumer's own project
says is two lines --

    Require(tash VERSION "0.0.1" CONAN tash COMPONENTS linked)
    Link_dependencies(tash::linked)

-- the first in its `sources/CMakeLists.txt`, the second in the module
that links libtash.

It does not run yet, and `buildutil.toml` carries no `[package]` section
for that reason. With one, `buildutil publish --no-upload --version 0.0.2`
builds and packages the tree -- 0.0.1 is on the site remote already, and a
version that is on the remote is never published again -- and then conan
refuses the recipe twice over:

    ERROR: tash/0.0.1: package_info(): There are
    '(cpp_info/components).requires' that includes package 'xxHash::',
    but such package is not a a direct requirement of the recipe

    ERROR: tash/0.0.1: package_info(): The direct dependency 'opencv' is
    not used by any '(cpp_info/components).requires'.

The manifest records the CMAKE target a module links (`xxHash::xxhash`,
`opencv_core`), and conan wants the package the target came from
(`xxhash`, `opencv`) -- a mapping the recipe's own `Require()` parse
already carries for the first spelling and cannot derive for the second.
And conan makes every direct requirement belong to some component, which
a manifest of cmake targets cannot satisfy. Neither is this module's to
fix; both are buildutil's.

Past them there is a third thing, which no component can fix: conan
requires are package-level, so a consumer that asks for tash resolves
ffmpeg, opencv, sqlite and the rest whether or not it links them. A
target that wants libtash and nothing else in its graph wants libtash as
a package of its own.

## Inside

One class per file, and none of them public: `_byte-sink` is a file or a
buffer, `_trace-writer` the records, `_tape-writer` the lines, and
`_recording` the frame the target is in. `harness.cpp` is the out-of-line
half of the header and nothing else. The module links no library of
tash's; the headers it includes from the rest of the tree --
`trace/format.hpp`, `tape/names.hpp`, `clock/determinism.hpp`,
`utilities/outcome.hpp` and `utilities/version.hpp` -- are declarations of
the formats it writes and carry no symbols. What keeps the two sides from
drifting is the test: `file-mode.test.cpp` reads everything back with
`tape::TapeFrom`, `trace::Reader` and `recorder::Bundle`, which are the
readers the harness itself uses.
