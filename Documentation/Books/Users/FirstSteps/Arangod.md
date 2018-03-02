Details about the ArangoDB Server
=================================

The ArangoDB database server has two modes of operation: As a server, where it
will answer to client requests and as an emergency console, in which you can
access the database directly. The latter - as the name suggests - should 
only be used in case of an emergency, for example, a corrupted
collection. Using the emergency console allows you to issue all commands
normally available in actions and transactions. When starting the server in
emergency console mode, the server cannot handle any client requests.

You should never start more than one server using the same database directory,
independent from the mode of operation. Normally ArangoDB will prevent
you from doing this by placing a lockfile in the database directory and
not allowing a second ArangoDB instance to use the same database directory
if a lockfile is already present.

The following command starts the ArangoDB database in server mode. You will
be able to access the server using HTTP requests on port 8529. Look 
[here](#frequently-used-options) for a list of 
frequently used options – see 
[here](../ConfigureArango/README.md) for a complete list.

```
unix> /usr/local/sbin/arangod /tmp/vocbase
20ZZ-XX-YYT12:37:08Z [8145] INFO using built-in JavaScript startup files
20ZZ-XX-YYT12:37:08Z [8145] INFO ArangoDB (version 1.x.y) is ready for business
20ZZ-XX-YYT12:37:08Z [8145] INFO Have Fun!
```

After starting the server, point your favorite browser to:

    http://localhost:8529/

to access the administration front-end.

Linux
-----

To start the server at system boot time you should use one of the 
pre-rolled packages that will install the necessary start / stop
scripts for ArangoDB. You can use the start script as follows:

    unix> /etc/init.d/arangod start
 
To stop the server you can use the following command:

    unix> /etc/init.d/arangod stop

You may require root privileges to execute these commands.

If you compiled ArangoDB from source and did not use any installation
package – or using non-default locations and/or multiple ArangoDB
instances on the same host – you may want to start the server process 
manually. You can do so by invoking the arangod binary from the command
line as shown before. To stop the database server gracefully, you can
either press CTRL-C or by send the SIGINT signal to the server process. 
On many systems this can be achieved with the following command:

    unix> kill -2 `pidof arangod`

Frequently Used Options
-----------------------

The following command-line options are frequently used. 
[For a full list of options see the documentation](../ConfigureArango/README.md).

`database-directory`

Uses the "database-directory" as base directory. There is an
alternative version available for use in configuration files, see 
[configuration documentation](../ConfigureArango/Arangod.md).

`--help`<br >
`-h`

Prints a list of the most common options available and then exists. 
In order to see all options use `--help-all`.

`--log level`

Allows the user to choose the level of information which is logged by
the server. The "level" is specified as a string and can be one of
the following values: fatal, error, warning, info, debug or trace.  For
more information see [here](../ConfigureArango/Logging.md).

<!-- ArangoServer.h -->

@startDocuBlock server_authentication

<!-- ApplicationEndpointServer.h -->

@startDocuBlock keep_alive_timeout

<!-- ApplicationEndpointServer.h -->

@startDocuBlock serverEndpoint

`--daemon`

Runs the server as a "daemon" (as a background process).

### Troubleshooting

If the ArangoDB server does not start or if you cannot connect to it 
using *arangosh* or other clients, you can try to find the problem cause by 
executing the following steps. If the server starts up without problems
you can skip this section.

* *Check the server log file*: If the server has written a log file you should 
  check it because it might contain relevant error context information.

* *Check the configuration*: The server looks for a configuration file 
  named *arangod.conf* on startup. The contents of this file will be used
  as a base configuration that can optionally be overridden with command-line 
  configuration parameters. You should check the config file for the most
  relevant parameters such as:
  * *server.endpoint*: What IP address and port to bind to
  * *log parameters*: If and where to log
  * *database.directory*: Path the database files are stored in

  If the configuration reveals that something is not configured right the config
  file should be adjusted and the server be restarted.

* *Start the server manually and check its output*: Starting the server might
  fail even before logging is activated so the server will not produce log
  output. This can happen if the server is configured to write the logs to
  a file that the server has no permissions on. In this case the server 
  cannot log an error to the specified log file but will write a startup 
  error on stderr instead.
  Starting the server manually will also allow you to override specific 
  configuration options, e.g. to turn on/off file or screen logging etc.

* *Check the TCP port*: If the server starts up but does not accept any incoming 
  connections this might be due to firewall configuration between the server 
  and any client(s). The server by default will listen on TCP port 8529. Please 
  make sure this port is actually accessible by other clients if you plan to use 
  ArangoDB in a network setup.

  When using hostnames in the configuration or when connecting, please make
  sure the hostname is actually resolvable. Resolving hostnames might invoke
  DNS, which can be a source of errors on its own.

  It is generally good advice to not use DNS when specifying the endpoints
  and connection addresses. Using IP addresses instead will rule out DNS as 
  a source of errors. Another alternative is to use a hostname specified
  in the local */etc/hosts* file, which will then bypass DNS.

* *Test if *curl* can connect*: Once the server is started, you can quickly
  verify if it responds to requests at all. This check allows you to
  determine whether connection errors are client-specific or not. If at 
  least one client can connect, it is likely that connection problems of
  other clients are not due to ArangoDB's configuration but due to client
  or in-between network configurations.

  You can test connectivity using a simple command such as:

  **curl --dump - -X GET http://127.0.0.1:8529/_api/version && echo**

  This should return a response with an *HTTP 200* status code when the
  server is running. If it does it also means the server is generally 
  accepting connections. Alternative tools to check connectivity are *lynx*
  or *ab*.
