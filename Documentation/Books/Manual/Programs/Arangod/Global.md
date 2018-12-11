# ArangoDB Server Global Options

## General help

`--help`

`-h`

Prints a list of the most common options available and then exits. In order to
see all options use *--help-all*.

To receive the startup options in JSON format, pass the `--dump-options` flag. This will
print out all options and exit.

## Version

`--version`

`-v`

Prints the version of the server and exits.

## Daemon

`--daemon`

Runs the server as a daemon (as a background process). This parameter can only
be set if the pid (process id) file is specified. That is, unless a value to the
parameter pid-file is given, then the server will report an error and exit.

## Default Language

`--default-language default-language`

The default language ist used for sorting and comparing strings.  The language
value is a two-letter language code (ISO-639) or it is composed by a two-letter
language code with and a two letter country code (ISO-3166). Valid languages are
"de", "en", "en_US" or "en_UK".

The default default-language is set to be the system locale on that platform.

## Supervisor

`--supervisor`

Executes the server in supervisor mode. In the event that the server
unexpectedly terminates due to an internal error, the supervisor will
automatically restart the server. Setting this flag automatically implies that
the server will run as a daemon. Note that, as with the daemon flag, this flag
requires that the pid-file parameter will set.

```js
unix> ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
2012-06-27T15:58:28Z [10133] INFO starting up in supervisor mode
```

As can be seen (e.g. by executing the ps command), this will start a supervisor
process and the actual database process:

```js
unix> ps fax | grep arangod
10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
10142 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
```

When the database process terminates unexpectedly, the supervisor process will
start up a new database process:

```
> kill -SIGSEGV 10142

> ps fax | grep arangod
10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
10168 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
```

## User identity

`--uid uid`

The name (identity) of the user the server will run as. If this parameter is not
specified, the server will not attempt to change its UID, so that the UID used
by the server will be the same as the UID of the user who started the server. If
this parameter is specified, then the server will change its UID after opening
ports and reading configuration files, but before accepting connections or
opening other files (such as recovery files).  This is useful when the server
must be started with raised privileges (in certain environments) but security
considerations require that these privileges be dropped once the server has
started work.

Observe that this parameter cannot be used to bypass operating system
security. In general, this parameter (and its corresponding relative gid) can
lower privileges but not raise them.


## Group identity

`--gid gid`

The name (identity) of the group the server will run as. If this parameter is
not specified, then the server will not attempt to change its GID, so that the
GID the server runs as will be the primary group of the user who started the
server. If this parameter is specified, then the server will change its GID
after opening ports and reading configuration files, but before accepting
connections or opening other files (such as recovery files).

This parameter is related to the parameter uid.


## Process identity

`--pid-file filename`

The name of the process ID file to use when running the server as a
daemon. This parameter must be specified if either the flag *daemon* or
*supervisor* is set.

## Console

`--console`

Runs the server in an exclusive emergency console mode. When
starting the server with this option, the server is started with
an interactive JavaScript emergency console, with all networking
and HTTP interfaces of the server disabled.

No requests can be made to the server in this mode, and the only
way to work with the server in this mode is by using the emergency
console.
Note that the server cannot be started in this mode if it is
already running in this or another mode.
