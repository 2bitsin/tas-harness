Init_submodule()
Link_dependencies(
  utilities
  bus
  perception       # the hashes, the change amount and the template match
  plan             # tash.plan: the grid searches a scenario plans over
  session          # the run a scenario drives
  recorder         # the shots look() writes, the bundle they land in
  report           # report(): the bundle's page, mid-run
  tape             # play(): a tape through the same player the cli uses
  trace            # the writer a mark and a verdict go through
  watches          # the values observe() reports by name
  pybind11::embed  # the cli embeds an interpreter with these bindings in it
  Python3::Python
)
