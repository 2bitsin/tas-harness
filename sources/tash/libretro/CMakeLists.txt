Init_submodule()
Link_dependencies(
  utilities        # Result: a core that will not load is an answer, not a throw
  ${CMAKE_DL_LIBS} # shared-library.cpp: dlopen of the core
)
