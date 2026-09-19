# Scooby-Doo Mystery (Genesis): the script VM and its data

This document specifies the adventure interpreter inside *Scooby-Doo
Mystery* (USA, Genesis, 2 MiB, md5 `f27ea503631e67c05860926099d97f7a`)
well enough to write a replacement for it. Everything here was derived
from the cartridge image; every claim carries a ROM address, a count, or
the handler instructions that prove it.

**Byte order.** The 68000 is big-endian and so is the cartridge. Every
"word" below is a 16-bit big-endian value and every "long" a 32-bit
big-endian value, read at the ROM address given. Work RAM addresses are
written as the 68000 sees them (`0xff0000`..`0xffffff`); a memory
snooper that mirrors the Genesis Plus GX work-RAM region sees each byte
at `address xor 1`, because the core stores that region byte-swapped.

**How the evidence was obtained.** The handlers were disassembled with
the `capstone` disassembler (`CS_ARCH_M68K` / `CS_MODE_M68K_000`), which
is not on the box by default: it was `pip install`-ed into a virtualenv
under the scratch directory, not into the repo. Script bodies were
walked by a small Python program, also under the scratch directory. The
emulator was not used; nothing here needs it. Where a claim comes from
walking the shipped scripts rather than from a handler, it says so and
gives the count.

Two shorthands are used throughout:

* **object word** — the number a script uses to name an object. It is
  the index into the object table plus 3.
* **scenario block** — the 13-long header that names every table for one
  of the game's two scenarios (below).

---

## 1. The cartridge layout

### 1.1 The scenario block

The game ships two self-contained scenarios: *Blake's Hotel* and *Ha Ha
Carnival*. Which one is live is chosen by the word at `0xff06aa`, used
as an index into the table of longs at ROM **`0x31af2`**; the chosen
long is the scenario block's address and is cached in **`0xff068e`**
(`0x7f66`: `move.w 0xff06aa,d0; lea 0x31af2,a0; lsl.w #2,d0;
movea.l (a0,d0.w),a0; move.l a0,0xff068e`).

| index | scenario | block |
|---|---|---|
| 0 | Blake's Hotel | `0xfca0c` |
| 1 | Ha Ha Carnival | `0xfca40` |

Each block is 13 longs. Every other address in this document is reached
through it, so an interpreter needs no other absolute constant except
`0x31af2` and the dispatch table.

| offset | what it points at | hotel | carnival |
|---|---|---|---|
| +0x00 | flag seed bytes | `0xfca74` | `0x15059a` |
| +0x04 | object seed records | `0xfca86` | `0x1505a8` |
| +0x08 | room table | `0xfd4b0` | `0x150e96` |
| +0x0c | room picture base | `0xfd654` | `0x1510ee` |
| +0x10 | room geometry base | `0x12e986` | `0x1952ee` |
| +0x14 | (not decoded) | `0x1393d2` | `0x1a4b76` |
| +0x18 | actor/sprite table, stride 0x182 (read at `0x5d7e`) | `0x139e52` | `0x1a5a76` |
| +0x1c | walk-box base | `0x13e9b6` | `0x1a8934` |
| +0x20 | packed string block | `0x14199c` | `0x1af33c` |
| +0x24 | walk-box index (longs) | `0x145eb8` | `0x1b3508` |
| +0x28 | **object table** | `0x146204` | `0x1b3860` |
| +0x2c | **script base** | `0x1467ac` | `0x1b3d38` |
| +0x30 | scenario header / boot script | `0x14f5dc` | `0x1bad9c` |

Each region ends where the next one begins, which is how the counts
below are obtained.

### 1.2 The object table, and the "one entry back" script pointer

The object table is at **block +0x28**, and an entry is **8 bytes**:

```
+0  long  script offset, relative to the script base (block +0x2c)
+4  long  name offset, relative to the string block (block +0x20)
```

Entry *i* holds object word *i + 3*, i.e.

```
entry(w)  = objects + (w - 3) * 8
script(w) = script_base + long(entry(w) + 0)
name(w)   = string_base + long(entry(w) + 4)
```

Counts: hotel `0x146204..0x1467ac` = 1448 bytes = **181 entries**
(words 3..183); carnival `0x1b3860..0x1b3d38` = 1240 bytes = **155
entries** (words 3..157). Both match the number of object seed records
exactly (§2.2).

**Proof of the pointer, and why it looked "one entry back".** The action
driver at `0x19b0` does:

```
0019be  movea.l 0xff068e,a0          ; scenario block
0019c4  movea.l 0x2c(a0),a6          ; a6 = script base
0019c8  movea.l 0x28(a0),a0          ; a0 = object table
0019cc  move.w  0xff06ae,d0
0019d2  subi.w  #3,d0                ; d0 = w - 3
0019f8  lsl.w   #3,d0                ; * 8
0019fa  adda.l  (a0,d0.w),a6         ; + long at entry+0
```

Earlier reconnaissance had guessed the table base at `0x1461f0` and
therefore read a name at `base + w*8` and a script at `base + w*8 - 4`,
"one entry back from the name". The two formulations are arithmetically
identical (`0x1461f0 + 8w = 0x146204 + 8(w-3) + 4`); the real base is
`0x146204` and the script pointer is simply the *first* long of the
entry, with the name second. Nothing is one entry back.

### 1.3 Scripts

All object scripts, all room scripts and the boot script live in one
contiguous area starting at the script base. For the hotel that area
runs **`0x1467ac`..`0x14fcbe`**; `0x14f5dc` (block +0x30) is where the
boot script starts, not where the area ends.

An **object script** has a six-byte header:

```
+0  word  length of the code that follows the header, in bytes
+2  word  gender/number key, copied to 0xff06b0 (see 3.7)
+4  word  not read (the driver does `tst.w (a5)+`)
+6        the code
```

The driver at `0x1a00` reads it as

```
001a00  move.w  (a5)+,d0             ; length
001a02  move.w  (a5)+,0xff06b0       ; gender key
001a08  tst.w   (a5)+                ; third word skipped
001a0a  lea     (a6,d0.w),a6         ; a6 = script_start + length
```

so the end pointer `a6` is `start + length`, while the code begins at
`start + 6`. Every interpreter loop tests `a5 < a6`, so an instruction
that *starts* before `start + length` runs to completion even though it
ends after it. In practice the code region is exactly
`[start+6, start+6+length)`: walking all 181 hotel scripts and all 155
carnival scripts with that convention decodes every byte, leaves no
remainder, and every script's last top-level instruction begins before
`start + length`.

**Coverage check.** Taking `[start, start+6+length)` for every object
script, `[start, start+4+length)` for every room script (§1.4) and the
boot script, the hotel's area is covered by **one contiguous run** from
`0x1469b8` to `0x14fcbe` with a single 524-byte hole at
`0x1467ac..0x1469b8`; the carnival's is one run from `0x1b3d3c` with a
4-byte hole at the start. The hole at `0x1467ac` is not VM code: it
contains `ff fd`/`ff ff` markers and pairs of coordinates, i.e. the same
shape as the animation streams carried inside opcode 33 (§4). It is not
reachable through any object, room or boot pointer.

A **room script** has a four-byte header — length word, one unused word
— and its code is `[start+4, start+4+length)`. Proof at `0x2250`:
`move.w (a5)+,d0; tst.w (a5)+; lea (a5,d0.w),a6`.

### 1.4 The room table

The room table is at **block +0x08**; a record is **20 bytes** and room
*r* is at `rooms + (r - 2) * 20`. Hotel: `0xfd4b0..0xfd654` = 420 bytes
= **21 rooms**, numbered 2..22. Carnival: `0x150e96..0x1510ee` = 600
bytes = **30 rooms**, numbered 2..31.

| offset | meaning | proof |
|---|---|---|
| +0x00 | long: name offset into the string block | `0x77e4`: `move.l (a3),d0; add.l 0x20(a5),d0; move.l d0,0xff06d2` |
| +0x04 | long: offset of the room **picture**, added to block +0x0c | `0x7636`: `movea.l 4(a3),a0; adda.l 0xc(a5),a0; tst.l (a0)+; lea 0xff3000,a1; lea 0xff4000,a2; bsr 0x97aa` |
| +0x08 | long: offset of the room **geometry** record, added to block +0x10 | `0x77f0`: `movea.l 8(a3),a4; adda.l 0x10(a5),a4` |
| +0x0c | long: **room-entry script** offset, from the script base | `0x2246`: `move.l 0xc(a1,d0.w),d0; movea.l 0x2c(a0),a5; adda.l d0,a5` |
| +0x10 | long: **per-frame room script** offset | `0x22a4`: `move.l 0x10(a1,d0.w),d0` (same shape) |

