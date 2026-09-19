#!/usr/bin/env python3
"""tools/vendor-libretro-header.py: the pin, the tree it produced, the fetch."""

from __future__ import annotations

import importlib.util
import os
import socket
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
_SPEC = importlib.util.spec_from_file_location(
  "vendor_libretro_header", HERE / "vendor-libretro-header.py")
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
    pytest.fail(f"the pinned header could not be fetched in CI: {NO_NETWORK}")
  pytest.skip(NO_NETWORK)


def test_the_url_is_the_pinned_commit_and_member():
  url = vendor.SourceUrl()
  assert url.startswith("https://raw.githubusercontent.com/")
  assert vendor.COMMIT in url and url.endswith(vendor.MEMBER)


def test_the_pin_is_a_full_commit_and_a_sha256():
  assert len(vendor.COMMIT) == 40 and len(vendor.SHA256) == 64


def test_anything_but_the_pinned_bytes_is_refused():
  with pytest.raises(SystemExit) as refusal:
    vendor.Checked(b"not the header", "a test")
  assert vendor.SHA256 in str(refusal.value)


def test_the_checked_in_header_is_the_pin():
  root = vendor.Repository()
  assert vendor.Verify(root) == (root / vendor.DESTINATION).stat().st_size


def test_a_missing_header_is_named_rather_than_fetched(tmp_path):
  with pytest.raises(SystemExit) as refusal:
    vendor.Verify(tmp_path)
  assert vendor.DESTINATION in str(refusal.value)


def test_an_edited_header_fails_verify(tmp_path):
  planted = tmp_path / vendor.DESTINATION
  planted.parent.mkdir(parents=True)
  planted.write_bytes(
    (vendor.Repository() / vendor.DESTINATION).read_bytes() + b"\n")
  with pytest.raises(SystemExit) as refusal:
    vendor.Verify(tmp_path)
  assert "never edited" in str(refusal.value)


def test_the_pinned_commit_still_serves_these_bytes():
  _RequireNetwork()
  assert vendor.Digest(vendor.Download()) == vendor.SHA256
