"""An arrival capture keeps each picture beside a row of profile watches."""

import csv
import os
import struct
import subprocess
import sys

import pytest

import intro


def _cartridge_present(rom):
    """A zero-byte file is the published placeholder, not a cartridge."""
    return rom.is_file() and rom.stat().st_size > 0


@pytest.mark.parametrize("scenario", ("hotel", "carnival"))
def test_intro_capture(tmp_path, scenario):
    rom = intro.ROOT / next(line.split(":", 1)[1].strip()
                            for line in intro.PROFILE.read_text().splitlines()
                            if line.startswith("rom:"))
    if not _cartridge_present(rom):
        pytest.skip(f"the scooby rom is not at {rom}")
    if intro.binary() is None:
        pytest.skip("this tree holds no built tash")
    done = subprocess.run(
        [sys.executable, str(intro.HERE / "intro.py"),
         "--scenario", scenario, "--every", "300", "--frames", "600",
         str(tmp_path)], stdin=subprocess.DEVNULL, capture_output=True,
        text=True, timeout=120, env=dict(os.environ, TMPDIR=str(tmp_path)),
        check=False)
    print(done.stdout, end="")
    print(done.stderr, end="", file=sys.stderr)
    assert done.returncode == 0, done.stdout + done.stderr
    assert {path.name for path in tmp_path.glob("*.png")} == {
        "00000.png", "00300.png", "00600.png"}
    for path in tmp_path.glob("*.png"):
        png = path.read_bytes()
        assert png[:8] == b"\x89PNG\r\n\x1a\n"
        assert struct.unpack(">II", png[16:24]) == (256, 224)
    watches = [line.split(":", 1)[1].strip()
               for line in intro.PROFILE.read_text().splitlines()
               if line.strip().startswith("- name:")]
    with (tmp_path / "frames.tsv").open(newline="") as table:
        reader = csv.DictReader(table, delimiter="\t")
        assert reader.fieldnames == ["frame", "room"] + [
            name for name in watches if name != "room"]
        rows = list(reader)
    assert [int(row["frame"]) for row in rows] == [0, 300, 600]
    assert all(int(row["live"]) == 0 for row in rows)
    for row in rows:
        assert all(value is not None and int(value) >= -32768
                   for value in row.values())
