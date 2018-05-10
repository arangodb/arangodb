Arangosh Examples
=================

By default _arangosh_ will try to connect to an ArangoDB server running on
server *localhost* on port *8529*. It will use the username *root* and an
empty password by default. Additionally it will connect to the default database
(*_system*). All these defaults can be changed using the following 
command-line options:

- *--server.database <string>*: name of the database to connect to
- *--server.endpoint <string>*: endpoint to connect to
- *--server.username <string>*: database username
- *--server.password <string>*: password to use when connecting 
- *--server.authentication <bool>*: whether or not to use authentication

For example, to connect to an ArangoDB server on IP *192.168.173.13* on port
8530 with the user *foo* and using the database *test*, use:

    arangosh --server.endpoint tcp://192.168.173.13:8530 --server.username foo --server.database test --server.authentication true

_arangosh_ will then display a password prompt and try to connect to the 
server after the password was entered.

The shell will print its own version number and if successfully connected
to a server the version number of the ArangoDB server.

To change the current database after the connection has been made, you
can use the `db._useDatabase()` command in Arangosh:

    @startDocuBlockInline shellUseDB
    @EXAMPLE_ARANGOSH_OUTPUT{shellUseDB}
    db._createDatabase("myapp");
    db._useDatabase("myapp");
    db._useDatabase("_system");
    db._dropDatabase("myapp");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellUseDB

To get a list of available commands, Arangosh provides a *help()* function.
Calling it will display helpful information.

_arangosh_ also provides auto-completion. Additional information on available 
commands and methods is thus provided by typing the first few letters of a
variable and then pressing the tab key. It is recommend to try this with entering
*db.* (without pressing return) and then pressing tab.
