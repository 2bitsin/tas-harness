"""Capture an untouched arrival, with the profile's watches beside each PNG."""

import argparse
import csv
import json
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PROFILE = HERE / "profile.yaml"
TITLE = HERE / "tapes/title-to-control.anchors/title.png"
OPTIONS = "TASH_SCOOBY_INTRO"
TITLE_LIMIT = 3000
PRESS_FRAMES = 4
BETWEEN_PRESSES = 60
SCENARIOS = ("hotel", "carnival")


def binary():
    beside = [ROOT / "_install/tash"]
    beside += sorted((ROOT / "_build").glob("*/bin"))
    return next((path / "tash" for path in beside
                 if (path / "tash").is_file()), None)


def tap(run, button):
    run.hold(1, [button])
    run.step(PRESS_FRAMES)
    run.release(1)


def capture(run, scenario, every, frames, outdir):
    run.reset()
    run.run_until(f"template_image {TITLE} at 0.95", TITLE_LIMIT)
    print(f"title frame {run.observe()['frame']}", flush=True)
    if scenario == "carnival":
        tap(run, "down")
        run.step(BETWEEN_PRESSES)
    tap(run, "start")
    run.step(BETWEEN_PRESSES)
    tap(run, "start")
    seen = run.observe()

    columns = ["frame", "room"]
    columns += [name for name in seen["watches"] if name != "room"]
    outdir = pathlib.Path(outdir)
    with (outdir / "frames.tsv").open(
            "w", newline="", encoding="utf-8") as table:
        writer = csv.DictWriter(table, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        for frame in range(0, frames + 1, every):
            if frame:
                run.step(every)
            run.look(str(outdir / f"{frame:05d}.png"))
            writer.writerow(dict(frame=frame, **run.observe()["watches"]))
        if frames % every:
            run.step(frames % every)
    print(f"{scenario}: captured {frames // every + 1} frames "
          f"through {frames} untouched frames", flush=True)


def positive(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    parser.add_argument("--every", type=positive, required=True, metavar="N")
    parser.add_argument("--frames", type=positive, required=True, metavar="M")
    parser.add_argument("outdir", type=pathlib.Path)
    args = parser.parse_args()
    executable = binary()
    if executable is None:
        parser.error("build tash in this tree first")
    args.outdir = args.outdir.resolve()
    args.outdir.mkdir(parents=True, exist_ok=True)
    if any(args.outdir.glob("*.png")) or (args.outdir / "frames.tsv").exists():
        parser.error("outdir already contains a capture")
    options = dict(scenario=args.scenario, every=args.every,
                   frames=args.frames, outdir=str(args.outdir))
    done = subprocess.run(
        [str(executable), "run", "--profile", str(PROFILE),
         "--scenario", str(pathlib.Path(__file__).resolve())],
        cwd=ROOT, stdin=subprocess.DEVNULL,
        env=dict(os.environ, **{OPTIONS: json.dumps(options)}), check=False)
    return done.returncode


if __name__ == "__main__":
    if OPTIONS in os.environ:
        import tash

        capture(tash.run, **json.loads(os.environ[OPTIONS]))
    else:
        raise SystemExit(main())
