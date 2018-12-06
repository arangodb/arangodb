Features and Improvements in ArangoDB 2.2
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.2.  ArangoDB 2.2 also contains several bugfixes that are not listed
here.

AQL improvements
----------------

### Data modification AQL queries

Up to including version 2.1, AQL supported data retrieval operations only.
Starting with ArangoDB version 2.2, AQL also supports the following 
data modification operations:

- INSERT: insert new documents into a collection
- UPDATE: partially update existing documents in a collection
- REPLACE: completely replace existing documents in a collection
- REMOVE: remove existing documents from a collection

Data-modification operations are normally combined with other AQL
statements such as *FOR* loops and *FILTER* conditions to determine
the set of documents to operate on. For example, the following query
will find all documents in collection *users* that match a specific 
condition and set their *status* variable to *inactive*:

    FOR u IN users
      FILTER u.status == 'not active'
      UPDATE u WITH { status: 'inactive' } IN users

The following query copies all documents from collection *users* into
collection *backup*:

    FOR u IN users
      INSERT u IN backup

And this query removes documents from collection *backup*:

    FOR doc IN backup
      FILTER doc.lastModified < DATE_NOW() - 3600
      REMOVE doc IN backup

For more information on data-modification queries, please refer to
[Data modification queries](../../AQL/DataQueries.html#data-modification-queries).

### Updatable variables

Previously, the value of a variable assigned in an AQL query with the `LET` keyword 
was not updatable in an AQL query. This prevented statements like the following from 
being executable:

    LET sum = 0
    FOR v IN values
      SORT v.year
      LET sum = sum + v.value
      RETURN { year: v.year, value: v.value, sum: sum }

### Other AQL improvements

* added AQL TRANSLATE function

  This function can be used to perform lookups from static objects, e.g.

      LET countryNames = { US: "United States", UK: "United Kingdom", FR: "France" }
      RETURN TRANSLATE("FR", countryNames) 

      LET lookup = { foo: "foo-replacement", bar: "bar-replacement", baz: "baz-replacement" }
      RETURN TRANSLATE("foobar", lookup, "not contained!")


Write-ahead log
---------------

All write operations in an ArangoDB server will now be automatically logged
in the server's write-ahead log. The write-ahead log is a set of append-only 
logfiles, and it is used in case of a crash recovery and for replication.

Data from the write-ahead log will eventually be moved into the journals or
datafiles of collections, allowing the server to remove older write-ahead logfiles. 

Cross-collection transactions in ArangoDB should benefit considerably by this
change, as less writes than in previous versions are required to ensure the data 
of multiple collections are atomically and durably committed. All data-modifying 
operations inside transactions (insert, update, remove) will write their 
operations into the write-ahead log directly now. In previous versions, such 
operations were buffered until the commit or rollback occurred. Transactions with
multiple operations should therefore require less physical memory than in previous 
versions of ArangoDB.
  
The data in the write-ahead log can also be used in the replication context. In
previous versions of ArangoDB, replicating from a master required turning on a
special replication logger on the master. The replication logger caused an extra
write operation into the *_replication* system collection for each actual write
operation. This extra write is now superfluous. Instead, slaves can read directly
from the master's write-ahead log to get informed about most recent data changes. 
This removes the need to store data-modification operations in the *_replication*
collection altogether. 

For the configuration of the write-ahead log, please refer to
[Write-ahead log options](../Programs/Arangod/Wal.md).

The introduction of the write-ahead log also removes the need to configure and
start the replication logger on a master. Though the replication logger object
is still available in ArangoDB 2.2 to ensure API compatibility, starting, stopping,
or configuring it will have no effect.


Performance improvements
------------------------

* Removed sorting of attribute names when in collection shaper

  In previous versions of ArangoDB, adding a document with previously not-used
  attribute names caused a full sort of all attribute names used in the 
  collection. The sorting was done to ensure fast comparisons of attribute
  names in some rare edge cases, but it considerably slowed down inserts into
  collections with many different or even unique attribute names.

* Specialized primary index implementation to allow faster hash table 
  rebuilding and reduce lookups in datafiles for the actual value of `_key`.
  This also reduces the amount of random memory accesses for primary index inserts.

* Reclamation of index memory when deleting last document in collection

  Deleting documents from a collection did not lead to index sizes being reduced.
  Instead, the index memory was kept allocated and re-used later when a collection
  was refilled with new documents. Now, index memory of primary indexes and hash 
  indexes is reclaimed instantly when the last document in a collection is removed.

* Prevent buffering of long print results in arangosh's and arangod's print
  command

  This change will emit buffered intermediate print results and discard the
  output buffer to quickly deliver print results to the user, and to prevent
  constructing very large buffers for large results.


Miscellaneous improvements
--------------------------

* Added `insert` method as an alias for `save`. Documents can now be inserted into
  a collection using either method:
  
      db.test.save({ foo: "bar" }); 
      db.test.insert({ foo: "bar" }); 

* Cleanup of options for data-modification operations

  Many of the data-modification operations had signatures with many optional 
  bool parameters, e.g.:

      db.test.update("foo", { bar: "baz" }, true, true, true)
      db.test.replace("foo", { bar: "baz" }, true, true)
      db.test.remove("foo", true, true)
      db.test.save({ bar: "baz" }, true)

  Such long parameter lists were unintuitive and hard to use when only one of
  the optional parameters should have been set.

  To make the APIs more usable, the operations now understand the following 
  alternative signature:

      collection.update(key, update-document, options)
      collection.replace(key, replacement-document, options)
      collection.remove(key, options)
      collection.save(document, options)

  Examples:

      db.test.update("foo", { bar: "baz" }, { overwrite: true, keepNull: true, waitForSync: true })
      db.test.replace("foo", { bar: "baz" }, { overwrite: true, waitForSync: true })
      db.test.remove("foo", { overwrite: true, waitForSync: true })
      db.test.save({ bar: "baz" }, { waitForSync: true })

* Added `--overwrite` option to arangoimp

  This allows removing all documents in a collection before importing into it
  using arangoimp.

* Honor startup option `--server.disable-statistics` when deciding whether or not
  to start periodic statistics collection jobs

  Previously, the statistics collection jobs were started even if the server was
  started with the `--server.disable-statistics` flag being set to `true`. Now if 
  the option is set to `true`, no statistics will be collected on the server.

* Disallow storing of JavaScript objects that contain JavaScript native objects
  of type `Date`, `Function`, `RegExp` or `External`, e.g.

      db.test.save({ foo: /bar/ });
      db.test.save({ foo: new Date() });

  This will now print

      Error: <data> cannot be converted into JSON shape: could not shape document

  Previously, objects of these types were silently converted into an empty object
  (i.e. `{ }`) and no warning was issued.

  To store such objects in a collection, explicitly convert them into strings 
  like this:

      db.test.save({ foo: String(/bar/) });
      db.test.save({ foo: String(new Date()) });


Removed features
----------------

### MRuby integration for arangod

ArangoDB had an experimental MRuby integration in some of the publish builds.
This wasn't continuously developed, and so it has been removed in ArangoDB 2.2.

This change has led to the following startup options being superfluous:

- `--ruby.gc-interval`
- `--ruby.action-directory`
- `--ruby.modules-path`
- `--ruby.startup-directory`

Specifying these startup options will do nothing in ArangoDB 2.2, so using these 
options should be avoided from now on as they might be removed in a future version
of ArangoDB.

### Removed startup options

The following startup options have been removed in ArangoDB 2.2. Specifying them
in the server's configuration file will not produce an error to make migration
easier. Still, usage of these options should be avoided as they will not have any
effect and might fully be removed in a future version of ArangoDB:

- `--database.remove-on-drop`
- `--database.force-sync-properties`
- `--random.no-seed`
- `--ruby.gc-interval`
- `--ruby.action-directory`
- `--ruby.modules-path`
- `--ruby.startup-directory`
- `--server.disable-replication-logger`

<!-- Michaels new features for graph -->

Multi Collection Graphs
-----------------------
ArangoDB is a multi model database with native graph support.
In version 2.2 the features for graphs have been improved by integration of a new graph module.
All graphs created with the old module are automatically migrated into the new module but can still be used by the old module.

### New graph module
Up to including version 2.1, ArangoDB offered a module for graphs and graph operations.
This module allowed you to use exactly one edge collection together with one vertex collection in a graph.
With ArangoDB version 2.2 this graph module is deprecated and a new graph module is offered.
This new module allows to combine an arbitrary number of vertex collections and edge collections in the same graph.
For each edge collection a list of collections containing source vertices and a list of collections containing target vertices can be defined.
If an edge is stored ArangoDB checks if this edge is valid in this collection.
Furthermore if a vertex is removed from one of the collections all connected edges will be removed as well, giving the guarantee of no loose ends in the graphs.
The layout of the graph can be modified at runtime by adding or removing collections and changing the definitions for edge collections.
All operations on the graph level are transactional by default.

### Graphs in AQL
Multi collection graphs have been added to AQL as well.
Basic functionality (getting vertices, edges, neighbors) can be executed using the entire graph.
Also more advanced features like shortest path calculations, characteristic factors of the graph or traversals have been integrated into AQL.
For these functions all graphs created with the graph module can be used.

<!-- End of Michaels new features -->
