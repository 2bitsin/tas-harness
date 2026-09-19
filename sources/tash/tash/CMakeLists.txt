Init_submodule()
Link_dependencies(
  utilities
  bus
  clock
  libretro
  journal     # --trace: one frame record per frame
  session
  tape            # --tape, and `tash tape check`: what a tape will do
  trace
  watches         # the profile's watches, and the search mem-command drives
  python          # --scenario: the bindings, embedded in this interpreter
  mcp             # `tash serve` and `tash session`: the tools over http
  recorder        # the bundle, its video and the --shot it leaves behind
  report          # report.html, at the end of a run and on demand
  oxbox::oxbox    # the cli, and the yaml a profile is read from
  ffmpeg::avformat  # bundle-recording.test.cpp: reopening what was recorded
)
