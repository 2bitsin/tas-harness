Init_submodule()
Link_dependencies(
  utilities        # Result: a tape that will not play is an answer, not a throw
  bus              # FrameView: what an anchor is asked about
  perception       # the hashes and the template match an anchor is made of
  session          # Session: the player steps it and holds its pads
  trace            # player.test.cpp: the records a held pad leaves
  recorder         # WritePng: the anchor crop written beside the tape
  oxbox::oxbox     # the yaml a tape is, and `tash tape check`
  opencv_core      # anchor-image.cpp: the Mat imread fills
  opencv_imgproc   # anchor-image.cpp: cvtColor back into rgb565
  opencv_imgcodecs # anchor-image.cpp: imread
)
