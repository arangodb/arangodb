!CHAPTER Getting familiar with ArangoDB

First of all download and install the corresponding RPM or Debian package or use
homebrew on the MacOS X. See the [installation manual](../Installing/README.md) for more details.
In case you just want to experiment with ArangoDB you
can use the [online](https://www.arangodb.com/tryitout) demo without
installing ArangoDB locally.

!SUBSECTION For Linux

* Visit the official [ArangoDB install page](https://www.arangodb.com/install)
  and download the correct package for your Linux distribution
* Install the package using your favorite package manager
* Start up the database server, normally this is done by
  executing */etc/init.d/arangod start*. The exact command
  depends on your Linux distribution

!SUBSECTION For MacOS X

* Execute *brew install arangodb*
* And start the server using */usr/local/sbin/arangod &*

!SUBSECTION For Microsoft Windows

* Visit the official [ArangoDB install page](https://www.arangodb.com/install)
  and download the installer for Windows
* Start up the database server

After these steps there should be a running instance of *_arangod_* -
the ArangoDB database server.

    unix> ps auxw | fgrep arangod
    arangodb 14536 0.1 0.6 5307264 23464 s002 S 1:21pm 0:00.18 /usr/local/sbin/arangod

If there is no such process, check the log file
*/var/log/arangodb/arangod.log* for errors. If you see a log message
like

    2012-12-03T11:35:29Z [12882] ERROR Database directory version (1) is lower than server version (1.2).
    2012-12-03T11:35:29Z [12882] ERROR It seems like you have upgraded the ArangoDB binary. If this is what you wanted to do, please restart with the --upgrade option to upgrade the data in the database directory.
    2012-12-03T11:35:29Z [12882] FATAL Database version check failed. Please start the server with the --upgrade option

make sure to start the server once with the *--upgrade* option.