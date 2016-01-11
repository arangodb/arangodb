

@brief performance logging
`--log.performance`

If this option is set, performance-related info messages will be logged
via
the regular logging mechanisms. These will consist of mostly timing and
debugging information for performance-critical operations.

Currently performance-related operations are logged as INFO messages.
Messages starting with prefix `[action]` indicate that an instrumented
operation was started (note that its end won't be logged). Messages with
prefix `[timer]` will contain timing information for operations. Note that
no timing information will be logged for operations taking less time than
1 second. This is to ensure that sub-second operations do not pollute
logs.

The contents of performance-related log messages enabled by this option
are subject to change in future versions of ArangoDB.

