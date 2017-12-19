Command-Line Options for Logging
================================

There are two different kinds of logs. Human-readable logs and machine-readable
logs. The human-readable logs are used to provide an administration with
information about the server. The machine-readable logs are used to provide
statistics about executed requests and timings about computation steps.

General Logging Options
-----------------------

### Logfile
`--log.file filename`

This option allows the user to specify the name of a file to which information
is logged. By default, if no log file is specified, the standard output is
used. Note that if the file named by *filename* does not exist, it will be
created. If the file cannot be created (e.g. due to missing file privileges),
the server will refuse to start. If the specified file already exists, output is
appended to that file.

Use *+* to log to standard error. Use *-* to log to standard output.
Use *""* to disable logging to file.

`--log.tty filename`

Be default, if started on a tty, the log output will also go to the ttyp.
Use *""* to disable.

### Request
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logRequests

Human Readable Logging
----------------------

### Logfiles
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logFile

### Level
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logLevel

### Local Time
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logLocalTime

### Line number
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logLineNumber

### Prefix
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logPrefix

### Thread
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logThread

### Source Filter
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logSourceFilter

### Content Filter
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logContentFilter

### Performance
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logPerformance

Machine Readable Logging
------------------------

### Application
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logApplication

### Facility
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock logFacility
