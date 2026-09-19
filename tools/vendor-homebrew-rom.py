#!/usr/bin/env python3
"""Vendor the freely licensed Genesis ROM the tests run without a ROM share.

    tools/vendor-homebrew-rom.py            # fetch if the tree is not it
    tools/vendor-homebrew-rom.py --verify   # check the tree, write nothing

Zsenilia is a 256 KiB Mega Drive demo, LGPL-3.0, whose repository ships the
built ROM; it draws a moving picture from its first frames, which is what a
determinism test needs and a title screen does not give.  The ROM is committed
so CI needs no network, and its licence travels beside it.
"""

import argparse
import hashlib
import pathlib
import sys
import urllib.request

PROJECT = 'ResistanceVault/demo-Zsenilia'

COMMIT = '9e035b7d5361f308bdfe8809cbc5068f3221a821'

# member, where it lands, sha256 of the bytes at COMMIT.
VENDORED = (
  ('release/ZSENILIA-BY-RSE-2015.bin', 'examples/homebrew/zsenilia.bin',
   '5c4fa4d3b16fe94c9feb707e3cbb3ff6de04380c277dfa6155419228f887c988'),
  ('LICENSE', 'examples/homebrew/zsenilia-license.txt',
   'da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768'),
)


def Usage(problem: str) -> int:
  print(__doc__)
  print(f'vendor-homebrew-rom: {problem}', file=sys.stderr)
  return 2


def Repository() -> pathlib.Path:
  return pathlib.Path(__file__).resolve().parent.parent


def SourceUrl(member: str) -> str:
  return f'https://raw.githubusercontent.com/{PROJECT}/{COMMIT}/{member}'


def Digest(raw: bytes) -> str:
  return hashlib.sha256(raw).hexdigest()


def Checked(raw: bytes, where: str, member: str, pinned: str) -> bytes:
  """`raw`, or the mismatch named: everything downstream is these bytes."""
  digest = Digest(raw)
  if digest != pinned:
    raise SystemExit(
      f'vendor-homebrew-rom: {where} hashes to {digest}, and this tool is '
      f'pinned to {pinned}.  Either that is not {member} at {COMMIT[:12]}, or '
      'the file was edited here -- a vendored file is copied, never edited.')
  return raw


def Download(member: str) -> bytes:
  with urllib.request.urlopen(SourceUrl(member)) as answer:
    return answer.read()


def Verify(root: pathlib.Path) -> list[int]:
  sizes = []
  for member, destination, pinned in VENDORED:
    path = root / destination
    if not path.is_file():
      raise SystemExit(
        f'vendor-homebrew-rom: {destination} is missing -- run this tool '
        'without --verify to fetch it')
    sizes.append(len(Checked(path.read_bytes(), str(path), member, pinned)))
  return sizes


def Write(root: pathlib.Path) -> list[tuple[int, bool]]:
  written = []
  for member, destination, pinned in VENDORED:
    path = root / destination
    if path.is_file() and Digest(path.read_bytes()) == pinned:
      written.append((path.stat().st_size, False))
      continue
    raw = Checked(Download(member), SourceUrl(member), member, pinned)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(raw)
    written.append((len(raw), True))
  return written


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
    for (_, destination, pinned), size in zip(VENDORED, Verify(root)):
      print(f'{destination}  {size} bytes  sha256 {pinned[:16]}…')
    print(f'vendor-homebrew-rom: the tree is {PROJECT} {COMMIT[:12]}')
    return 0

  for (member, destination, _), (size, fetched) in zip(VENDORED, Write(root)):
    print(f'{destination}  {size} bytes  <- {member}'
          f'{"" if fetched else "  (already the pinned bytes)"}')
  print(f'vendor-homebrew-rom: {PROJECT} {COMMIT[:12]}')
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
