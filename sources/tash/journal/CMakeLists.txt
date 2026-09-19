Init_submodule()
Link_dependencies(
  utilities
  bus
  perception  # the hash worker: hashes and the change amount, off the step
  session     # FrameObserver: what the session calls on every frame
  trace       # the writer this fills with frame records
)