The geometry record at `block+0x10 + room[+8]` begins with two words
copied to `0xff06c6` and `0xff06c8` (the room's scroll extent), then a
long at +4 that is an offset to the room's **hit mask**, decompressed to
`0xff8c00` (`0x7806`: `lea 8(a4),a0; movea.l a0,a5; adda.l -4(a0),a0;
tst.l (a0)+; lea 0xff8c00,a2; bsr 0x97aa`). The compression used by
`0x97aa` is not decoded.

Both room-record script offsets are counted in the coverage check above:
41 hotel room scripts (one room has a zero entry-script offset) and 77
carnival ones, all of which walk cleanly.

### 1.5 Walk boxes

A box number *b* (1-based) is resolved as

```
record = block[+0x1c] + long(block[+0x24] + (b - 1) * 4)
```

proven at `0x4ce4` in opcode 15's handler (`movea.l 0xff068e,a1;
movea.l 0x24(a1),a2; lsl.w #2,d2; movea.l (a2,d2.w),a2;
adda.l 0x1c(a1),a2`) and again in the per-frame overlap test at
`0x12cc`. The index is `block[+0x24]..block[+0x28]`: **211 longs** for
the hotel, **214** for the carnival.

The first four words of a box record are a rectangle in eight-pixel
cells, compared against Shaggy's cell in the overlap test at `0x1310`
and against the crosshair cell in the hit test at `0x1018`; words at
+0x08 and +0x0a are an anchor point added to a target position by
opcode 15 (`0x4cf8`). The rest of the record is not decoded.

### 1.6 The dispatch table

**34 longs at ROM `0x2354`**, indexed by the opcode word:

```
002354 + op*4  ->  handler address
```

The scan and execute loops both do `move.w (a5),d0; lsl.w #2,d0;
lea 0x2354,a0; movea.l (a0,d0.w),a0; ... jsr (a0)` (`0x23e2` and
`0x240c`). The 34 addresses are listed in §4. Opcodes 14 and 27 share
one handler; 18 and 20 are two byte-identical routines.

### 1.7 String block and fixed text

Strings are NUL-terminated 8-bit text at `block[+0x20] + offset`. The
hotel's block is `0x14199c..0x145eb8` (17692 bytes), the carnival's
`0x1af33c..0x1b3508` (16844 bytes). Opcode 16 proves the addressing:
its handler builds `a0 = long(block+0x20) + long(a5+4)` and passes it to
the line drawer at `0xa130`.

Text that is not per-scenario lives in the program:

| address | content |
|---|---|
| `0x2fe40` | eight words `3 7 9 6 8 4 1 5` — the verb-bar layout |
| `0x2fe50` | `"Shaggy\0Scooby\0"` |
| `0x2fe5e` | `"Pull \0Take \0Open \0Shut \0Use \0Give \0Push \0Talk to \0Eat \0Look at \0"` — verbs 1..10 in order |
| `0x2fea0` | ten longs: the default refusal line per verb (§3.7) |
| `0x2fecc`..`0x2ffa4` | the ten refusal strings themselves |
| `0x2ffbe` | `"It ain't gonna happen, dude."` |
| `0x2ffdb`, `0x2ffe0`, `0x2ffe4` | `"that"`, `"him"`, `"her"` |
| `0x2ffe8`, `0x2ffef` | `" with "`, `" to "` |

---

## 2. The machine state

The interpreter owns three things: the flag array, the object records,
and about twenty scalar words. Everything else in work RAM belongs to
the renderer, the sound driver or the input layer, and a fresh
implementation can invent its own. The table at the end of this section
says which is which.

### 2.1 The flag array at `0xff2a00`

**256 bytes**, cleared at boot and then seeded from ROM:

```
007f34  lea     0xff2a00,a1
007f3a  moveq   #0,d0
007f3c  move.w  #0x7f,d1
007f40  move.w  d0,(a1)+          ; 0x80 words = 256 bytes
007f42  dbra    d1,0x7f40
007f46  movea.l 0(a0),a1          ; a0 = scenario block, so a1 = block[+0x00]
007f4a  lea     0xff2a00,a2
007f50  move.b  (a1)+,(a2)+
007f52  cmpa.l  4(a0),a1          ; until a1 reaches block[+0x04]
007f56  bne     0x7f50
```

so the seed's length is `block[+0x04] - block[+0x00]`: **18 bytes** for
the hotel (`01 00 00 00 00 00 00 00 00 38 00 40 02 82 ff 01 00 00`) and
**14** for the carnival (`01 00 00 00 00 00 10 00 06 80 0c 00 00 00`).

**A flag is one bit of one byte.** A script operand pair *(index, bit)*
addresses `bit` of the byte at `0xff2a00 + index`. The proof is the
condition evaluator:

```
0027b2  move.w  6(a5),d3          ; index
0027b6  lea     0xff2a00,a0
0027bc  move.w  8(a5),d4          ; bit
0027c2  btst    d4,(a0,d3.w)      ; memory destination => byte operation
```

`BTST Dn,<memory>` on the 68000 is byte-sized and the bit number is
taken modulo 8; the shipped scripts never use a bit above 7 and never an
index above 16 (census over all 336 object scripts, all room scripts and
both boot scripts: indices 0..16, bits 0..7). An interpreter that models
the array as 17 bytes is enough for the shipped data; 256 is what the
ROM clears.

Flag 0 is engine-owned. Bit 1 is the "this cut-scene has already played"
latch (tested and set by built-in 5 at `0x45a4`, cleared at `0x48aa`,
tested by the room loader at `0x2168`); bit 2 is written by opcode 28
and bit 3 by opcode 29 (§4); bit 4 is read by the walker at `0x501a`
and `0x56d0`. Flags 1..16 are the adventure's own puzzle state.

### 2.2 The object records at `0xff1200`

One record per object, **stride 0x1a**, in object-word order starting at
word 3. The number of records is in `0xff06f4`.

They are built at boot (`0x7f92`..`0x7fe2`) from the seed table at
block +0x04. A seed entry is **14 bytes**, plus 4 more when the byte at
its +12 has bit 1 set (an actor). The copy is:

```
007f92  movea.l a2,a3
007f94  move.w  #0x19,d0
007f98  clr.b   (a3)+             ; 26 bytes of zero
007f9a  dbra    d0,0x7f98
007f9e  move.l  (a1)+,(a2)        ; seed +0  -> record +0x00
007fa0  move.l  (a1)+,4(a2)       ; seed +4  -> record +0x04
007fa4  move.w  (a1)+,8(a2)       ; seed +8  -> record +0x08
007fa8  move.w  (a1)+,0x16(a2)    ; seed +10 -> record +0x16
007fac  move.w  (a1)+,0x18(a2)    ; seed +12 -> record +0x18
007fb0  clr.w   0xa(a2)
007fb4  move.b  #0xff,0x19(a2)
007fba  btst    #1,0x18(a2)       ; actor?
007fc0  beq     0x7fc8
007fc4  move.l  (a1)+,0x12(a2)    ; actor start position
007fc8  cmpi.w  #1,6(a2)          ; room 1 == carried
007fce  bne     0x7fd6
007fd0  move.w  d1,d0
007fd2  addq.w  #1,d0
007fd4  move.w  d0,(a4)+          ; append to the inventory list
007fd6  addq.w  #1,d1
007fd8  adda.l  #0x1a,a2
007fde  cmpa.l  8(a0),a1          ; until a1 reaches the room table
```

Note that `btst #1,0x18(a2)` is a **byte** test on the high half of the
word at +0x18, i.e. bit 9 of that word. Walked with that stride, the
hotel seed table ends exactly on the room table after **181** records
and the carnival's after **155** — the same counts as the object tables,
which is the check that the stride is right. The hotel has 17 actors
(objects 3, 4, 5 *Ancient Chieftan*, 6 *Bellhop*, 13 *Fred*, 14 *Velma*,
15 *Daphne*, 16, 17, 18, 19, 26 *Uncle Blake*, 29 *Mine Car*, 40 *Fire*,
125 *The Cook*, 138 *Scooby*, 177 *Bear*), the carnival 29.

The fields a script or a handler touches:

| offset | size | meaning | who proves it |
|---|---|---|---|
| +0x00 | word | the object's current walk box / displayed cel; `0xc000` OR-ed in means "leave the room" | opcode 11 handler `0x2e14`, `0x2e3a`, `0x2e52` |
| +0x02 | word | the walk box the object is moving to | opcode 11 `0x2e2a`; read by the crosshair hit test `0x101c` and the overlap test `0x12f4` |
| +0x04 | word | the object's graphic: `value - 1` indexes the sprite table at `block[+0x18]`, stride 0x182 (`0x5d90`..`0x5d9a`). Writing it while the object is carried redraws the inventory panel. | opcode 10 `0x2dd4`..`0x2de8`, `0x5d90` |
| +0x06 | word | **room**. 1 means "carried". | `0x19e2`, `0x2e1e`, `0x2e30`, `0xb010`; the condition evaluator's `obj.field6` |
| +0x08 | word | state; also the value opcode 15 copies into the plane-set words | `0x4d2c`, `0x4d58` |
| +0x0a | word | cleared at seed; used as script scratch | `0x7fb0` |
| +0x0c, +0x0e, +0x10 | word | script scratch (the most written fields in the game: 449, 159 and 55 reads or writes across both scenarios) | opcode 3/4/8 operands |
| +0x12, +0x14 | word | actor x, y (seeded only for actors) | `0x7fc4`, `0x4cd2`, `0x4cd6` |
| +0x16 | byte | default verb when the object is clicked with no verb chosen | opcode 9 `0x2d9a`; the click path `0x12a2` |
| +0x17 | byte | second default-verb slot (opcode 9's signed slot byte selects it) | opcode 9 `0x2d74`..`0x2d7c` |
| +0x18 | word | flags. Byte at +0x18 (the high half): bit 0 = "needs a redraw", bit 1 = actor, bit 2 = "run my wake block" (§4, opcode 19). | `0x2e62`, `0x7fba`, `0xb006` |
| +0x19 | byte | animation slot, seeded to `0xff` | `0x7fb4`, `0x4966`, `0x253c` |

Two object words are reserved and have no record: a condition operand of
object **1** with field offset **6** is the *room word* rather than a
record read (`0x27e6`: `cmpi.w #1,d4; bne; cmpi.w #6,d3; bne;
move.w 0xff06ac,d1`), and object 2 is never used by the shipped scripts.
For every other object the evaluator does `subq.w #3,d4;
mulu.w #0x1a,d4` (`0x27f8`).

### 2.3 The scalars

| address | size | what | interpreter or renderer |
|---|---|---|---|
| `0xff06ac` | word | **the current room** | interpreter |
| `0xff06ae` | word | the object under the crosshair, and the object the action driver runs | interpreter |
| `0xff06b4` | word | the hovered object as published to the UI last frame (`0xff06ae` copied at `0x21ca` and `0x21f2`) | renderer |
| `0xff06b6` | word | the **second** object of a two-object command; 0 when there is none | interpreter |
| `0xff06bc` | word | `0xff06b6` published to the UI (`0x2206`) | renderer |
| `0xff06b8` | word | the verb shown on the status line (`0xff06c0` published at `0x21fc`) | renderer |
| `0xff06be` | word | **the verb being run** (1..11) | interpreter |
| `0xff06c0` | word | the verb the player has selected on the bar | input |
| `0xff06c2`, `0xff06c4` | word | the pending default verb for the hovered object | input |
| `0xff06b0` | word | the current script's gender/number key (§3.7) | interpreter |
| `0xff06dc`, `0xff06de` | word | crosshair x, y in screen pixels | renderer |
| `0xff06ca`, `0xff06ce` | word | camera x, y, added to the crosshair before the cell divide (`0xec0`) | renderer |
| `0xff04dc`, `0xff04f4` | word | Shaggy's x and y | interpreter (scripts read and write them) |
| `0xff04e0`, `0xff04f8` | word | Scooby's x and y | interpreter |
| `0xff0ac9` | byte | **the VM's mode and match byte** (§3.3) | interpreter |
| `0xff0aca` | byte | bits 0 and 1 gate the execute loop; bit 3 requests a follow-up "Talk to" | interpreter |
| `0xff0878` | long | **the nested runner** — the address of whichever loop is live (§3.6) | interpreter |
| `0xff0a1e` | words | **the inventory**, `0xffff`-terminated, each entry `object word - 3` | interpreter |
| `0xff06ec` | word | inventory panel page = count >> 2 | renderer |
| `0xff06f4` | word | number of object records | interpreter |
| `0xff068e` | long | the live scenario block | interpreter |
| `0xff080e` | word | number of dialogue choices gathered this pass (max 5) | interpreter |
| `0xff0824`, `0xff082e`, `0xff0838`, `0xff084c`, `0xff0860` | arrays of 5 | the gathered dialogue choices (§4, opcode 2) | interpreter |
| `0xff09dc`, `0xff09de`, `0xff09e8`, `0xff09eb`, `0xff0ab5`..`0xff0ac3`, `0xff0acd` | byte | renderer and input latches poked by opcodes 22, 23, 25, 31, 32 and the built-ins | renderer |
| `0xff08bc` | bytes | the assembled status line | renderer |
| `0xff087c` | 5x5 words | opcode 26's cycling slots | renderer |
| `0xff05f4` | 5 longs | opcode 33's animation stream pointers | renderer |
| `0xff0428` | long | the random seed (LCG at `0x22d6`) | interpreter |

**The inventory.** Opcode 5's handler maintains the word list at
`0xff0a1e`: it removes an entry when an object leaves room 1
(`0x2a4a`..`0x2a62`) and appends when it enters (`0x2a70`..`0x2a8e`),
keeping the `0xffff` terminator, and recomputes `0xff06ec = count >> 2`.
The list is seeded at `0x7fd0` with every object whose seed room is 1.
Entries are `object word - 3`.

---

## 3. The execution model

### 3.1 The two loops, and how a handler knows which pass it is in

There are exactly two interpreter loops, and both walk a script linearly
from `a5` to `a6`, dispatching on the opcode word.

```
; 0x23dc  SCAN
0023dc  cmpa.l  a5,a6
0023de  ble     0x2404            ; while a6 > a5
0023e2  move.w  (a5),d0
0023e4  lsl.w   #2,d0
0023e6  lea     0x2354,a0
0023ec  movea.l (a0,d0.w),a0
0023f0  moveq   #0,d0             ; pass = 0  (sets Z)
0023f2  jsr     (a0)
0023f4  btst    #0,0xff0ac9       ; match?
0023fc  bne     0x2404            ; stop on the matching instruction
002400  bra     0x23dc
002404  rts

; 0x2406  EXECUTE
002406  btst    #1,0xff0aca
00240e  beq     0x241a
002410  btst    #0,0xff0aca
002418  bne     0x2444            ; aborted
00241a  cmpa.l  a5,a6
00241c  ble     0x2444
002420  move.w  (a5),d0
002422  lsl.w   #2,d0
002424  lea     0x2354,a0
00242a  movea.l (a0,d0.w),a0
00242e  moveq   #1,d0             ; pass = 1  (clears Z)
002430  jsr     (a0)
002432  btst    #0,0xff0ac9
00243a  bne     0x2444
00243e  cmpa.l  a6,a5
002440  blt     0x2406
002444  rts
```

The pass number is passed in `d0`, but no handler reads `d0`: the
`moveq` that loads it also sets the Z flag — `moveq #0` sets Z and
`moveq #1` clears it — and every handler's *first instruction* is a
branch on Z. A handler that only has an effect when executing begins `beq
<epilogue>`; opcode 1 begins `bne 0x2502` (on the execute pass, skip the
block); opcodes 0, 3 and 4 begin with no branch at all and act on both
passes. An interpreter must reproduce this: **each opcode is a pair of
behaviours, "scan" and "execute", and on the scan pass most opcodes do
nothing but advance.**

The scan loop stops as soon as `0xff0ac9` bit 0 (the **match bit**) is
set, leaving `a5` on the instruction that set it. Only three handlers
set it: opcode 1 (`0x24f8`), opcode 19 (`0x2456`), and built-in 1
(`0x453e`).

`0xff0ac9` also selects what the scan pass is scanning *for*:

| value | set by | opcode 1 acts | opcode 2 acts |
|---|---|---|---|
| 4 (bit 2) | `0x2320` | no (`btst #1` fails at `0x246e`) | yes (`btst #2` at `0x25b0`) |
| 2 (bit 1) | `0x1a22` | yes | no |
| 0x10 (bit 4) | `0xb024` | no | no; opcode 19 matches |

### 3.2 What starts a script

**(a) A verb on an object.** The player picks a verb (or uses the
object's default verb) and clicks an object; `0xff06ae` holds the
object, `0xff06be` the verb, `0xff06b6` the second object if there is
one, and the action driver at `0x19b0` runs. Verbs are 1..10 in the
order of the strings at `0x2fe5e`: Pull, Take, Open, Shut, Use, Give,
Push, Talk to, Eat, Look at.

**(b) Walking into a doorway — verb 11.** Every frame, the routine at
`0x12cc` divides Shaggy's position by 8 (`d0 = 0xff04dc >> 3`,
`d1 = 0xff04f4 >> 3`), walks the object array skipping records whose
room is not the current room and records that are actors
(`btst #1,0x18(a0)`), resolves each one's box (record +0x02) through the
box index, and rectangle-tests the four words of the box record. On a
hit it sets `0xff06ae = d7`, and if that differs from `0xff06b4` it does
`move.w #11,0xff06be; bset #7,0xff09dc; bsr 0x19b0` (`0x1382`). Verb 11
is not on the verb bar; 68 of the shipped hotel and carnival `on` blocks
use it.

**(c) Entering a room.** The room loader `0x2138` ends with
`bsr 0x221a`, which runs the room record's +0x0c script. It does **not**
scan: it sets `0xff0878 = 0x2406` and calls the execute loop directly,
so any opcode 1 inside a room script is skipped.

**(d) Every frame in a room.** `0x21c0` (the routine that publishes the
hovered object and verb to the UI) ends with `bsr 0x2278`, which runs
the room record's +0x10 script the same way — execute loop, no scan.

**(e) The boot script.** At `0x7f5a` the boot reads `block[+0x30]`:
its first word becomes `0xff06ac` (the starting room: 0x10 = *The Lobby*
for the hotel, 0x0c for the carnival), its second word goes to
`0xff06a6`, and the third is the length of the code that follows —
i.e. the same six-byte header an object script has.

**(f) The wake pass.** `0xafda` walks every object record; for each one
whose flags byte at +0x18 has bit 2 set *and* whose room is 1 or the
current room, it sets `0xff0ac9 = (0xff0ac9 & 0x80) | 0x10`,
`0xff0878 = 0x23dc`, and scans that object's script. Only opcode 19
matches under bit 4. On a match it clears the bit, sets
`a6 = a5 + 2(a5)`, `a5 += 4`, `0xff0878 = 0x2406` and executes the
block (`0xb06a`..`0xb096`). **In the shipped game this never fires**:
the only writers of the +0x18 flags word are the seed copy at `0x7fac`
and the `bclr #2` at `0xb072`, and no seed record in either scenario has
that bit set (the high byte of the seed flags word is only ever `0x00`,
`0x02` or `0x0a`). The 25 opcode-19 blocks in the shipped scripts are
therefore dead code. An interpreter must still implement the pass, or
must decide deliberately not to.

### 3.3 The action driver at `0x19b0`

This is the whole of "the player clicked something".

```
0019b0  clr.w   0xff080e              ; choice count = 0
0019b6  bclr    #3,0xff0aca
0019be  movea.l 0xff068e,a0
0019c4  movea.l 0x2c(a0),a6           ; script base
0019c8  movea.l 0x28(a0),a0           ; object table
0019cc  move.w  0xff06ae,d0
0019d2  subq.w  #3,d0
0019d4  bmi     0x1bd8                ; object < 3: nothing to run
0019d8  cmpi.w  #2,0xff06be           ; verb 2 = Take ...
0019e0  bne     0x19f8
0019e2  ... cmpi.w #1,6(a1,d1.w)      ; ... and already carried?
0019f6  rts                           ;     then do nothing
0019f8  lsl.w   #3,d0
0019fa  adda.l  (a0,d0.w),a6          ; a6 = script start
0019fe  movea.l a6,a5
001a00  move.w  (a5)+,d0              ; length
001a02  move.w  (a5)+,0xff06b0        ; gender key
001a08  tst.w   (a5)+
001a0a  lea     (a6,d0.w),a6          ; end pointer
001a0e  movem.l a5-a6,-(a7)
001a12  bsr     0x2320                ; PASS 1: gather dialogue choices
001a16  tst.w   0xff080e
001a1c  beq     0x1a22
001a1e  bsr     0x1bda                ; choices found: open the menu
001a22  move.b  #2,0xff0ac9           ; scan mode = match a verb block
001a2a  move.l  #0x23dc,0xff0878
001a34  move.w  0xff06be,d7           ; the verb, for opcode 1
001a3a  move.b  0xff09de,-(a7)
001a40  bsr     0x23dc                ; PASS 2: find the matching block
001a46  btst    #0,0xff0ac9
001a4e  beq     0x1a80                ; no match -> refusal line
001a52  bset    #6,0xff0ac9           ; remember that we handled it
001a5a  move.w  6(a5),d0
001a5e  lea     (a5,d0.w),a6          ; a6 = END OF THE MATCHED BLOCK
001a62  lea     8(a5),a5              ; a5 = its body
001a66  move.l  #0x2406,0xff0878
001a70  bclr    #0,0xff0ac9
001a78  bsr     0x1bda
001a7c  bsr     0x2406                ; PASS 3: run the body
```

Pass 1 runs with `0xff0ac9 = 4`, so opcode 1 skips every verb block and
only opcode 2 does anything: the dialogue choices reachable at the top
level and inside true conditions are filed away (§3.5). Pass 2 runs with
`0xff0ac9 = 2`, so opcode 2 is inert and opcode 1 compares verbs. Pass 3
runs the matched block **and only the matched block** — `a6` is set from
the block's own length word at +6, not from the script's end.

After pass 3, `0x1a84` re-gathers the choices if the verb was 8 (Talk
to) so that the menu stays live, and `0x1ba0` handles the pending verb
and the "now talk to them" request in `0xff0aca` bit 3.

### 3.4 Opcode 1, and how two-object commands pick a script

`0x246a`:

```
00246a  bne     0x2502                ; execute pass: skip the block
00246e  btst    #1,0xff0ac9
002476  beq     0x2502                ; not looking for verbs: skip
00247a  cmp.w   2(a5),d7              ; verb mismatch?
00247e  bne     0x2502
002482  cmpi.w  #5,d7                 ; Use  -> clear the "giving" bit
002488  bclr    #0,0xff09e8
002492  cmpi.w  #6,d7                 ; Give -> set it
00249a  bset    #0,0xff09e8
0024a2  tst.w   4(a5)                 ; no second object named?
0024a6  beq     0x24f8                ;   -> MATCH
0024aa  tst.w   0xff06b6              ; is there a second object yet?
0024b2  bset    #5,0xff0ac9           ;   no: this object wants a pair
0024c6  bset    #0,0xff09de           ;       open the pair, ask for one
0024ce  clr.w   0xff06b6
0024d4  bra     0x2502                ;       and skip this block
0024e2  cmp.w   2(a5),d7              ; yes: verb still has to match
0024ea  move.w  0xff06b6,d0
0024f0  cmp.w   4(a5),d0              ;      and the second object too
0024f8  bset    #0,0xff0ac9           ;   -> MATCH
002500  rts
002502  move.w  6(a5),d0              ; skip: a5 += total length
002506  lea     (a5,d0.w),a5
```

So a block is `on <verb> [with <object>]` and matches when the verb is
equal and either the block names no second object or the named object
equals `0xff06b6`. When a block *does* name a second object but none has
been clicked yet, the VM flips into "waiting for the second click"
(`0xff09de` bit 0) instead of matching.

**Which script runs.** The click handler at `0x11f6` puts the clicked
object into `0xff06ae` if `0xff09de` bit 0 is clear, and into
`0xff06b6` otherwise (or 0 if it is the same object). The driver always
runs the script of `0xff06ae`. Therefore, for both "Use X with Y" and
"Give X to Y", **X — the first object clicked — owns the script that
runs, and Y is the operand compared against the block's +4 word.** Of
the 585 `on` blocks in the two scenarios, 81 name a second object.

### 3.5 Dialogue choices — opcode 2

Opcode 2's header is 14 bytes and its body is everything after it:

```
+0x00  word  2
+0x02  long  offset of the menu line, into the string block
+0x06  long  offset of the reply line
+0x0a  word  total length, header included
+0x0c  word  an id (not decoded; 0xffff in most blocks)
+0x0e        the body, (+0x0a) - 14 bytes of it
```

The handler at `0x25ac` runs only on the gather pass and only while
`0xff0ac9` bit 2 is set, refuses to file more than five choices
(`cmpi.w #5,d0; bge`), and stores, for slot *i* = `0xff080e`:

```
0025ec  L(0xff0838 + i*4) = strings + long(a5+2)     ; the menu line
0025f6  L(0xff084c + i*4) = strings + long(a5+6)     ; the reply line
0025fa  W(0xff082e + i*2) = word(a5+0x0a) - 0x0e     ; the body length
002606  W(0xff0824 + i*2) = word(a5+0x0c)            ; the id
002610  L(0xff0860 + i*4) = a5 + 0x0e                ; the body pointer
002614  0xff080e += 1
```

then advances `a5` by the total, so the body is never entered on the
gather pass. When the player picks slot *i*, the menu code runs the
body with `a5 = L(0xff0860+i*4)` and `a6 = a5 + W(0xff082e+i*2)`.

A choice whose body length is 0 simply prints the reply and ends the
exchange; the pattern in the shipped scripts is that a body ends with
`call 1` (built-in 1, `bset #0,0xff0ac9`) to tell the driver the
command was handled.

### 3.6 if / if-else — opcodes 3 and 4, and the nested runner

Both have a 16-byte header:

```
+0x00  word  3 or 4
+0x02  word  total length of the block, header included
+0x04  word  length of the else-part that FOLLOWS the block
+0x06  word  left operand A
+0x08  word  left operand B
+0x0a  word  right operand A
+0x0c  word  right operand B
+0x0e  word  kind
+0x10        the then-body
```

Kind bits, from the evaluator at `0x2796`:

| bit | meaning |
|---|---|
| 0 | left is `flag[+0x06].bit(+0x08)` |
| 1 | left is `object(+0x08).field(+0x06)`, except that object 1 field 6 is the room word |
| neither | left is the immediate `+0x06` |
| 2 | right is `flag[+0x0a].bit(+0x0c)` |
| 3 | right is `object(+0x0c).field(+0x0a)` |
| neither | right is the immediate `+0x0a` |
| 4 | compare `==` |
| 5 | compare `>` |
| 6 | compare `<` |
| none of 4,5,6 | compare `!=` |

`0x2732` (opcode 3):

```
002732  bsr     0x2796
002736  beq     0x2740
00273a  lea     0x10(a5),a5        ; true: fall straight into the body
00273e  rts
002740  move.w  2(a5),d0           ; false: skip the whole block
002744  lea     (a5,d0.w),a5
```

so opcode 3's body runs **inline, under whichever loop is running**, on
both passes. Its +4 word is 0 in all 498 occurrences in the object scripts: opcode 3
is opcode 4 with an empty else-part and no nesting.

`0x274a` (opcode 4):

```
00274a  bsr     0x2796
00274e  beq     0x278c
002752  movem.l a5-a6,-(a7)
002756  move.w  2(a5),d0
00275a  lea     (a5,d0.w),a6       ; a6 = end of the then-body
00275e  lea     0x10(a5),a5
002762  movea.l 0xff0878,a0        ; THE NESTED RUNNER
002768  jsr     (a0)
00276a  btst    #0,0xff0ac9
002772  beq     0x277c
002776  lea     8(a7),a7           ; matched inside: leave a5 there
00277a  rts
00277c  movea.l a5,a4
00277e  movem.l (a7)+,a5-a6
002782  move.w  4(a5),d0
002786  lea     (a4,d0.w),a5       ; skip the else-part
00278a  rts
00278c  move.w  2(a5),d0           ; condition false:
002790  lea     (a5,d0.w),a5       ;   land exactly on the else-part
```

`0xff0878` holds the address of the loop that is currently running —
`0x23dc` during a scan, `0x2406` during an execution — so opcode 4's
body is interpreted in the same mode as its surroundings, and a match
found inside it propagates out with `a5` still pointing at the matching
instruction. The else-part is not a separate block: it is the next
`+0x04` bytes of ordinary instructions after the block, executed by
falling through when the condition is false and skipped by arithmetic
when it is true. Walking the shipped scripts with that flat convention
decodes 181 of 181 hotel scripts and 155 of 155 carnival scripts to the
byte.

### 3.7 No match: the refusal line

If pass 2 found nothing and no pair is open, `0x1b32` builds a line:

```
001b32  lea     0x2fea0,a0
001b38  subq.w  #1,d7
001b3a  lsl.w   #2,d7
001b3c  movea.l (a0,d7.w),a2       ; the refusal string for this verb
001b40  lea     0xff08bc,a1
001b46  move.b  (a2)+,(a1)+        ; copy until NUL
001b4c  cmpi.b  #1,-1(a1)          ; byte 0x01 is a placeholder
001b58  btst    #0,0xff06b0
001b64  btst    #1,0xff06b0
001b70  lea     0x2ffe0,a0         ; bit0 set, bit1 clear -> "him"
001b7a  lea     0x2ffe4,a0         ; both set             -> "her"
001b84  lea     0x2ffdb,a0         ; bit0 clear           -> "that"
001b8a  bsr     0x5b56             ; append it
001b94  lea     0xff08bc,a0
001b9a  jsr     0xa130             ; draw the line
```

`0xff06b0` is the script header's second word, so each object carries
its own pronoun. Verb 11 never reaches this code (`cmpi.w #0xb,d7; beq`
at `0x1b2a`), which is why walking into a doorway with no matching block
is silent.

### 3.8 Pseudocode

```
run(a5, a6, pass):
    while True:
        if pass == EXECUTE and aborted(): return
        if a5 >= a6: return
        op = word(a5)
        handler[op](a5, pass)          # advances a5 itself
        if match_bit: return
        if pass == EXECUTE and a5 >= a6: return

act(object, verb, second):
    script  = script_base + long(object_table + (object-3)*8)
    length  = word(script)
    gender  = word(script+2)
    code    = script + 6
    end     = script + length

    mode = GATHER;  choices = []
    nested = SCAN;  run(code, end, SCAN)          # opcode 2 files choices
    if choices: open_menu(choices)

    mode = MATCH_VERB
    nested = SCAN;  a5 = run_and_stop(code, end)  # opcode 1 sets the match
    if not matched:
        if not pair_open and verb != 11: say(refusal[verb], gender)
        return
    body_end = a5 + word(a5+6)
    nested = EXECUTE
    run(a5+8, body_end, EXECUTE)
```

---

## 4. The opcodes

All 34, in numeric order. **Bytes** is the instruction's total length;
for the five block opcodes it is a word inside the instruction, and the
column gives the header size and where that word sits. **Uses** counts
every occurrence in both scenarios' object scripts, room scripts and
boot scripts — 7677 instructions in total, all decoded. The lengths
agree with the `FIXED`/`BLOCK` tables in `examples/scooby/script.py`;
each was re-derived here from the handler's own epilogue (`lea N(a5),a5`
or `lea (a5,d0.w),a5`).

Three conventions recur:

* **actor selector** — a word: 1 means Shaggy (slot 0), 2 means Scooby
  (slot 1), anything else is an object word whose record byte +0x19 is
  the slot, plus 2. Proven identically at `0x2ad0`, `0x2b0e`, `0x4912`,
  `0x4944`, `0x4f74`, `0x4fc2`, `0x2514`, `0x2562`.
* **bit 15 of the first operand** means "and wait for it" in opcodes 11,
  13, 14, 15, 27 and 30.
* **object word** operands are converted with `subq.w #3; mulu.w #0x1a`.

| op | mnemonic | bytes | handler | uses | operands, semantics and the instructions that prove them |
|---|---|---|---|---|---|
| 0 | `nop` | 2 | `0x250c` | 0 | The whole handler is `tst.w (a5)+; rts`. Never used by the shipped data. |
| 1 | `on` | header 8, total at +6 | `0x246a` | 585 | `+2` verb (1..11), `+4` second object or 0, `+6` total length, body from +8. Matches only on the scan pass with `0xff0ac9` bit 1 set; see §3.4 for the full listing. Sets the match bit at `0x24f8` leaving `a5` on the opcode. |
| 2 | `choice` | header 14, total at +0x0a | `0x25ac` | 62 | `+2` long menu-string offset, `+6` long reply-string offset, `+0x0a` total, `+0x0c` id (not decoded), body from +0x0e. Files one dialogue option on the gather pass; §3.5. |
| 3 | `if` | header 16, total at +2 | `0x2732` | 525 | Condition header of §3.6. True: `lea 0x10(a5),a5`, body runs inline. False: `a5 += +2`. The `+4` word is 0 in every object-script occurrence. |
| 4 | `ifelse` | header 16, total at +2 | `0x274a` | 278 | Same header. True: body runs through the nested runner at `0xff0878`, then `a5` skips `+4` more bytes. False: `a5 += +2`, landing on those `+4` bytes. §3.6. |
| 5 | `put` | 6 | `0x28a0` | 476 | `+2` object word (bit 15 masked at `0x28a8`), `+4` destination room; room 1 means "carried". `0x2908`: `move.w 4(a5),6(a0)`. For an actor leaving the current room it frees the animation slot (`0x28f6`, `move.b #0xff,0x19(a0)`); it keeps the inventory word list at `0xff0a1e` in step (`0x2a4a` removes, `0x2a70` appends) and republishes `0xff06ec`. |
| 6 | `image` | 6 | `0x2b5a` | 646 | `+2` selector, `+4` cel number. Selector 1 writes `0xff0488` and waits on `0xff0ab5` bit 0 (plane A); 2 writes `0xff048a`, bit 1 (plane B); otherwise the object's animation slot gets `W(0xff048c+slot*2) = +4`, `W(0xff061c+slot*2) = 0xffff`, waits for `0xff0abd`, and `record[+0] = +4` (`0x2bec`). An object with no slot (`+0x19` = 0xff) only gets `record[+0] = +4`. |
| 7 | `refresh` | 4 | `0x2bf8` | 9 | `+2` object word. If the object is in the current room, `ori.b #1,0x18(a0,d0.w)` — ask for a redraw. If it is carried, recount `0xff0a1e`, update `0xff06ec` and call the panel redraw at `0x5cf6`. |
| 8 | `set` | 12 | `0x2c56` | 767 | `+2`/`+4` destination, `+6`/`+8` source, `+0x0a` kind. Source: bit 2 -> `flag[+6].bit(+8)`, bit 3 -> `object(+8).field(+6)`, else the immediate `+6`; bit 7 replaces it with `random(+6)` through the LCG at `0x22d6` over `0xff0428`. Destination: bit 0 -> `flag[+2].bit(+4)`, set/cleared when bit 4 is set, otherwise toggled; else `object(+4).field(+2)`, with bit 4 = move, bit 5 = subtract, bit 6 = or, none = add. **The toggle path at `0x2d08` is `eor.w d2,(a0,a3.w)` where every other path uses `d3` — an apparent ROM bug.** |
| 9 | `defverb` | 6 | `0x2d5e` | 71 | `+2` object word, `+4` **byte** = the verb, `+5` **signed byte** = which of the two slots at record +0x16. `0x2d7c`: `lea 0x16(a0,d0.w),a1` after `d0 += sign_extend(byte 5(a5))`; `0x2d9a`: `move.b 4(a5),(a1)`. If the object is the hovered one it also republishes the status line through `0x5bdc`. Only two operand pairs occur in the shipped data: value 3 slot 0 (29 times) and value 4 slot 0 (42). |
| 10 | `graphic` | 6 | `0x2dc0` | 3 | `+2` object word, `+4` value written to record +0x04 (`0x2dd4`) — the sprite index (§2.2). If the object is carried (`record[+6] == 1`) it redraws the inventory panel (`0x2de4`). |
| 11 | `place` | 6 | `0x2df2` | 824 | `+2` object word, bit 15 = wait; `+4` walk box, 0 meaning "out of sight". For an actor, `record[+0] = +4` and nothing else. Otherwise `record[+2] = +4` when non-zero (`0x2e2a`); if the object is not in the current room the box is written straight to `record[+0]`; if it is, `record[+0]` gets the new box or is OR-ed with `0xc000` when the box is 0, and `record[+0x18] |= 1` asks for the move. With bit 15 the handler spins until that bit clears (`0x2e6e`). This is the opcode that makes something visibly move between two positions. |
| 12 | `call` | 4 | `0x2e9e` | 403 | `+2` built-in number. `0x2ea2`: `move.w 2(a5),d0; lsl.w #2,d0; movea.l 0x2eb4(pc,d0.w),a0; jsr (a0)`. The table is 62 longs at **`0x2eb4`..`0x2fac`**; entry 0 points at the `rts` at `0x2fac`. Operands 1..61 all occur. See §4.1. |
| 13 | `sound` | 4 | `0x48b6` | 421 | `+2` id. Bit 15 clear: `ext.l d0; bsr 0x9762` (a sound effect; `0x9762` calls the driver entry `0x1f6ef8`). Bit 15 set: `andi.w #0x7fff,d0; bsr 0x9776` (music; driver entry `0x1f6f16`). |
| 14 | `walkspot` | 10 | `0x49ba` | 112 | `+2` actor (bit 15 masked, 0 = no-op), `+4` speed — **negative means teleport**, `+6` pose/frame, `+8` index into the spot table at `L(0xff0696)` (two words per entry, each shifted left 3 at `0x49f8`). Shares its handler with opcode 27, which it distinguishes by re-reading its own opcode word (`0x49d8`: `move.w (a5),d0; cmpi.w #0x0e,d0`). Teleport writes `0xff04dc`/`0xff04f4` and waits on `0xff0abd`; otherwise `d2 = 4(a5); bsr 0x5002` walks. |
| 15 | `moveto` | 12 | `0x4c20` | 166 | `+2` mode (bit 15 masked): 1 walks through `0x5002` and then copies the reference object's `record[+8]` into `0xff05b8`; 2 walks through `0x5ad8` and copies into `0xff05ba`; anything else calls `0x4b24`. `+4` names the reference — 1 adds Shaggy's position, 2 adds Scooby's, otherwise the object's own anchor: for an actor `record[+0x12]`/`record[+0x14]` (`0x4cd2`), for a prop the box record's words at +8 and +0x0a (`0x4cf8`). `+6` is a pose, `+8`/`+0x0a` the offset and speed. The handler also looks the object up in a table of 36 longs at `0x32670` keyed on the record's +2 word (`0x4c62`); what that table holds is not decoded. |
| 16 | `say` | 8 | `0x4d80` | 883 | `+2` style, `+4` long string offset. `0x4e42`: `movea.l 0xff068e,a0; movea.l 0x20(a0),a0; adda.l 4(a5),a0; bsr 0xa130`. Style bits: low nibble is a colour index passed to `0x7a2a` (`0x4e38`), bits 4-5 are shifted into `0xff068c` (`0x4e24`), bit 12 pre-sets `0xff0abc` bit 0 and `0xff09de` bit 4 (`0x4d92`), bit 13 switches the plane-A image set through `0xfc740` before the line and restores it after (`0x4daa`, `0x4e7a`). Bits 14 and 15 are set in 192 of the 883 uses and are not read by the handler. |
| 17 | `gotofade` | 8 | `0x4f0e` | 73 | `+2` room -> `0xff06ac`, `+6` -> `0xff05b8`, fade out through `0x9de0`, `+4` shifted left 2 -> `0xff069e` (the entry spot), then `bsr 0x2196` (load the room, fade in, publish). |
| 18 | `setbox` | 6 | `0x4f50` | 0 | `+2` object word, `+4` -> `record[+2]`. Byte-identical to opcode 20; never used by the shipped data. |
| 19 | `wake` | header 4, total at +2 | `0x2446` | 25 | `+2` total length, body from +4. On the execute pass it skips itself. On the scan pass it sets the match bit if and only if `0xff0ac9` bit 4 is set, which only `0xb024` does. See §3.2(f): in the shipped game the pass that sets that bit is never reached, so these 25 blocks are dead. |
| 20 | `setbox` | 6 | `0x2e7e` | 125 | `+2` object word, `+4` -> `record[+2]` (`0x2e92`). Sets an object's destination box without asking for a move. |
| 21 | `wait` | 4 | `0x48d8` | 284 | `+2` frames. `0x48dc`: `move.w 2(a5),0xff08ae`, then spin on the frame wait `0xb0b8` until `0xff08ae` goes negative. |
| 22 | `hold` | 4 | `0x2ad0` | 103 | `+2` actor selector. With interrupts masked (`move.w #0x2700,sr`), `bset d0,0xff0abf` — freeze that actor's animation. Paired with opcode 25 in the scripts (49 `hold 1` and 49 `hold 2` against 54 and 52 of opcode 25). |
| 23 | `syncwait` | 4 | `0x4fbe` | 110 | `+2` actor selector. Spins on `0xb0b8` until `0xff0abc` bit *slot* is set (`0x4ff2`). |
| 24 | `goto` | 8 | `0x4ee4` | 61 | `+2` room -> `0xff06ac`, `+6` -> `0xff05b8`, `+4` shifted left 2 -> `0xff069e`, `bsr 0x2138`. Identical to opcode 17 without the fade. |
| 25 | `release` | 4 | `0x2b0e` | 111 | `+2` actor selector. `bset d0,0xff0abe`, spin until `0xff0abd` bit *slot* is set, then `bclr d0,0xff0abf` — let a held actor run one step and unfreeze it. |
| 26 | `cycle` | 10 | `0x2626` | 26 | `+2` slot, 0..4 (`cmpi.w #4,d0; bgt` at `0x262e`), `+4` first, `+6` last, `+8` period. With `+8` negative it clears `0xff0acd` bit *slot* and calls `0x9b10` with `0xff0778 + first*2`. Otherwise it fills five parallel word arrays based at `0xff087c` — `+0x00` first, `+0x0a` last, `+0x14` and `+0x1e` the period — and sets `0xff0acd` bit *slot*. A renderer-side colour or tile cycler; the consumer of `0xff087c` is not decoded. |
| 27 | `walkxy` | 12 | `0x49ba` | 332 | Same handler and same `+2`/`+4`/`+6` as opcode 14, but `+8` and `+0x0a` are a literal x and y rather than a spot index (`0x4a5e`, reached because the opcode word is not 14). |
| 28 | `probe.moving` | 4 | `0x255e` | 4 | `+2` actor selector. `bclr #2,0xff2a00`, then `bset #2,0xff2a00` if that actor's bit is set in `0xff0ab7`. Publishes an engine bit into flag 0 bit 2 so that an `if` can test it. |
| 29 | `probe.ready` | 4 | `0x2510` | 0 | `+2` actor selector. `bclr #3,0xff2a00`, then `bset #3,0xff2a00` if the actor's bit is set in `0xff0abc`. Never used by the shipped data. |
| 30 | `plane` | 6 | `0x268e` | 82 | `+2` selector (bit 15 = wait), `+4` new image set. Selector 1: index `0xfc740` with `old(0xff05b8)*8 + new*2`, write the result to `0xff0488`, store the new set in `0xff05b8`, `bset #0,0xff0ab5`; any other selector does the same through `0xfc700`, `0xff05ba`, `0xff048a` and bit 1. With bit 15 it then waits for `0xff0abc`. The two ROM tables are transition tables indexed by (old, new). |
| 31 | `waitidle` | 4 | `0x4f70` | 87 | `+2` actor selector. Spins on `0xb0b8` while either `0xff0ab7` or `0xff0ac0` has that actor's bit — wait until the actor has finished walking and finished its animation. |
| 32 | `stopanim` | 4 | `0x48f8` | 5 | `+2` actor selector. If `0xff0ab7` bit *slot* is set, clear it; otherwise set `0xff0ac3` bit *slot*. Cancels a move or marks the animation finished. |
| 33 | `anim` | header 8, body length at +2 | `0x4944` | 18 | `+2` body length, `+4` actor selector, `+6` frame delay, body from +8. `0x4982`: `move.l a5+8,(0xff05f4 + slot*4)` — the stream pointer; `0x4990`/`0x4994`: `move.b 6(a5),(0xff0aa5+slot)` and `(0xff0aab+slot)`; `0x499e`: `W(0xff060c+slot*2) = 0`; `bclr slot,0xff0ac3`; `bset slot,0xff0ac0`. Epilogue `lea 8(a5,d0.w),a5`. **The body is an animation stream, not VM code**: it is a sequence of `ff fd <count>` groups of x,y word pairs terminated by `ff ff`, and its consumer is the sprite engine. An interpreter must treat opcode 33 as opaque and skip `8 + word(+2)` bytes. |

### 4.1 The built-ins of opcode 12

62 longs at `0x2eb4`. Their addresses are all proven (they are the table
itself); the meaning below is proven only for the rows marked. The
rest are listed so that a reader knows exactly where to look; most
are one- or two-instruction bit pokes into the renderer and input
latches.

| n | address | uses | what it does |
|---|---|---|---|
| 1 | `0x453e` | 27 | `bset #0,0xff0ac9` — **declare the command handled**, so the driver prints no refusal line. Proven. |
| 2 | `0x4548` | 59 | `bsr 0x219a` — reload the room and republish the UI. Proven. |
| 3 | `0x454e` | 55 | save `0xff09dc`, `bclr #6`, `bsr 0x9de0` (the fade), restore. Proven. |
| 4 | `0x4568` | 3 | run the idle pass `0xafda` and one frame `0xb0b8`, looping until a button edge appears in `0xff09e0`/`0xff09e2` — wait for a keypress. Proven. |
| 5, 12 | `0x45a4` | 5, 5 | the same routine twice in the table: if `0xff2a00` bit 1 is already set, return; otherwise set it and run the long routine at `0x45c4` — the "play this cut-scene once" latch. Proven. |
| 6 | `0x481a` | 14 | not decoded |
| 19 | `0x4374` | 80 | `d0 = random(2)`; play sound `0x25` when it is 0, else `0x26 + d0` — a random Shaggy grunt. Proven. |
| 20 | `0x4390` | 2 | the same with sounds `0x28`/`0x29 + d0`. Proven. |
| 13, 14 | `0x4482`, `0x448c` | 5, 22 | `bset`/`bclr #1,0xff09e8`. Proven. |
| 17, 18 | `0x43ac`, `0x43b6` | 2, 4 | `bset`/`bclr #2,0xff09e8`. Proven. |
| 9, 10 | `0x4496`, `0x44a8` | 1, 1 | `bset`/`bclr #7,0xff09de` together with `#1,0xff0abf`. Proven. |
| 56..61 | `0x2fd6`, `0x2fe0`, `0x2fc2`, `0x2fcc`, `0x2fae`, `0x2fb8` | 2,2,4,2,1,2 | single `bset`/`bclr` pairs on `0xff0aca` bit 1, `0xff09eb` bits 5 and 6. Proven. |
| 11 | `0x2ffc` | 3 | `0xff048a = 0xff05ba + 4; bset #2,0xff09eb`. Proven. |
| 0 | `0x2fac` | 0 | `rts`. |
| 7, 8, 15, 16, 21..55 | see the table | | not decoded; each is a short routine at the listed address |

### 4.2 Opcodes that stay unproven

Nothing in the table is a blank row, but five entries are only partly
read:

* **26 (`cycle`)** — the handler is fully decoded, but what consumes
  `0xff087c` and `0xff0acd` is renderer code that was not followed. All
  26 uses pass slot 0, 1 or 2.
* **15 (`moveto`)** — the operand layout and both walk calls are proven,
  but the 36-long table at `0x32670` that the handler searches is not
  decoded, and the copy of `record[+8]` into `0xff05b8`/`0xff05ba` is
  read from the instructions without an explanation. 162 of its 166 uses
  pass mode 1.
* **16 (`say`)** — bits 14 and 15 of the style word are set in 192 uses
  and no instruction in the handler reads them.
* **2 (`choice`)** — the `+0x0c` word is filed into `0xff0824` and never
  read by any code that was followed; it is `0xffff` in most blocks.
* **12 (`call`)** — 40 of the 62 built-ins were not read; §4.1 gives
  every address.

Opcodes **0**, **18** and **29** have zero uses in either scenario;
their handlers are short and fully decoded, so they are specified above
rather than guessed at.

---

## 5. A worked example: the Statue

Object word **166**, name `"Statue"`, hotel scenario. Its table entry is
at `0x146204 + (166-3)*8 = 0x14671c`; the long there is `0x8224`, so its
script starts at `0x1467ac + 0x8224 = 0x14e9d0`. Its seed record (at
`0xfd3b0`) gives box 194, room 20 (*The Tomb*), state 0, default-verb
word `0x0a0a` — verb 10, *Look at*, in both slots — and flags 0.

### 5.1 The script, 330 bytes

```
14e9d0  01 44 00 0d ff ff 00 01 00 0a 00 00 00 10 00 10
14e9e0  f0 06 00 00 3e ad 00 04 01 24 00 10 00 0b 00 00
14e9f0  00 00 00 00 00 11 00 01 00 08 00 00 00 18 00 0e
14ea00  00 01 00 01 00 00 00 03 00 1e 80 01 00 00 00 04
14ea10  00 2a 00 d2 00 0c 00 00 00 00 00 00 00 11 00 02
14ea20  00 00 3e eb 00 00 1f 54 00 1a ff ff 00 10 f0 06
14ea30  00 00 3e cf 00 0c 00 01 00 02 00 00 3e fa 00 00
14ea40  1f 54 00 0e ff ff 00 02 00 00 3f 07 00 00 1f 54
14ea50  00 0e ff ff 00 02 00 00 3f 0f 00 00 1f 54 00 0e
14ea60  ff ff 00 02 00 00 3f 76 00 00 3f 7d 00 96 ff ff
14ea70  00 0d 00 61 00 10 10 06 00 00 3f 1c 00 15 00 14
14ea80  00 0b 00 29 00 00 00 15 00 01 00 0b 00 a6 00 c9
14ea90  00 0b 00 29 00 c7 00 15 00 14 00 0b 00 29 00 00
14eaa0  00 15 00 01 00 0b 00 a6 00 ca 00 0b 00 29 00 c8
14eab0  00 0d 80 61 00 0d 00 3a 00 06 00 1a 00 10 00 10
14eac0  00 0e 00 00 3f 34 00 10 00 0d 00 00 3f 55 00 0b
14ead0  00 a4 00 cb 00 0d 00 46 00 15 00 14 00 0b 00 a4
14eae0  00 cc 00 05 00 a5 00 14 00 08 00 0b 00 00 00 01
14eaf0  00 00 00 11 00 0c 00 01 00 02 00 00 3f 9e 00 00
14eb00  1f 54 00 12 ff ff 00 0c 00 01 00 01 00 08 00 00
14eb10  00 10 00 10 f0 06 00 00 3f b3 00 20 00 0f ff ff
```

The first three words are the header: length `0x0144`, gender key
`0x000d`, and one unused word. The code is
`[0x14e9d6, 0x14e9d6 + 0x144)` = `0x14e9d6..0x14eb1a`, and the driver's
end pointer is `0x14e9d0 + 0x144 = 0x14eb14` — six bytes short of the
last instruction's end, which is harmless because the loop test is
`a5 < a6` (§1.3).

### 5.2 Decoded

Indentation shows nesting: a line is inside the block above it.

```
14e9d6 on        000a 0000 0010         ; verb=10 look, no second object, body=8 bytes
14e9de   say       f006 0000 3ead         ; 'I think this thing is pretty old.'
14e9e6 ifelse    0124 0010 000b 0000 0000 0000 0011 ; flag[11].0 == 0  (then=276 else=16)
14e9f6   on        0008 0000 0018         ; verb=8 talk, no second object, body=16 bytes
14e9fe     move      0001 0001 0000 0003    ; 
14ea08     plane     8001 0000              ; 
14ea0e   ifelse    002a 00d2 000c 0000 0000 0000 0011 ; flag[12].0 == 0  (then=26 else=210)
14ea1e     choice    0000 3eeb 0000 1f54 001a ffff ; prompt='Um, Mr Statue?' reply='...' body=12
14ea2c       say       f006 0000 3ecf         ; 'Why am I talking to a rock?'
14ea34       call      0001                   ; builtin 1 (00453e)
14ea38   choice    0000 3efa 0000 1f54 000e ffff ; prompt='Abracadabra!' reply='...' body=0
14ea46   choice    0000 3f07 0000 1f54 000e ffff ; prompt='Shazam!' reply='...' body=0
14ea54   choice    0000 3f0f 0000 1f54 000e ffff ; prompt='Hokus Pokus!' reply='...' body=0
14ea62   choice    0000 3f76 0000 3f7d 0096 ffff ; prompt='XYZZY!' reply='You have proven yourself worthy.' body=136
14ea70     sound     0061                   ; sfx 97
14ea74     say       1006 0000 3f1c         ; 'Zoinks!  Who said that?'
14ea7c     wait      0014                   ; 20 frames
14ea80     place     0029 0000              ; object 41 'Goblet' box 0
14ea86     wait      0001                   ; 1 frames
14ea8a     place     00a6 00c9              ; object 166 'Statue' box 201
14ea90     place     0029 00c7              ; object 41 'Goblet' box 199
14ea96     wait      0014                   ; 20 frames
14ea9a     place     0029 0000              ; object 41 'Goblet' box 0
14eaa0     wait      0001                   ; 1 frames
14eaa4     place     00a6 00ca              ; object 166 'Statue' box 202
14eaaa     place     0029 00c8              ; object 41 'Goblet' box 200
14eab0     sound     8061                   ; music 97
14eab4     sound     003a                   ; sfx 58
14eab8     setimage  001a 0010              ; object 26 'Uncle Blake' image 16
14eabe     say       000e 0000 3f34         ; "Something's happening over here!"
14eac6     say       000d 0000 3f55         ; 'You may look upon the medallion.'
14eace     place     00a4 00cb              ; object 164 'Tomb' box 203
14ead4     sound     0046                   ; sfx 70
14ead8     wait      0014                   ; 20 frames
14eadc     place     00a4 00cc              ; object 164 'Tomb' box 204
14eae2     put       00a5 0014              ; object 165 'Medallion' -> room 20
14eae8     set       000b 0000 0001 0000 0011 ; flag[11].0 = 1
14eaf4     call      0001                   ; builtin 1 (00453e)
14eaf8   choice    0000 3f9e 0000 1f54 0012 ffff ; prompt='I give up.  Tell me.' reply='...' body=4
14eb06     call      0001                   ; builtin 1 (00453e)
14eb0a on        0008 0000 0010         ; verb=8 talk, no second object, body=8 bytes
14eb12   say       f006 0000 3fb3         ; "I think he's said all he's going to say."

```

Read as source:

```
on look:
    say "I think this thing is pretty old."

if flag[11].0 == 0:                       ; the medallion is still hidden
    on talk:
        walkspot shaggy, speed 1, pose 0, spot 3
        plane A = 0, and wait
    if flag[12].0 == 0:
        choice "Um, Mr Statue?" -> "...":
            say "Why am I talking to a rock?"
            call 1                        ; handled
    choice "Abracadabra!"  -> "..."
    choice "Shazam!"       -> "..."
    choice "Hokus Pokus!"  -> "..."
    choice "XYZZY!"        -> "You have proven yourself worthy.":
        sfx 97
        say "Zoinks!  Who said that?"
        wait 20
        place Goblet, nowhere ; wait 1
        place Statue, box 201 ; place Goblet, box 199
        wait 20
        place Goblet, nowhere ; wait 1
        place Statue, box 202 ; place Goblet, box 200
        music 97 ; sfx 58
        image "Uncle Blake" = 16
        say "Something's happening over here!"
        say "You may look upon the medallion."
        place Tomb, box 203 ; sfx 70 ; wait 20
        place Tomb, box 204
        put Medallion -> room 20          ; it becomes takeable
        flag[11].0 = 1                    ; and the puzzle is done
        call 1
    choice "I give up.  Tell me." -> "...":
        call 1
else:                                     ; 16 bytes, the else-part
    on talk:
        say "I think he's said all he's going to say."
```

Two structural points are worth noting. The `ifelse` at `0x14e9e6`
covers 292 bytes and declares an else-part of 16; those 16 bytes are the
final `on talk` block at `0x14eb0a`, which is reached by falling through
when `flag[11].0` is not 0 and skipped by `0x2786` when it is. The inner
`ifelse` at `0x14ea0e` covers only the first choice and declares an
else-part of 210 bytes — every remaining choice — so once `flag[12].0`
is set, "Um, Mr Statue?" is replaced by the four magic words and "I give
up".

### 5.3 Clicking **Use** on the Statue

With the flags as seeded (`flag[11] = 0x40`, so bit 0 is clear) and the
player in *The Tomb*, clicking the Statue with the verb *Use* selected
sets `0xff06ae = 166`, `0xff06be = 5`, `0xff06b6 = 0`, and calls
`0x19b0`.

1. `0x19d8`: the verb is not 2 (*Take*), so the "already carried"
   shortcut does not apply.
2. `0x19fa`: `a6 = a5 = 0x14e9d0`. The header gives length `0x144`,
   `0xff06b0 = 0x000d`, and `a6 = 0x14eb14`.
3. **Pass 1**, `0x2320`: `0xff0ac9 = 4`, `0xff080e = 0`, scan from
   `0x14e9d6`.
   * `on look` — opcode 1 with `0xff0ac9` bit 1 clear, so `0x2476`
     skips the block.
   * `ifelse flag[11].0 == 0` — true, so the body runs through the
     nested runner, which during a scan is `0x23dc` itself.
     * `on talk` — skipped the same way.
     * `ifelse flag[12].0 == 0` — `flag[12]` is seeded to `0x02`, so
       bit 0 is clear and this is true too; its body is entered.
       * `choice "Um, Mr Statue?"` — `0xff0ac9` bit 2 *is* set, so this
         one is filed: slot 0 gets the two string pointers, body length
         12 and body pointer `0x14ea2c`. `0xff080e` becomes 1.
     * Back in the outer body, the four remaining choices are skipped,
       because the inner `ifelse` was true and `0x2786` advanced `a5`
       past its 210-byte else-part, landing on `0x14eb0a`.
   * `a5` has now reached `0x14eb0a`, which is exactly the outer
     `ifelse`'s `a6`, so the nested run ends; the outer `ifelse` then
     skips its own 16-byte else-part, putting `a5` at `0x14eb1a`, past
     the script's `a6` of `0x14eb14`, and pass 1 ends.
4. `0x1a16`: `0xff080e` is 1, so `0x1bda` prepares a one-line dialogue
   menu. (It is never shown: the menu only opens for *Talk to*.)
5. **Pass 2**, `0x1a22`: `0xff0ac9 = 2`, `d7 = 5`, scan again from
   `0x14e9d6`.
   * `on look` — verb 10 against `d7 = 5`: `0x247a` fails, block
     skipped.
   * `ifelse` true again; inside it `on talk` is verb 8, also skipped;
     the inner `ifelse` is true, and opcode 2 does nothing this time
     because `0xff0ac9` bit 2 is clear. Nothing matches.
   * Pass 2 ends with the match bit clear.
6. `0x1a4e` branches to `0x1a80`. The verb is 5, not 8, so no
   re-gather. `0xff09de` bit 0 is clear (no pair was opened, because no
   block named a second object). The verb is not 11.
7. `0x1b32`: `a2 = long(0x2fea0 + (5-1)*4) = 0x2ff20`, the string
   `"Um, how would you suggest I use \x01"`. The placeholder byte
   `0x01` is replaced using `0xff06b0 = 0x000d`: bit 0 is set and bit 1
   is clear, so `0x1b70` chooses `0x2ffe0`, `"him"`.
8. `0x1b9a`: Shaggy says **"Um, how would you suggest I use him"**.

Nothing in the object records or the flag array changes. For contrast,
clicking *Talk to* takes the same route until pass 2, where the `on
talk` block inside the true `ifelse` matches at `0x14e9f6`; the driver
then sets `a6 = 0x14e9f6 + 0x18 = 0x14ea0e` and `a5 = 0x14e9fe`, and
runs exactly the two instructions of that block — the walk and the plane
change — before opening the menu that pass 1 gathered.

---

## 6. What is left to rip

The VM above is complete: with the tables of §1, the state of §2, the
loops of §3 and the opcodes of §4, every one of the 7677 shipped
instructions can be executed. What it cannot do on its own is draw
anything. This section lists the assets an interpreter still needs, with
their addresses, and says plainly where the format was not decoded.

**Room pictures.** `block[+0x0c] + room[+0x04]`. The loader at `0x7636`
does `tst.l (a0)+` — the first long is a header the routine skips — then
`lea 0xff3000,a1; lea 0xff4000,a2; bsr 0x97aa`. `0x97aa` is a
decompressor that writes a length into `0xff3000` and the data into
`0xff4000`; the result is DMA-ed into VRAM by `0x992c`. **The
compression format is not decoded.** The same routine is used for the
title art at `0x30916` (`0x76b2`), which makes it a good test case.

**Room geometry and the hit mask.** `block[+0x10] + room[+0x08]`. The
record's first two words are the scroll extent (copied to `0xff06c6`
and `0xff06c8` at `0x77f8`); the long at +4 is an offset from +8 to a
block decompressed by `0x97aa` into `0xff8c00` (`0x7806`). That buffer
is what the crosshair hit test at `0x0e94` reads after dividing the
pointer position by 8 and adding the camera. **The mask's bit layout is
not decoded**; the box path of the same test is (§1.5).

