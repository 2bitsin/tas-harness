Init_submodule()
Link_dependencies(
  utilities
  bus
  perception      # the region a look or a shot is cropped to
  recorder        # look(): one encoder, straight to bytes
  session
  tape            # the anchor grammar run_until, anchor and expect answer in
  trace
  python          # ScenarioRun: the run object every tool maps onto
  oxbox::oxbox    # json on the wire, and the http server underneath it
)
