Init_submodule()
Link_dependencies(
  utilities
  bus
  clock
  libretro
  perception   # session.test.cpp: ExactHash, the digest two runs compare
  trace        # the pads it is held by and the states it goes back to
  libzip::zip  # rom-file.cpp: a Genesis ROM ships zipped
)
