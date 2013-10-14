The ArangoDB Shell {#UserManualArangosh}
========================================

@NAVIGATE_UserManualArangosh
@EMBEDTOC{UserManualArangoshTOC}

ArangoDB Shell Introduction {#UserManualArangoshIntro}
======================================================

The ArangoDB shell (_arangosh_) is a command-line tool that can be used for
administration of ArangoDB, including running ad-hoc queries.

The _arangosh_ binary is shipped with ArangoDB, and can be invoked like so:

    unix> arangosh
    
By default, arangosh will try to connect to an ArangoDB server running on
server `localhost` on port `8529`. It will use the username `root` and an
empty password by default. Additionally, it will connect to the default database
(`_system`). All these defaults can be changed using the following 
command-line options:

- `--server.database <string>`: name of the database to connect to
- `--server.endpoint <string>`: endpoint to connect to
- `--server.username <string>`: username
- `--server.password <string>`: password to use (omit this and you'll be prompted for the
  password)

For example, to connect to an ArangoDB server on IP `192.168.173.13` on port
8530 with the user `foo` and using the database `test`, use

    unix> arangosh  \
      --server.endpoint tcp://192.168.173.13:8530  \
      --server.username foo  \
      --server.database test  \
      --server.disable-authentication false

arangosh will then display a password prompt and try to connect to the 
server after the password was entered.

The change the current database after the connection has been made, you
can use the `db._useDatabase()` command in arangosh:

    arangosh> db._useDatabase("myapp");

To get a list of available commands, arangosh provides a `help()` function.
Calling it will display helpful information.

arangosh also provides auto-completion. Additional information on available 
commands and methods is thus provided by typing the first few letters of a
variable and then pressing the tab key. It is recommend to try this with entering
`db.` (without pressing return) and then pressing tab.


ArangoDB Shell Output {#UserManualArangoshOutput}
=================================================

In general the ArangoDB shell prints its as output to standard output channel
using the JSON stringifier.

    arangosh> db.five.toArray();
    [{ "_id" : "five/3665447", "_rev" : "3665447", "name" : "one" }, 
    { "_id" : "five/3730983", "_rev" : "3730983", "name" : "two" }, 
    { "_id" : "five/3862055", "_rev" : "3862055", "name" : "four" }, 
    { "_id" : "five/3993127", "_rev" : "3993127", "name" : "three" }]

@CLEARPAGE
@FUN{start_pretty_print()}

While the standard JSON stringifier is very concise it is hard to read.  Calling
the function @FN{start_pretty_print} will enable the pretty printer which
formats the output in a human-readable way.

    arangosh> start_pretty_print();
    using pretty printing
    arangosh> db.five.toArray();
    [
      { 
        "_id" : "five/3665447", 
        "_rev" : "3665447", 
        "name" : "one"
      }, 
      { 
        "_id" : "five/3730983", 
        "_rev" : "3730983", 
        "name" : "two"
      }, 
      { 
        "_id" : "five/3862055", 
        "_rev" : "3862055", 
        "name" : "four"
      }, 
      { 
        "_id" : "five/3993127", 
        "_rev" : "3993127", 
        "name" : "three"
      }
    ]

@CLEARPAGE
@FUN{stop_pretty_print()}

The function disables the pretty printer, switching back to the standard dense
JSON output format.
