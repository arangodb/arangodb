Features and Improvements
=========================

The following list shows in detail which features have been added or improved in
ArangoDB 3.4. ArangoDB 3.4 also contains several bugfixes that are not listed
here.
   
   
ArangoSearch 
------------

[ArangoSearch](../Views/ArangoSearch.md)


Streaming AQL Cursors
---------------------

It is now possible to create AQL query cursors with the new *stream* option.
Specify *true* and the query will be executed in a **streaming** fashion. The query result is
not stored on the server, but calculated on the fly. *Beware*: long-running queries will
need to hold the collection locks for as long as the query cursor exists.
When set to *false* the query will be executed right away in its entirety. 
In that case query results are either returned right away (if the resultset is small enough),
or stored on the arangod instance and accessible via the cursor API. It is advisable
to *only* use this option on short-running queries *or* without exclusive locks (write locks on MMFiles).
Please note that the query options `cache`, `count` and `fullCount` will not work on streaming
queries. Additionally query statistics, warnings and profiling data will only be available
after the query is finished. 
The default value is *false*


Single document operations
--------------------------

When you now have AQL queries that `INSERT`, `UPDATE`, `REMOVE` or fetch a single document
in a cluster by i.e. using `FILTER _key == '123'`, the coordinator node will now directly
carry out the change on the db-server instead of instanciating respective AQL-Snippets
on the DB-Servers. This reduces the amount of cluster roundtrips and thus improve the performance.
This may also be used when working with simple queries, where you don't even use AQL by 
yourselves.

Miscellaneous features
----------------------

- new optional collection property `cacheEnabled` which enables in-memory caching
  for documents and primary index entries. This can potentially speed up point-lookups
  significantly, especially if your collections has a subset of frequently accessed
  keys. Please test this feature carefully to ensure that it does not adversely
  affect the performance of your system.
