# tash/trace/

What a run leaves behind: a header and one record per event, written by
`Writer`, read by `Reader`, folded into the line the run kept by `Line`.
`format.hpp` holds the layout and the version.

## `tash trace dump`

    tash trace dump <file>

One line per record in the order the file holds them, then a `#` line for
what the reader makes of the file itself. A restore prints ahead of the
frames it restored to: its leading column is the harness frame it was made
at while `to line frame N` counts the line the run keeps, two different
units, and a frame record waits on its hashes before it is written, so the
leading column is not in order around a restore.
