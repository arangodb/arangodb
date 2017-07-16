Features and Improvements
=========================

The following list shows in detail which features have been added or improved in
ArangoDB 3.1. ArangoDB 3.1 also contains several bugfixes that are not listed
here.

SmartGraphs
-----------

ArangoDB 3.1 adds a first major enterprise only feature called SmartGraphs.
SmartGraphs form an addition to the already existing graph features and allow to
scale graphs beyond a single machine while keeping almost the same query performance.
The SmartGraph feature is suggested for all graph database use cases that require
a cluster of database servers for what ever reason.
You can either have a graph that is too large to be stored on a single machine only.
Or you can have a small graph, but at the same time need additional data with has to be
sharded and you want to keep all of them in the same envirenment.
Or you simply use the cluster for high-availability.
In all the above cases SmartGraphs will significantly increase the performance of
graph operations.
For more detailed information read [this manual section](../Graphs/SmartGraphs/index.html).

Data format
-----------

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

Communication Layer
-------------------

ArangoDB up to 3.0 used [libev](http://software.schmorp.de/pkg/libev.html) for
the communication layer. ArangoDB starting from 3.1 uses
[Boost ASIO](www.boost.org).

Starting with ArangoDB 3.1 we begin to provide the VelocyStream Protocol (vst) as
a addition to the established http protocol.

A few options have changed concerning communication, please checkout
[Incompatible changes in 3.1](./UpgradingChanges31.html).

Cluster
-------

For its internal cluster communication a (bundled version) of curl is now being
used. This enables asynchronous operation throughout the cluster and should
improve general performance slightly.

Authentication is now supported within the cluster.


Document revisions cache
------------------------

The ArangoDB server now provides an in-memory cache for frequently accessed 
document revisions. Documents that are accessed during read/write operations 
are loaded into the revisions cache automatically, and subsequently served from 
there.

The cache has a total target size, which can be controlled with the startup
option `--database.revision-cache-target-size`. Once the cache reaches the
target size, older entries may be evicted from the cache to free memory. Note that
the target size currently is a high water mark that will trigger cache memory
garbage collection if exceeded. However, if all cache chunks are still in use
when the high water mark is reached, the cache may still grow and allocate more
chunks until cache entries become unused and are allowed to be garbage-collected.

The cache is maintained on a per-collection basis, that is, memory for the cache 
is allocated on a per-collection basis in chunks. The size for the cache memory
chunks can be controlled via the startup option `--database.revision-cache-chunk-size`.
The default value is 4 MB per chunk.
Bigger chunk sizes allow saving more documents per chunk, which can lead to more 
efficient chunk allocation and lookups, but will also lead to memory waste if many 
chunks are allocated and not fully used. The latter will be the case if there exist 
many small collections which all allocate their own chunks but not fully utilize them 
because of the low number of documents.


AQL
---

### Functions added

The following AQL functions have been added in 3.1:

- *OUTERSECTION(array1, array2, ..., arrayn)*: returns the values that occur
  only once across all arrays specified.

- *DISTANCE(lat1, lon1, lat2, lon2)*: returns the distance between the two
  coordinates specified by *(lat1, lon1)* and *(lat2, lon2)*. The distance is
  calculated using the haversine formula.

- *JSON_STRINGIFY(value)*: returns a JSON string representation of the value.

- *JSON_PARSE(value)*: converts a JSON-encoded string into a regular object


### Index usage in traversals
 
3.1 allows AQL traversals to use other indexes than just the edge index.
Traversals with filters on edges can now make use of more specific indexes. For
example, the query

    FOR v, e, p IN 2 OUTBOUND @start @@edge 
      FILTER p.edges[0].foo == "bar"
      RETURN [v, e, p]

may use a hash index on `["_from", "foo"]` instead of the edge index on just 
`["_from"]`.


### Optimizer improvements

Make the AQL query optimizer inject filter condition expressions referred to 
by variables during filter condition aggregation. For example, in the following 
query

    FOR doc IN collection
      LET cond1 = (doc.value == 1)
      LET cond2 = (doc.value == 2)
      FILTER cond1 || cond2
      RETURN { doc, cond1, cond2 }

the optimizer will now inject the conditions for `cond1` and `cond2` into the 
filter condition `cond1 || cond2`, expanding it to `(doc.value == 1) || (doc.value == 2)` 
and making these conditions available for index searching.

Note that the optimizer previously already injected some conditions into other 
conditions, but only if the variable that defined the condition was not used elsewhere. 
For example, the filter condition in the query

    FOR doc IN collection
      LET cond = (doc.value == 1)
      FILTER cond
      RETURN { doc }

already got optimized before because `cond` was only used once in the query and the 
optimizer decided to inject it into the place where it was used.

This only worked for variables that were referred to once in the query. When a variable 
was used multiple times, the condition was not injected as in the following query

    FOR doc IN collection
      LET cond = (doc.value == 1)
      FILTER cond
      RETURN { doc, cond }

3.1 allows using this condition so that the query can use an index on `doc.value` 
(if such index exists).


### Miscellaneous improvements

The performance of the `[*]` operator was improved for cases in which this operator
did not use any filters, projections and/or offset/limits.

The AQL query executor can now report the time required for loading and locking the
collections used in an AQL query. When profiling is enabled, it will report the total 
loading and locking time for the query in the `loading collections` sub-attribute of the 
`extra.profile` value of the result. The loading and locking time can also be view in the
AQL query editor in the web interface.

Audit Log
---------

Audit logging has been added, see [Auditing](../Administration/Auditing/README.md).

Client tools
------------

Added option `--skip-lines` for arangoimp
This allows skipping the first few lines from the import file in case the CSV or TSV 
import are used and some initial lines should be skipped from the input.

Web Admin Interface
-------------------

The usability of the AQL editor significantly improved. In addition to the standard JSON
output, the AQL Editor is now able to render query results as a graph preview or a table.
Furthermore the AQL editor displays query profiling information.

Added a new Graph Viewer in order to exchange the technically obsolete version. The new Graph
Viewer is based on Canvas but does also include a first WebGL implementation (limited
functionality - will change in the future). The new Graph Viewer offers a smooth way to
discover and visualize your graphs.

The shard view in cluster mode now displays a progress indicator while moving shards.

Authentication
--------------

Up to ArangoDB 3.0 authentication of client requests was only possible with HTTP basic 
authentication.

Starting with 3.1 it is now possible to also use a [JSON Web Tokens](https://jwt.io/)
(JWT) for authenticating incoming requests.

For details check the HTTP authentication chapter. Both authentication methods are
valid and will be supported in the near future. Use whatever suits you best.

Foxx
----

### GraphQL

It is now easy to get started with providing GraphQL APIs in Foxx, see [Foxx GraphQL](../Foxx/GraphQL.md).

### OAuth2

Foxx now officially provides a module for implementing OAuth2 clients, see [Foxx OAuth2](../Foxx/OAuth2.md).

### Per-route middleware

It's now possible to specify middleware functions for a route when defining a route handler. These middleware functions only apply to the single route and share the route's parameter definitions. Check out the [Foxx Router documentation](../Foxx/Router/README.md) for more information.
