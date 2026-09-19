#!/usr/bin/env python3
"""tools/vendor-homebrew-rom.py: the pins, the tree they produced, the fetch."""

from __future__ import annotations

import importlib.util
import os
import socket
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_SPEC = importlib.util.spec_from_file_location(
  "vendor_homebrew_rom", HERE / "vendor-homebrew-rom.py")
vendor = importlib.util.module_from_spec(_SPEC)
assert _SPEC.loader is not None
_SPEC.loader.exec_module(vendor)

IN_CI = bool(os.environ.get("CI"))


def _WhyNoNetwork() -> str | None:
  try:
    socket.create_connection(("raw.githubusercontent.com", 443),
                             timeout=5).close()
  except OSError as unreachable:
    return f"raw.githubusercontent.com is unreachable: {unreachable}"
  return None


NO_NETWORK = _WhyNoNetwork()


def _RequireNetwork() -> None:
  """A CI runner has the network, so its absence there is a fault, not a skip."""
  if NO_NETWORK is None:
    return
  if IN_CI:
    pytest.fail(f"the pinned ROM could not be fetched in CI: {NO_NETWORK}")
  pytest.skip(NO_NETWORK)


def test_every_member_is_pinned_by_a_sha256():
  assert len(vendor.COMMIT) == 40
  for member, destination, digest in vendor.VENDORED:
    assert member and destination.startswith("examples/homebrew/")
    assert len(digest) == 64


def test_the_url_is_the_pinned_commit_and_member():
  url = vendor.SourceUrl(vendor.VENDORED[0][0])
  assert url.startswith("https://raw.githubusercontent.com/")
  assert vendor.COMMIT in url and url.endswith(vendor.VENDORED[0][0])


def test_anything_but_the_pinned_bytes_is_refused():
  member, _, digest = vendor.VENDORED[0]
  with pytest.raises(SystemExit) as refusal:
    vendor.Checked(b"not the rom", "a test", member, digest)
  assert digest in str(refusal.value)


def test_the_checked_in_tree_is_the_pin():
  root = vendor.Repository()
  sizes = vendor.Verify(root)
  assert sizes == [(root / where).stat().st_size
                   for _, where, _ in vendor.VENDORED]


def test_the_rom_is_a_mega_drive_cartridge():
  """The console's own header, so a mispinned file cannot look like a ROM."""
  rom = (vendor.Repository() / vendor.VENDORED[0][1]).read_bytes()
  assert rom[0x100:0x10f].decode("ascii") == "SEGA MEGA DRIVE"


def test_a_missing_member_is_named_rather_than_fetched(tmp_path):
  with pytest.raises(SystemExit) as refusal:
    vendor.Verify(tmp_path)
  assert vendor.VENDORED[0][1] in str(refusal.value)


def test_an_edited_rom_fails_verify(tmp_path):
  for _, where, _ in vendor.VENDORED:
    planted = tmp_path / where
    planted.parent.mkdir(parents=True, exist_ok=True)
    planted.write_bytes(
      (vendor.Repository() / where).read_bytes() + b"\n")
  with pytest.raises(SystemExit) as refusal:
    vendor.Verify(tmp_path)
  assert "never edited" in str(refusal.value)


def test_the_pinned_commit_still_serves_these_bytes():
  _RequireNetwork()
  for member, _, digest in vendor.VENDORED:
    assert vendor.Digest(vendor.Download(member)) == digest
