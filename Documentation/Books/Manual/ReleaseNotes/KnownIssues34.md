Known Issues
============

The following known issues are present in this version of ArangoDB and will be fixed
in follow-up releases:

Installer
---------

* Rolling upgrades are not yet supported with the RC version of ArangoDB 3.4, when
  there are multiple instances of the same ArangoDB version running on the same host,
  and only some of them shall be upgraded, or they should be upgraded one after the
  other.

APIs
----

* the REST API for retrieving indexes at endpoint GET `/_api/index/?collection=<collection>` will 
  currently also return all links (type `arangosearch`) for views that refer to this collection. The links
  will be removed from the results of this API in a later version.
* the REST API for retrieving a single index at endpoint GET `/_api/index/<indexname>` will current
  succeed for indexes used internally for links of views. These requests will return a notfound error in 
  a later version.

MoveShards
----------

* moving shards for views might fail
