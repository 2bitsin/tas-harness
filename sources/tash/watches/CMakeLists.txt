Init_submodule()
Link_dependencies(
  utilities
  libretro      # search-script.cpp: the joypad button ids a hold step names
  session       # FrameObserver, and the memory the core hands over
  trace         # the writer a sampler fills with watch records
  oxbox::oxbox  # the reflect tag a profile's watch entries are read through
)
