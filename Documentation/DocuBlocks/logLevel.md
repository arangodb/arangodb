

@brief log level
`--log.level level`

`--log level`

Allows the user to choose the level of information which is logged by the
server. The argument *level* is specified as a string and can be one of
the values listed below. Note that, fatal errors, that is, errors which
cause the server to terminate, are always logged irrespective of the log
level assigned by the user. The variant *c* log.level can be used in
configuration files, the variant *c* log for command line options.

**fatal**:
Logs errors which cause the server to terminate.

Fatal errors generally indicate some inconsistency with the manner in
which
the server has been coded. Fatal errors may also indicate a problem with
the
platform on which the server is running. Fatal errors always cause the
server to terminate. For example,

```
2010-09-20T07:32:12Z [4742] FATAL a http server has already been created
```

**error**:
Logs errors which the server has encountered.

These errors may not necessarily result in the termination of the
server. For example,

```
2010-09-17T13:10:22Z [13967] ERROR strange log level 'errors'\, going to
'warning'
```

**warning**:
Provides information on errors encountered by the server,
which are not necessarily detrimental to it's continued operation.

For example,

```
2010-09-20T08:15:26Z [5533] WARNING got corrupted HTTP request 'POS?'
```

**Note**: The setting the log level to warning will also result in all
errors
to be logged as well.

**info**:
Logs information about the status of the server.

For example,

```
2010-09-20T07:40:38Z [4998] INFO SimpleVOC ready for business
```

**Note**: The setting the log level to info will also result in all errors
and
warnings to be logged as well.

**debug**:
Logs all errors, all warnings and debug information.

Debug log information is generally useful to find out the state of the
server in the case of an error. For example,

```
2010-09-17T13:02:53Z [13783] DEBUG opened port 7000 for any
```

**Note**: The setting the log level to debug will also result in all
errors,
warnings and server status information to be logged as well.

**trace**:
As the name suggests, logs information which may be useful to trace
problems encountered with using the server.

For example,

```
2010-09-20T08:23:12Z [5687] TRACE trying to open port 8000
```

**Note**: The setting the log level to trace will also result in all
errors,
warnings, status information, and debug information to be logged as well.