**Walk boxes.** `block[+0x1c] + long(block[+0x24] + (box-1)*4)`; 211
boxes in the hotel, 214 in the carnival. Words +0x00..+0x06 are a
rectangle in eight-pixel cells (`0x1018`, `0x1310`); words +0x08 and
+0x0a are an anchor point (`0x4cf8`). The rest of the record, including
its size, is not decoded. The pathfinder that turns a box into a walk
is at `0x5002` (Shaggy) and `0x5ad8`/`0x566a`.

**Sprites and actors.** `block[+0x18]`, stride **0x182**, entry
`record[+4] - 1` (`0x5d8a`..`0x5d9e`). A second table of 36 longs at
`0x32670` is searched by opcode 15 on the object's `+2` word
(`0x4c62`). Neither record's layout is decoded.

**Animation streams.** Carried inline in opcode 33's body and installed
at `0xff05f4 + slot*4`. The shape is visible in the data — `ff fd
<count>` introduces a run of `<count>` x,y word pairs, and `ff ff` ends
the stream — and the 524-byte block at `0x1467ac`, the only part of the
hotel's script area that no pointer reaches, has exactly that shape.
**The stream's opcodes beyond `ff fd`/`ff ff` are not decoded.**

**Palettes and plane image sets.** Two tables of 8-word rows at
`0xfc700` (plane B) and `0xfc740` (plane A), indexed by
`old_set * 8 + new_set * 2` and yielding a word written to `0xff048a` /
`0xff0488` (opcode 30 at `0x26be`, opcode 16 at `0x4e94`). What the
renderer does with that word is not decoded. Opcode 26's cycler writes
five word arrays based at `0xff087c`; its consumer is not decoded.

