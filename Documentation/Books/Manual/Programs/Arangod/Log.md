# ArangoDB Server Log Options

## Log levels and topics

ArangoDB's log output is grouped into topics. `--log.level` can be specified
multiple times at startup, for as many topics as needed. The log verbosity and
output files can be adjusted per log topic. For example

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

The available log levels are:

- `fatal`: only logs fatal errors
- `error`: only logs errors
- `warning`: only logs warnings and errors
- `info`: logs information messages, warnings and errors
- `debug`: logs debug and information messages, warnings and errors
- `trace`: logs trace, debug and information messages, warnings and errors

Note that levels `debug` and `trace` will be very verbose.

Some relevant log topics available in ArangoDB 3 are:

- `agency`: information about the agency
- `collector`: information about the WAL collector's state
- `compactor`: information about the collection datafile compactor
- `datafiles`: datafile-related operations
- `mmap`: information about memory-mapping operations (including msync)
- `performance`: performance-related messages
- `queries`: executed AQL queries, slow queries
- `replication`: replication-related info
- `requests`: HTTP requests
- `startup`: information about server startup and shutdown
- `threads`: information about threads

See more [log levels](../../../HTTP/AdministrationAndMonitoring/index.html#modify-and-return-the-current-server-log-level)

### Log outputs

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

### Forcing direct output

The option `--log.force-direct` can be used to disable logging in an extra
logging thread. If set to `true`, any log messages are immediately printed in the
thread that triggered the log message. This is non-optimal for performance but
can aid debugging. If set to `false`, log messages are handed off to an extra
logging thread, which asynchronously writes the log messages.

### Local time

Log dates and times in local time zone: `--log.use-local-time`

If specified, all dates and times in log messages will use the server's
local time-zone. If not specified, all dates and times in log messages
will be printed in UTC / Zulu time. The date and time format used in logs
is always `YYYY-MM-DD HH:MM:SS`, regardless of this setting. If UTC time
is used, a `Z` will be appended to indicate Zulu time.

### Escaping

`--log.escape value`

This option toggles the escaping of log output. 

If set to `true`, the following characters in the log output are escaped:

* the carriage return character (hex 0d)
* the newline character (hex 0a)
* the tabstop character (hex 09)
* any other characters with an ordinal value less than hex 20

If the option is set to `false`, no characters are escaped. Characters with
an ordinal value less than hex 20 will not be printed in this mode but will
be replaced with a space character (hex 20).

A side effect of turning off the escaping is that it will reduce the CPU 
overhead for the logging. However, this will only be noticeable when logging
is set to a very verbose level (e.g. debug or trace).

The default value for this option is `true`.

### Color logging

`--log.color value`

Logging to terminal output is by default colored. Colorful logging can be 
turned off by setting the value to false.

### Source file and Line number

Log line number: `--log.line-number`

Normally, if an human readable fatal, error, warning or info message is
logged, no information about the file and line number is provided. The
file and line number is only logged for debug and trace message. This option
can be use to always log these pieces of information.

### Prefix

Log prefix: `--log.prefix prefix`

This option is used specify an prefix to logged text.

### Threads

Log thread identifier: `--log.thread true`

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

To also log thread names, it is possible to set the `--log.thread-name`
option. By default `--log.thread-name` is set to `false`.

### Role

Log role: `--log.role true`

When set to `true`, this option will make the ArangoDB logger print a single 
character with the server's role into each logged message. The roles are: 
  
- U: undefined/unclear (used at startup)
- S: single server
- C: coordinator
- P: primary
- A: agent

The default value for this option is `false`, so no roles will be logged. 
