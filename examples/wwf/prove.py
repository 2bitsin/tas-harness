"""Play every row of the move table at VERY HARD, one verdict each."""

import os
import sys

sys.path.insert(0, "examples/wwf")

import combos
import tash

# The DDT is the one row that needs the room to run; it is played from the
# bell, where the wrestlers stand 207 ring units apart.
FROM_RANGE = ("DDT",)


def main():
    run = tash.run
    run.mark("tape")
    run.play(combos.HARDEST_TAPE)
    run.checkpoint("bell")
    measuring = bool(os.environ.get("WWF_MEASURE"))
    for combo in combos.COMBOS:
        name, _, frames, damage, hits, action = combo
        run.restore("bell")
        if name not in FROM_RANGE:
            run.expect(f"{name}: the approach reaches point blank",
                       combos.approach(run))
        landed = combos.perform(run, combo)
        run.mark(name, "combo")
        if measuring:
            print(f"{name}: first {landed.first()}, damage {landed.damage},"
                  f" hits {landed.hits()}, ids"
                  f" {sorted(hex(v) for v in landed.seen())}", flush=True)
            continue
        run.judge(f"{name} plays its own action {hex(action)}",
                  action in landed.seen(),
                  f"ids {sorted(hex(v) for v in landed.seen())},"
                  f" first hit at {landed.first()} for {frames},"
                  f" {landed.damage} damage for {damage},"
                  f" {landed.hits()} hits for {hits}")
    run.look()


main()
