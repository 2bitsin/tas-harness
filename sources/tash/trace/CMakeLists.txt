Init_submodule()
Link_dependencies(
  utilities        # the refusal grammar in outcome.hpp
  oxbox::oxbox     # the cli Command tree, and serdes for the byte order
  SQLite::SQLite3  # sqlite-export.cpp
)