**Text.** Fully decoded: NUL-terminated 8-bit strings at
`block[+0x20] + offset`, 17692 bytes for the hotel and 16844 for the
carnival, plus the fixed program text listed in §1.7. The line drawer
is `0xa130` (it clears `0xff08bc`, renders, and writes `0x9202` to the
VDP control port at `0xc00004`); the colour selection is `0x7a2a`.
**The font — the glyph tiles and the widths — is not decoded.**

**Sound.** Opcode 13 reaches the driver through `0x9762` (effects,
driver entry `0x1f6ef8`) and `0x9776` (music, `0x1f6f16`). The driver
lives in the last 64 KiB of the cartridge and is not decoded; sound ids
range 0..127 for effects and 15..123 for music in the shipped
scripts.

**Save games.** `0x9334` clears the flag array and then copies 29 bytes
from `0xff0020` into it, which is the password or save restore path.
Not decoded.

---

## Corrections

The reconnaissance in `examples/scooby/README.md` was mostly right. Four things in them are
wrong, and one helper in `examples/scooby/` has a latent bug. They are
listed here so that the code can be fixed on its own branch; **no
`examples/scooby/*.py` file was changed by this document.**

1. **"Execution runs on past the verb block it matched."** It does not.
   `0x1a5a`..`0x1a62` sets `a6 = a5 + word(a5+6)` — the end of the
   matched `on` block — and `a5 = a5 + 8` — its body — before calling
   the execute loop. Only the matched block's body runs. The claim that
   opcode 1 "skips later blocks" during execution is true but
   irrelevant, because execution never reaches them. Consequence:
   `graph.py`'s `follows()`, which collects every effect from the
   matched block to the end of a flat walk of the whole script,
   over-collects. It should stop at `block_start + word(block_start+6)`,
   and it should descend into opcodes 3 and 4 the way the VM does
   rather than reading a flat list.

