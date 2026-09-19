Init_submodule()
Link_dependencies(
  utilities       # Result: bad pixels are an answer, not a throw
  bus             # FrameView: the shape the video callback delivers
  xxHash::xxhash  # frame-hash.cpp: XXH3_64bits, the exact per-frame digest
  opencv_core     # the grey Mat, the DCT, absdiff
  opencv_imgproc  # cvtColor, resize, matchTemplate
)
