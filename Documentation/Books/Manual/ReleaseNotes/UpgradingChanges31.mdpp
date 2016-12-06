Incompatible changes in ArangoDB 3.1
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.1, and adjust any client programs if necessary.

Communication Layer
-------------------

The internal commication layer is now based on Boost ASIO. A few options
regarding threads and communication have been changed.

There are no longer two different threads pools (`--scheduler.threads` and
`--server.threads`). The option `--scheduler.threads` has been removed. The 
number of threads is now controlled by the option `--server.threads` only.
By default `--server.threads` is set to the number of hyper-cores.

As a consequence of the change, the following (hidden) startup options have 
been removed:

* `--server.extra-threads`
* `--server.aql-threads`
* `--server.backend`
* `--server.show-backends`
* `--server.thread-affinity`

AQL
---
 
The behavior of the AQL array comparison operators has changed for empty arrays:

* `ALL` and `ANY` now always return `false` when the left-hand operand is an
  empty array. The behavior for non-empty arrays does not change:
  * `[] ALL == 1` will return `false`
  * `[1] ALL == 1` will return `true`
  * `[1, 2] ALL == 1` will return `false`
  * `[2, 2] ALL == 1` will return `false`
  * `[] ANY == 1` will return `false`
  * `[1] ANY == 1` will return `true`
  * `[1, 2] ANY == 1` will return `true`
  * `[2, 2] ANY == 1` will return `false`

* `NONE` now always returns `true` when the left-hand operand is an empty array.
  The behavior for non-empty arrays does not change:
  * `[] NONE == 1` will return `true`
  * `[1] NONE == 1` will return `false`
  * `[1, 2] NONE == 1` will return `false`
  * `[2, 2] NONE == 1` will return `true`

Data format changes
-------------------

The attribute `maximalSize` has been renamed to `journalSize` in collection
meta-data files ("parameter.json"). Files containing the `maximalSize` attribute 
will still be picked up correctly for not-yet adjusted collections.

The format of the revision values stored in the `_rev` attribute of documents
has been changed in 3.1. Up to 3.0 they were strings containing largish decimal numbers. With 3.1, revision values are still strings, but are actually encoded time stamps of the creation date of the revision of the document. The time stamps are acquired using a hybrid logical clock (HLC) on the DBserver that holds the
revision (for the concept of a hybrid logical clock see 
[this paper](http://www.cse.buffalo.edu/tech-reports/2014-04.pdf)).
See [this manual section](../DataModeling/Documents/DocumentAddress.html#document-revision) for details.

ArangoDB >= 3.1 can ArangoDB 3.0 database directories and will simply continue
to use the old `_rev` attribute values. New revisions will be written with
the new time stamps.

It is highly recommended to backup all your data before loading a database
directory that was written by ArangoDB <= 3.0 into an ArangoDB >= 3.1.

To change all your old `_rev` attributes into new style time stamps you
have to use `arangodump` to dump all data out (using ArangoDB 3.0), and 
use `arangorestore` into the new ArangoDB 3.1, which is the safest
way to upgrade.

The change also affects the return format of `_rev` values and other revision
values in HTTP APIs (see below).

HTTP API changes
----------------

### APIs added

The following HTTP REST APIs have been added for online loglevel adjustment of
the server:

* GET `/_admin/log/level` returns the current loglevel settings
* PUT `/_admin/log/level` modifies the current loglevel settings

### APIs changed

* the following REST APIs that return revision ids now make use of the new revision
  id format introduced in 3.1. All revision ids returned will be strings as in 3.0, but
  have a different internal format.
  
  The following APIs are affected:
  - GET /_api/collection/{collection}/checksum: `revision` attribute
  - GET /_api/collection/{collection}/revision: `revision` attribute
  - all other APIs that return documents, which may include the documents' `_rev` attribute

  Client applications should not try to interpret the internals of revision values, but only 
  use revision values for checking whether two revision strings are identical.

* the replication REST APIs will now use the attribute name `journalSize` instead of
  `maximalSize` when returning information about collections.

* the default value for `keepNull` has been changed from `false` to `true` for 
  the following partial update operations for vertices and edges in /_api/gharial:

  - PATCH /_api/gharial/{graph}/vertex/{collection}/{key}
  - PATCH /_api/gharial/{graph}/edge/{collection}/{key}

  The value for `keepNull` can still be set explicitly to `false` by setting the
  URL parameter `keepNull` to a value of `false`.

* the REST API for dropping collections (DELETE /_api/collection) now accepts an
  optional query string parameter `isSystem`, which can set to `true` in order to
  drop system collections. If the parameter is not set or not set to true, the REST
  API will refuse to drop system collections. In previous versions of ArangoDB, the
  `isSystem` parameter did not exist, and there was no distinction between system 
  and non-system collections when dropping collections.

* the REST API for retrieving AQL query results (POST /_api/cursor) will now return an
  additional sub-attribute `loading collections` that will contain the total time
  required for loading and locking collections during the AQL query when profiling is
  enabled. The attribute can be found in the `extra` result attribute in sub-attribute
  `loading collections`. The attribute will only be set if profiling was enabled for
  the query.

Foxx Testing
------------

The QUnit interface to Mocha has been removed. This affects the behaviour of the `suite`, `test`, `before`, `after`, `beforeEach` and `afterEach` functions in Foxx test suites. The `suite` and `test` functions are now provided by the TDD interface. The `before`, `after`, `beforeEach` and `afterEach` functions are now provided by the BDD interface.

This should not cause any problems with existing tests but may result in failures in test cases that previously passed for the wrong reasons. Specifically the execution order of the `before`, `after`, etc functions now follows the intended order and is no longer arbitrary.

For details on the expected behaviour of these functions see the [testing chapter](../Foxx/Testing.md) in the Foxx documentation.