2. **The object table's base and the "one entry back" script pointer.**
   The table is at scenario block `+0x28` = `0x146204` (hotel) and an
   entry is `(word - 3) * 8` bytes in, with the **script offset first**
   and the **name offset second**. `world.py`'s `OBJECT_TABLE =
   0x1461f0` with `OBJECT_SCRIPT = -4` computes the same addresses, so
   no data is wrong, but the anomaly it documents does not exist: there
   is no entry one back, only a two-field record read in order. The
   proof is `0x19c8`/`0x19f8`/`0x19fa` (§1.2). The table has 181 hotel
   entries, exactly matching the 181 seed records.

3. **"Use X with Y" runs Y's script.** It runs **X's**. The click
   handler at `0x11f6` routes the first click to `0xff06ae` and the
   second to `0xff06b6`, and `0x19cc` runs the script of `0xff06ae`;
   opcode 1 compares `0xff06b6` against the block's `+4` word
   (`0x24ea`). Step 1e already corrected this; it is restated here
   because README.md still has it the other way round.

4. **Opcode 33's body is not VM code.** It is an animation stream
   (§4). `script.py`'s `_walk` recurses into any block whose `total >
   head`, which includes opcode 33, and that makes three hotel scripts
   fail to parse — objects 29 (*Mine Car*), 53 (*Christmas Lights*) and
   90 (*Bed Spring*). Treating opcode 33 as opaque and advancing by
   `8 + word(+2)` gives 181 of 181 hotel scripts and 155 of 155
   carnival scripts, with every byte of the script area accounted for
   (§1.3).

5. **Opcode 19 is a wake block, and it never fires.** Step 1e left it
   as "script extent / labelled entry, only acts when `0xff0ac9` bit 4
   is set". The bit is set only by `0xb024`, inside the idle pass at
   `0xafda`, which visits an object only when bit 2 of its record's
   flags byte at +0x18 is set — and no seed record in either scenario
   sets that bit, nor does any instruction in the ROM. The 25 opcode-19
   blocks are unreachable in the shipped game (§3.2(f)).

6. **`flag[12].3` does have a writer** — the open question from step
   1e. It is written at `0x146b7a`, in the hotel's per-frame room
   script for room 20, *The Tomb*: `if flag[12].5 == 1 { if flag[12].3
   == 0 { flag[12].3 = 1; image "Uncle Blake" = 14; say "This is quite
   interesting!" } }`. The room's entry script at `0x146b48` uses the
   same pair of guards to teleport Uncle Blake to spot 2. Step 1e
   missed it because the census behind it walked only the 181 object
   scripts; the room scripts (41 in the hotel, 77 in the carnival) are
   reached through the room record's +0x0c and +0x10 offsets (§1.4) and
   carry real logic.

7. **A probable ROM bug, for the record.** Opcode 8's flag-toggle path
   at `0x2d08` is `eor.w d2,(a0,a3.w)` where the index should be `d3`,
   as it is on every other path through the handler. An interpreter
   that wants to be bug-compatible will need to decide what `a3` holds
   at that point; the shipped scripts always set bit 4 of the kind
   word, so the toggle path is never taken.
