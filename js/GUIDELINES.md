Global Variables
================

There should be no global variables/functions except

* require
* module

The following functions are also defined, but should never be used globally in
any JavaScript module.

* print
* printf
* print_plain
* start_pretty_print
* stop_pretty_print
* start_color_print
* stop_color_print
* start_pager
* stop_pager

Bootstrapping
=============

When booting the arangod or arangosh process defines some global variables in
C++. These variables are named all in uppercase and are defined in the global
namespace.

The bootstrap defines the module management in `modules.js` and bootstraps
the following modules:

- "internal"
- "console"
- "fs"

These modules are defined in the files `module-xxx.js`. Common parts available
in the server and client are put into `common/bootstrap/module-xxx.js`,
server-only functions in `server/bootstrap/module-xxx.js`, and client-only parts
in `client/bootstrap/module-xxx.js`.

All global variables are moved into the module "internal" and are then deleted.

Modules
=======

Files
-----

Sometimes client and server share large parts of the code. If this is true for a
module "name", than there exists a companion module "name-common" which contains
these parts.

For large prototypes with a lot of methods, there exist separate files. I. e.
`ArangoCollection` is defined in `@arangodb/arango-collection.js`.

Module "internal"
-----------------

The module "internal" should not be used except during bootstrapping. All
required prototypes and variables are copied into the module "@arangodb",
which can be used everywhere.

Module "@arangodb"
---------------------

The module "@arangodb" exports the following prototypes

- ArangoCollection
- ArangoConnection (client-only)
- ArangoDatabase
- ArangoError
- ArangoQueryCursor (client-only)
- ArangoStatement
- ShapedJson (server-only)

It exports the global variables

- db
- arango (client-only)

It exports the following functions

- output
- print
- printf
- printObject

It exports the error codes, for example

- ERROR_NO_ERROR
- ERROR_HTTP_NO_FOUND
- ...

and as hash

- errors
