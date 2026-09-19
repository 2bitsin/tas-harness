#!/usr/bin/env python3
"""The export's roms/ placeholders: empty, and exactly what the profiles name."""

from __future__ import annotations

from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OVERLAY = ROOT / ".outbound" / "overlay"
PLACEHOLDERS = OVERLAY / "roms"
LIBRARY = "roms/"


def _RequireOverlay() -> None:
  """The export drops .outbound/, so a published tree has nothing to check."""
  if not PLACEHOLDERS.is_dir():
    pytest.skip(f"no export overlay at {PLACEHOLDERS}")


def _RomOf(profile: Path) -> str | None:
  for line in profile.read_text(encoding="utf-8").splitlines():
    if line.startswith("rom:"):
      return line.split(":", 1)[1].strip()
  return None


def _Named() -> set[str]:
  """Every cartridge an example profile asks for out of the library."""
  named = {_RomOf(profile)
           for profile in (ROOT / "examples").glob("*/profile.yaml")}
  return {rom for rom in named if rom and rom.startswith(LIBRARY)}


def _Files() -> list[Path]:
  return [path for path in PLACEHOLDERS.rglob("*") if path.is_file()]


def _Placed() -> set[str]:
  """Each placeholder under the path the export will lay it down at."""
  return {path.relative_to(OVERLAY).as_posix() for path in _Files()}


def test_every_placeholder_is_empty():
  _RequireOverlay()
  carrying = sorted(path.relative_to(ROOT).as_posix()
                    for path in _Files() if path.stat().st_size)
  assert not carrying, f"a cartridge would be committed here: {carrying}"


def test_the_placeholders_are_the_cartridges_the_profiles_name():
  _RequireOverlay()
  assert _Placed() == _Named()
