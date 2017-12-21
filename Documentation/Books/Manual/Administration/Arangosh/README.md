ArangoDB Shell Introduction
===========================

The ArangoDB shell (_arangosh_) is a command-line tool that can be used for
administration of ArangoDB, including running ad-hoc queries.

The _arangosh_ binary is shipped with ArangoDB. It offers a JavaScript shell
environment providing access to the ArangoDB server.
Arangosh can be invoked like this:

```
unix> arangosh
```

By default _arangosh_ will try to connect to an ArangoDB server running on
server *localhost* on port *8529*. It will use the username *root* and an
empty password by default. Additionally it will connect to the default database
(*_system*). All these defaults can be changed using the following 
command-line options:

* *--server.database <string>*: name of the database to connect to
* *--server.endpoint <string>*: endpoint to connect to
* *--server.username <string>*: database username
* *--server.password <string>*: password to use when connecting 
* *--server.authentication <bool>*: whether or not to use authentication

For example, to connect to an ArangoDB server on IP *192.168.173.13* on port
8530 with the user *foo* and using the database *test*, use:

    unix> arangosh  \
      --server.endpoint tcp://192.168.173.13:8530  \
      --server.username foo  \
      --server.database test  \
      --server.authentication true

_arangosh_ will then display a password prompt and try to connect to the 
server after the password was entered.

To change the current database after the connection has been made, you
can use the `db._useDatabase()` command in arangosh:

    @startDocuBlockInline shellUseDB
    @EXAMPLE_ARANGOSH_OUTPUT{shellUseDB}
    db._createDatabase("myapp");
    db._useDatabase("myapp");
    db._useDatabase("_system");
    db._dropDatabase("myapp");
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellUseDB

To get a list of available commands, arangosh provides a *help()* function.
Calling it will display helpful information.

_arangosh_ also provides auto-completion. Additional information on available 
commands and methods is thus provided by typing the first few letters of a
variable and then pressing the tab key. It is recommend to try this with entering
*db.* (without pressing return) and then pressing tab.

By the way, _arangosh_ provides the *db* object by default, and this object can
be used for switching to a different database and managing collections inside the
current database.

For a list of available methods for the *db* object, type 
    
    @startDocuBlockInline shellHelp
    @EXAMPLE_ARANGOSH_OUTPUT{shellHelp}
    db._help(); 
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellHelp

you can paste multiple lines into arangosh, given the first line ends with an
opening brace:

    @startDocuBlockInline shellPaste
    @EXAMPLE_ARANGOSH_OUTPUT{shellPaste}
    |for (var i = 0; i < 10; i ++) {
    |         require("@arangodb").print("Hello world " + i + "!\n");
    }
    @END_EXAMPLE_ARANGOSH_OUTPUT
    @endDocuBlock shellPaste


To load your own JavaScript code into the current JavaScript interpreter context,
use the load command:

    require("internal").load("/tmp/test.js")     // <- Linux / MacOS
    require("internal").load("c:\\tmp\\test.js") // <- Windows

Exiting arangosh can be done using the key combination ```<CTRL> + D``` or by
typing ```quit<CR>```

### Escaping

In AQL, escaping is done traditionally with the backslash character: `\`.
As seen above, this leads to double backslashes when specifying Windows paths.
Arangosh requires another level of escaping, also with the backslash character.
It adds up to four backslashes that need to be written in Arangosh for a single
literal backslash (`c:\tmp\test.js`):

    db._query('RETURN "c:\\\\tmp\\\\test.js"')

You can use [bind variables](../../../AQL/Invocation/WithArangosh.html) to
mitigate this:

    var somepath = "c:\\tmp\\test.js"
    db._query(aql`RETURN ${somepath}`)
