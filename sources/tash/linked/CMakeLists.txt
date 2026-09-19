Init_submodule()
# Nothing of tash's is linked: a target takes this component alone, so the
# only library here is the digest the harness hashes a frame with. The three
# headers it includes -- trace/format.hpp, tape/names.hpp, clock and
# utilities' constants -- are declarations of the formats it writes, and
# carry no symbols.
Link_dependencies(
  xxHash::xxhash   # _recording.cpp: XXH3_64bits, the frame's exact digest
  TEST utilities   # the scratch area a test records into
  TEST perception  # the digest the harness computes for the same frame
  TEST tape        # the tape it wrote, read back by the reader that plays one
  TEST trace       # the same, for the records
  TEST recorder    # the bundle a run.yaml makes the directory into
)
