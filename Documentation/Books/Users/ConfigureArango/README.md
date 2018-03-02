Command-line options
====================
### Configuration Files

Options can be specified on the command line or in configuration files. If a
string *Variable* occurs in the value, it is replaced by the corresponding
environment variable.

General Options
---------------

### General help
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock generalHelp

### Version
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock generalVersion
 
### Upgrade
`--upgrade`

Specifying this option will make the server perform a database upgrade at start. A database upgrade will first compare the version number stored in the file VERSION in the database directory with the current server version.

If the two version numbers match, the server will start normally.

If the version number found in the database directory is higher than the version number the server is running, the server expects this is an unintentional downgrade and will warn about this. It will however start normally. Using the server in these conditions is however not recommended nor supported.

If the version number found in the database directory is lower than the version number the server is running, the server will check whether there are any upgrade tasks to perform. It will then execute all required upgrade tasks and print their statuses. If one of the upgrade tasks fails, the server will exit and refuse to start. Re-starting the server with the upgrade option will then again trigger the upgrade check and execution until the problem is fixed. If all tasks are finished, the server will start normally.

Whether or not this option is specified, the server will always perform a version check on startup. Running the server with a non-matching version number in the VERSION file will make the server refuse to start.

### Configuration
<!-- lib/ApplicationServer/ApplicationServer.h -->
@startDocuBlock configurationFilename

### Daemon
`--daemon`

Runs the server as a daemon (as a background process). This parameter can only
be set if the pid (process id) file is specified. That is, unless a value to the
parameter pid-file is given, then the server will report an error and exit.

### Default Language
<!-- arangod/RestServer/ArangoServer.h -->
@startDocuBlock DefaultLanguage

### Supervisor
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

### User identity

<!-- lib/ApplicationServer/ApplicationServer.h -->

@startDocuBlock configurationUid

### Group identity

<!-- lib/ApplicationServer/ApplicationServer.h -->

@startDocuBlock configurationGid

### Process identity

<!-- lib/Rest/AnyServer.h -->

@startDocuBlock pidFile

### Console
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


### Random Generator
@startDocuBlock randomGenerator