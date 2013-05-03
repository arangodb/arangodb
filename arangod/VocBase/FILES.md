Files writes by ArangoDB
========================

This document briefly describes which files are written by ArangoDB on
startup and shutdown, and what the role of these files is.

All files mentioned are placed in the vocbase database directory.

lock
====

A lock file containing the process id of a running ArangoDB.
The purpose of the lock file is to avoid starting multiple instances of ArangoDB
with the same database directory, which would potentially lead to inconsistencies.

The presence of the lock file is checked at ArangoDB startup, and ArangoDB will
not start if there is already a lock file. The lock file will be removed on 
clean shutdown, and also on unclean shutdown.

The lock file will be flocked on Linux, and thus can't be put into a filesystem
that doesn't support flock.

The lock file is used since ArangoDB 1.0.


VERSION
=======

A JSON file with version information for the server. 
The file will contain a JSON array with a "version" attribute. The version attribute
will contain the version number that ArangoDB was last started with.

It will also contain a "tasks" attribute with an array of all the tasks that are
or were already executed by the upgrade procedure in js/server/version-check.js.
Every successful upgrade task will be inserted into the "tasks" array with a value of
true. Every failed upgrade task will be inserted into the "tasks" array with a value
of false.
Failed upgrade tasks will get re-executed on server startup if the task is still
present in the js/server/version-check.js file.

The VERSION file will be created on the first start of ArangoDB if the database
directory is still empty.

The VERSION file is used since ArangoDB 1.1.


SERVER
======

A JSON file containing some basic information about the server.
It contains a "createdTime" attribute, with the information when the SERVER file was
first created.
It will also contain a "serverId" attribute, which is a randomly generated server id
that may be used for replication purposes in the future. The "serverId" is currently
(as of ArangoDB 1.3) not used.

The SERVER file is used since ArangoDB 1.3.


SHUTDOWN
========

A JSON file containing information about the last clean shutdown of the server.
If the server is shut down cleanly, the SHUTDOWN file is created. It will contain an
attribute "tick" with the value of the last tick the server used. It will also contain
a string with the datetime of the server shutdown. This can be used for informational
purposes.

On server startup, ArangoDB will look for the SHUTDOWN file and read it if present.
When present, the "tick" attribute will be adjust the server's tick value. This is a
shortcut that allows bypassing the scanning of all collection datafiles for tick values
at startup.

On startup, the SHUTDOWN file is removed before the server enters the normal
operation mode. That prevents using a stale SHUTDOWN file in case of a server crash.

In case the SHUTDOWN file is not there, ArangoDB will scan the latest datafiles
(journals) of collections for the latest tick values used.

The SHUTDOWN file is in use since ArangoDB 1.4.

