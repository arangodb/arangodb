!CHAPTER Command-Line Options for Logging 

!SUBSECTION Log levels and topics

ArangoDB's log output is grouped into topics. `--log.level` can be specified
multiple times at startup, for as many topics as needed. The log verbosity
and output files can be adjusted per log topic. For example

```
--log.level startup=trace --log.level queries=trace --log.level info
```

will log messages concerning startup at trace level, AQL queries at trace level
and everything else at info level.

In a configuration file, it is written like this:

```
[log]
level = startup=trace
level = queries=trace
level = info
```

Note that there must not be any whitespace around the second `=`.

The available log levels are:

- `fatal`: only logs fatal errors
- `error`: only logs errors
- `warning`: only logs warnings and errors
- `info`: logs information messages, warnings and errors
- `debug`: logs debug and information messages, warnings and errors
- `trace`: logs trace, debug and information messages, warnings and errors

Note that levels `debug` and `trace` will be very verbose.

Some relevant log topics available in 3.0 are:

- `collector`: information about the WAL collector's state
- `compactor`: information about the collection datafile compactor
- `datafiles`: datafile-related operations
- `mmap`: information about memory-mapping operations (including msync)
- `performance`: performance-releated messages
- `queries`: executed AQL queries, slow queries
- `replication`: replication-related info
- `requests`: HTTP requests
- `startup`: information about server startup and shutdown
- `threads`: information about threads

The old option `--log.performance` is still available in 3.0. It is now a shortcut
for the more general option `--log.level performance=trace`.

!SUBSECTION Log outputs

The log option `--log.output <definition>` allows directing the global
or per-topic log output to different outputs. The output definition `<definition>`
can be one of

- `-` for stdin
- `+` for stderr
- `syslog://<syslog-facility>`
- `syslog://<syslog-facility>/<application-name>`
- `file://<relative-path>`

The option can be specified multiple times in order to configure the output
for different log topics. To set up a per-topic output configuration, use
`--log.output <topic>=<definition>`, e.g.

  queries=file://queries.txt

logs all queries to the file "queries.txt".

The old option `--log.file` is still available in 3.0 for convenience reasons. In
3.0 it is a shortcut for the more general option `--log.output file://filename`.

The old option `--log.requests-file` is still available in 3.0. It is now a shortcut
for the more general option `--log.output requests=file://...`.

Using `--log.output` also allows directing log output to different files based on 
topics. For example, to log all AQL queries to a file "queries.log" one can use the 
options:

```
--log.level queries=trace --log.output queries=file:///path/to/queries.log
```

To additionally log HTTP request to a file named "requests.log" add the options:

```
--log.level requests=info --log.output requests=file:///path/to/requests.log
```

!SUBSECTION Forcing direct output

The option `--log.force-direct` can be used to disable logging in an extra
logging thread. If set to `true`, any log messages are immediately printed in the
thread that triggered the log message. This is non-optimal for performance but
can aid debugging. If set to `false`, log messages are handed off to an extra
logging thread, which asynchronously writes the log messages.

!SUBSECTION Local time

Log dates and times in local time zone: `--log.use-local-time`

If specified, all dates and times in log messages will use the server's
local time-zone. If not specified, all dates and times in log messages
will be printed in UTC / Zulu time. The date and time format used in logs
is always `YYYY-MM-DD HH:MM:SS`, regardless of this setting. If UTC time
is used, a `Z` will be appended to indicate Zulu time.

!SUBSECTION Line number

Log line number: `--log.line-number`

Normally, if an human readable fatal, error, warning or info message is
logged, no information about the file and line number is provided. The
file and line number is only logged for debug and trace message. This option
can be use to always log these pieces of information.

!SUBSECTION Prefix

Log prefix: `--log.prefix prefix`

This option is used specify an prefix to logged text.

!SUBSECTION Thread

Log thread identifier: `--log.thread`

Whenever log output is generated, the process ID is written as part of the
log information. Setting this option appends the thread id of the calling
thread to the process id. For example,

```
2010-09-20T13:04:01Z [19355] INFO ready for business
```

when no thread is logged and

```
2010-09-20T13:04:17Z [19371-18446744072487317056] ready for business
```

when this command line option is set.
