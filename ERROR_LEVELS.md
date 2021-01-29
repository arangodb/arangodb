# Error Levels

In this section we define and describe the meaning of the error levels
in ArangoDB's log messages. The error levels are, from most to least severe:

  - FATAL
  - ERROR
  - WARN
  - INFO
  - DEBUG
  - TRACE

For each log topic one can configure the lowest level which is actually logged.
For example, if one sets the error level to `ERROR` for some log topic,
one only sees messages of level `ERROR` and above (`ERROR` and `FATAL`).

## FATAL

_Fatal_ errors are the most severe errors and only occur if a service or application
can not recover safely from an abnormal state, which forces it to shut down.

Typically, a fatal error only occurs once in the process lifetime,
so if the log file is tied to the process, this is typically
the last message in the log. There might be a few exceptions to this
rule, where it makes more sense to keep the server running, for example
to be able to diagnose the problem better.

We reserve this error type for the following events:

- crucial files/folders are missing or inaccessible during startup
- overall application or system failure with a serious danger of
  data corruption or loss (the following shutdown is intended to prevent
  possible or further data loss)

**Recommendation**:
Fatal errors should be investigated immediately by a system administrator.

**Note that**:
Alerts will be raised and people will be called at night for this type of
error.

## ERROR

If a problem is encountered which is fatal to some operation, but not for
the service or the application as a whole, then an _error_ is logged.

Reasons for log entries of this severity are for example:

- missing data
- a required file can't be opened
- incorrect connection strings
- missing services

If some operation is automatically retried and eventually succeeds,
no error will be written to the log. Therefore, if an error is logged then
it should be taken seriously as it may require user intervention to solve.

Note that in any distributed system, temporary failures of network connections
or certain servers or services can and will happen. Most systems will tolerate
such failures and retry for some time, but will eventually run out of patience,
give up and fail the operation one level up.

**Recommendation**:
A system administrator should be notified automatically to investigate the error.
By filtering the log to look at errors (and other logged events above)
one can determine the error frequency and quickly identify the initial failure
that might have resulted in a cascade of additional errors.

**Note that**:
Alerts will be raised and people will be called at night for this type of
error.

## WARN

A _warning_ is triggered by anything that can potentially cause
application oddities, but from which the system recovers automatically.

Examples of events which lead to warnings:

- switching from a primary to backup server
- retrying an operation
- missing secondary data
- things running inefficiently
  (in particular slow queries and bad system settings)
  
Certain warnings are logged at startup time only, such as startup option
values which lie outside the recommended range.

These _might_ be problems, or might not. For example, expected transient
environmental conditions such as short loss of network or database
connectivity are logged as warnings, not errors. Viewing a log filtered
to show only warnings and errors may give quick insight into early
hints at the root cause of subsequent errors.

**Recommendation**:
Can mostly be ignored but can give hints for inefficiencies or
future problems.

## INFO

_Info_ messages are generally useful information to log to better
understand what state the system is in. One will usually want to
have info messages available but does usually not care about them
under normal circumstances.

Informative messages are logged in events like:

- successful initialization
- services starting or stopping
- successful completion of significant transactions
- configuration assumptions

Viewing log entries of severity _info_ and above should give a quick overview
of major state changes in the process providing top-level context for
understanding any warnings or errors that also occur. Logging info level
messages and above will usually not spam anything beyond good readability.

**Recommendation**:
Usually good to have, but one does not have to look at _info_ level messages
under normal circumstances.

## DEBUG

Information which is helpful to ArangoDB developers as well as to other
people like system administrators to diagnose an application or service
is logged as _debug_ message.

Debug messages make software much more maintainable but require some
diligence because the value of individual debug statements may change
over time as programs evolve. The best way to achieve this is by getting
the development team in the habit of regularly reviewing logs as a standard
part of troubleshooting reported issues. We encourage our teams to
prune out messages that no longer provide useful context and to add
messages where needed to understand the context of subsequent messages.

**Recommendation**:
_Debug_ level messages are usually switched off, but one can switch them on
to investigate problems.

## TRACE

_Trace_ messages produce a lot of output and are usually only needed by
ArangoDB developers to debug problems in the source code.

**Recommendation**:
_Trace_ level logging should generally stay disabled.
