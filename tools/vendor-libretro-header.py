#!/usr/bin/env python3
"""Vendor libretro.h from the pinned Genesis Plus GX commit into sources/.

    tools/vendor-libretro-header.py            # fetch if the tree is not it
    tools/vendor-libretro-header.py --verify   # check the tree, write nothing

tash is built against the API header of the core it hosts, so this is Genesis
Plus GX's copy at a pinned commit, not RetroArch's moving one.  Both modes hash
against the pin below, so only a tree that is not it reaches the network.  The
file is copied byte for byte and never edited: its licence is inside it.
"""

import argparse
import hashlib
import pathlib
import sys
import urllib.request

PROJECT = 'libretro/Genesis-Plus-GX'

COMMIT = 'c2838c7dc4236fc2fe94e5dbd08b41486067918e'

# Under the core's own libretro/ port directory, not the usual libretro-common/.
MEMBER = 'libretro/libretro-common/include/libretro.h'

SHA256 = '3e05e66cdfa2ddb8e9ac4eee6bb38ff4d735ed82e8563ef116b710089c732512'

DESTINATION = 'sources/tash/libretro/libretro.h'


def Usage(problem: str) -> int:
  print(__doc__)
  print(f'vendor-libretro-header: {problem}', file=sys.stderr)
  return 2


def Repository() -> pathlib.Path:
  return pathlib.Path(__file__).resolve().parent.parent


def SourceUrl() -> str:
  return f'https://raw.githubusercontent.com/{PROJECT}/{COMMIT}/{MEMBER}'


def Digest(raw: bytes) -> str:
  return hashlib.sha256(raw).hexdigest()


def Checked(raw: bytes, where: str) -> bytes:
  """`raw`, or the mismatch named: everything downstream is these bytes."""
  digest = Digest(raw)
  if digest != SHA256:
    raise SystemExit(
      f'vendor-libretro-header: {where} hashes to {digest}, and this tool is '
      f'pinned to {SHA256}.  Either that is not {MEMBER} at {COMMIT[:12]}, or '
      'the file was edited here -- a vendored file is copied, never edited.')
  return raw


def Download() -> bytes:
  with urllib.request.urlopen(SourceUrl()) as answer:
    return answer.read()


def Verify(root: pathlib.Path) -> int:
  """The size of the vendored header, or the reason it is not the pin."""
  path = root / DESTINATION
  if not path.is_file():
    raise SystemExit(
      f'vendor-libretro-header: {DESTINATION} is missing -- run this tool '
      'without --verify to fetch it')
  return len(Checked(path.read_bytes(), str(path)))


def Write(root: pathlib.Path) -> tuple[int, bool]:
  path = root / DESTINATION
  if path.is_file() and Digest(path.read_bytes()) == SHA256:
    return path.stat().st_size, False
  raw = Checked(Download(), SourceUrl())
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_bytes(raw)
  return len(raw), True


def main(argv: list[str]) -> int:
  parse = argparse.ArgumentParser(add_help=False)
  parse.add_argument('--verify', action='store_true')
  parse.add_argument('-h', '--help', action='store_true')
  try:
    line = parse.parse_args(argv[1:])
  except SystemExit:
    return Usage('unreadable command line')
  if line.help:
    print(__doc__)
    return 0

  root = Repository()
  if line.verify:
    size = Verify(root)
    print(f'libretro.h  {size} bytes  sha256 {SHA256[:16]}…')
    print(f'vendor-libretro-header: {DESTINATION} is {PROJECT} {COMMIT[:12]}')
    return 0

  size, written = Write(root)
  print(f'libretro.h  {size} bytes  sha256 {SHA256[:16]}…')
  print(f'  {DESTINATION}  <- {PROJECT} {COMMIT[:12]}:{MEMBER}')
  print('vendor-libretro-header: '
        f'{"written" if written else "already the pinned header"}')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
