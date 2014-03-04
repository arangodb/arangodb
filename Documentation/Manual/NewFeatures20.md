New Features in ArangoDB 2.0 {#NewFeatures20}
=============================================

@NAVIGATE_NewFeatures20
@EMBEDTOC{NewFeatures20TOC}

Features and Improvements {#NewFeatures20Introduction}
======================================================

The following list shows in detail which features have been added or
improved in ArangoDB 2.0.  ArangoDB 2.0 also contains several bugfixes
that are not listed here.

Sharding {#NewFeatures20Sharding}
---------------------------------

ArangoDB 2.0 can be run in a clustered mode, allowing to use multiple
database servers together as if they were a single database.

This is an optional feature that must be turned on and configured explicitly. 
More information about the setup and the design decisions of ArangoDB's 
sharding feature can be found in the @ref Sharding "Sharding manual".

ArangoDB can still be used as a single server database.

Changed shape storage {#NewFeatures20Shapes}
--------------------------------------------

Prior to ArangoDB 2.0, the structure of documents stored (called "shapes" in
ArangoDB lingo) was saved in separate collections (the "shape collections").
The separate storage of structural information and the actual document data 
caused overhead when a document with a new shape was saved. In this case, two
writes were required (one into the shape collection, and for the actual document
data).

ArangoDB 2.0 now saves the shape information inside the same collection as the
document data goes into, requiring less writes and potentially less space.
  
This saves up to 2 MB of memory and disk space for each collection. The actual
savings depend on the number and size of shapes per collection, but the following
rule of thumb applies: the less different shapes there are in a collection, the 
higher the savings. Additionally, one less file descriptor will be used per
opened collection.

AQL Improvements {#NewFeatures20Aql}
------------------------------------

The following list functions have been added to AQL:

* `NTH`: returns the nth element from a list

* `POSITION`: returns the position of an element in a list

* `SLICE`: extracts a slice of a list

Miscellaneous Improvements {#NewFeatures20Misc}
-----------------------------------------------

* the `db` object on the server now allows direct access to collections whose
  names start with an underscore. This makes accessing system collections on
  the server easier, for example from a Foxx action:

  In 1.4, accessing a system collection on the server required this:

      db._collection("_user").<whatever>

  whereas in 2.0, a system collection works like this everywhere:

      db._users.<whatever>

* Added coffee script support for `require`:

  Scripts written in CoffeeScript can be required and executed in arangosh and
  arangod. Scripts must reside inside the modules path in order to be found, e.g.:

      unix> cat js/common/modules/add-example.coffee
      add = (a, b) -> a + b
      exports.add = add

      arangosh> require("add-example");
      { 
          "add" : [Function "(a, b)"] 
      }

* Added JSON support for `require`:

  Files containing JSON objects can be required from arangosh and arangod.
  Files must reside inside the modules path in order to be found, e.g.:
      
      unix> cat js/common/modules/my-config.json
      {
        "app": {
          "name": "foo"
        }
      }

      arangosh> require("my-config");
      { 
        "app" : { 
          "name"  : "foo"
        }
      }                                                                                                                                      

* Added startup option `--server.reuse-address` for arangod:

  This option can be used to control whether sockets should be acquired with the 
  SO_REUSEADDR flag.

* Added startup option `--javascript.gc-interval` for arangosh:
 
  This option can be used to control the amount of garbage collection that is
  performed by arangosh. For example, to trigger the garbage collection inside
  arangosh (not arangod) after each 5 commands, use

  unix> arangosh `--javascript.gc-interval 5`

@BNAVIGATE_NewFeatures20
