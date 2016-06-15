!CHAPTER Arangod

If the ArangoDB server does not start or if you cannot connect to it 
using *arangosh* or other clients, you can try to find the problem cause by 
executing the following steps. If the server starts up without problems
you can skip this section.

- *Check the server log file*: If the server has written a log file you should 
  check it because it might contain relevant error context information.

- *Check the configuration*: The server looks for a configuration file 
  named *arangod.conf* on startup. The contents of this file will be used
  as a base configuration that can optionally be overridden with command-line 
  configuration parameters. You should check the config file for the most
  relevant parameters such as:
  - *server.endpoint*: What IP address and port to bind to
  - *log parameters*: If and where to log
  - *database.directory*: Path the database files are stored in

  If the configuration reveals that something is not configured right the config
  file should be adjusted and the server be restarted.

- *Start the server manually and check its output*: Starting the server might
  fail even before logging is activated so the server will not produce log
  output. This can happen if the server is configured to write the logs to
  a file that the server has no permissions on. In this case the server 
  cannot log an error to the specified log file but will write a startup 
  error on stderr instead.
  Starting the server manually will also allow you to override specific 
  configuration options, e.g. to turn on/off file or screen logging etc.

- *Check the TCP port*: If the server starts up but does not accept any incoming 
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

- *Test if *curl* can connect*: Once the server is started, you can quickly
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
