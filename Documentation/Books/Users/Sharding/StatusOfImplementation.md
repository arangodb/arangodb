Status of the implementation
============================

This version 2.0 of ArangoDB contains the first usable implementation
of the sharding extensions. However, not all planned features are
included in this release. In particular, automatic fail-over is fully
prepared in the architecture but is not yet implemented. If you use
Version 2.0 in cluster mode in a production system, you have to
organize failure recovery manually. This is why, at this stage with
Version 2.0 we do not yet recommend to use the cluster mode in
production systems. If you really need this feature now, please contact
us.

This section provides an overview over the implemented and future
features.

In normal single instance mode, ArangoDB works as usual
with the same performance and functionality as in previous releases.

In cluster mode, the following things are implemented in version 2.0 
and work:

  * All basic CRUD operations for single documents and edges work
    essentially with good performance.
  * One can use sharded collections and can configure the number of
    shards for each such collection individually. In particular, one
    can have fully sharded collections as well as cluster-wide available
    collections with only a single shard. After creation, these
    differences are transparent to the client.
  * Creating and dropping cluster-wide databases works.
  * Creating, dropping and modifying cluster-wide collections all work.
    Since these operations occur seldom, we will only improve their
    performance in a future release, when we will have our own
    implementation of the agency as well as a cluster-wide event managing
    system (see road map for release 2.3).
  * Sharding in a collection, can be configured to use hashing
    on arbitrary properties of the documents in the collection.
  * Creating and dropping indices on sharded collections works. Please
    note that an index on a sharded collection is not a global index 
    but only leads to a local index of the same type on each shard.
  * All SimpleQueries work. Again, we will improve the performance in
    future releases, when we revisit the AQL query optimizer 
    (see road map for release 2.2).
  * AQL queries work, but with relatively bad performance. Also, if the
    result of a query on a sharded collection is large, this can lead
    to an out of memory situation on the coordinator handling the
    request. We will improve this situation when we revisit the AQL
    query optimizer (see road map for release 2.2).
  * Authentication on the cluster works with the method known from
    single ArangoDB instances on the coordinators. A new cluster-internal
    authorization scheme has been created. See below for hints on a
    sensible firewall and authorization setup.
  * Most standard API calls of the REST interface work on the cluster
    as usual, with a few exceptions, which do no longer make sense on
    a cluster or are harder to implement. See below for details.


The following does not yet work, but is planned for future releases (see
road map):

  * Transactions can be run, but do not behave like transactions. They
    simply execute but have no atomicity or isolation in version 2.0.
    See the road map for version 2.X.
  * Data-modification AQL queries are not executed atomically or isolated.
    If a data-modification AQL query fails for one shard, it might be
    rolled back there, but still complete on other shards.
  * Data-modification AQL queries require a *_key* attribute in documents
    in order to operate. If a different shard key is chosen for a collection,
    specifying the *_key* attribute is currently still required. This
    restriction might be lifted in a future release.
  * We plan to revise the AQL optimizer for version 2.2. This is
    necessary since for efficient queries in cluster mode we have to
    do as much as possible of the filtering and sorting on the
    individual DBservers rather than on the coordinator.
  * Our software architecture is fully prepared for replication, automatic 
    fail-over and recovery of a cluster, which will be implemented
    for version 2.3 (see our road map).
  * This setup will at the same time, allow for hot swap and in-service 
    maintenance and scaling of a cluster. However, in version 2.0 the
    cluster layout is static and no redistribution of data between the
    DBservers or moving of shards between servers is possible. 
  * At this stage the sharding of an [edge collection](../Glossary/README.md#edge-collection) is independent of
    the sharding of the corresponding vertex collection in a graph.
    For version 2.2 we plan to synchronize the two, to allow for more
    efficient graph traversal functions in large, sharded graphs. We
    will also do research on distributed algorithms for graphs and
    implement new algorithms in ArangoDB. However, at this stage, all
    graph traversal algorithms are executed on the coordinator and
    this means relatively poor performance since every single edge
    step leads to a network exchange.
  * In version 2.0 the import API is broken for sharded collections.
    It will appear to work but will in fact silently fail. Fixing this
    is on the road map for version 2.1.
  * In version 2.0 the *arangodump* and *arangorestore* programs
    can not be used talking to a coordinator to directly backup
    sharded collections. At this stage, one has to backup the
    DBservers individually using *arangodump* and *arangorestore*
    on them. The coordinators themselves do not hold any state and
    therefore do not need backup. Do not forget to backup the meta
    data stored in the agency because this is essential to access
    the sharded collections. These limitations will be fixed in
    version 2.1.
  * In version 2.0 the replication API (*/_api/replication*)
    does not work on coordinators. This is intentional, since the 
    plan is to organize replication with automatic fail-over directly
    on the DBservers, which is planned for version 2.3.
  * The *db.<collection>.rotate()* method for sharded collections is not
    yet implemented, but will be supported from version 2.1 onwards.
  * The *db.<collection>.rename()* method for sharded collections is not
    yet implemented, but will be supported from version 2.1 onwards.
  * The *db.<collection>.checksum()* method for sharded collections is
    not yet implemented, but will be supported from version 2.1
    onwards.

The following restrictions will probably stay, for cluster mode, even in
future versions. This is, because they are difficult or even impossible
to implement efficiently:

  * Custom key generators with the *keyOptions* property in the
    *_create* method for collections are not supported. We plan
    to improve this for version 2.1 (see road map). However, due to the
    distributed nature of a sharded collection, not everything that is
    possible in the single instance situation will be possible on a
    cluster. For example the auto-increment feature in a cluster with
    multiple DBservers and coordinators would have to lock the whole
    collection centrally for every document creation, which
    essentially defeats the performance purpose of sharding.
  * Unique constraints on non-sharding keys are unsupported. The reason
    for this is that we do not plan to have global indices for sharded
    collections. Therefore, there is no single authority that could
    efficiently decide whether or not the unique constraint is
    satisfied by a new document. The only possibility would be to have
    a central locking mechanism and use heavy communication for every
    document creation to ensure the unique constraint.
  * The method *db.<collection>.revision()* for a sharded collection 
    returns the highest revision number from all shards. However,
    revision numbers are assigned per shard, so this is not guaranteed
    to be the revision of the latest inserted document. Again,
    maintaining a global revision number over all shards is very
    difficult to maintain efficiently.
  * The methods *db.<collection>.first()* and *db.<collection>.last()* are 
    unsupported for collections with more than one shard. The reason for
    this, is that temporal order in a highly parallelized environment
    like a cluster is difficult or even impossible to achieve
    efficiently. In a cluster it is entirely possible that two
    different coordinators add two different documents to two
    different shards *at the same time*. In such a situation it is not
    even well-defined which of the two documents is "later". The only
    way to overcome this fundamental problem would again be a central
    locking mechanism, which is not desirable for performance reasons.
  * Contrary to the situation in a single instance, objects representing
    sharded collections are broken after their database is dropped.
    In a future version they might report that they are broken, but
    it is not feasible and not desirable to retain the cluster database 
    in the background until all collection objects are garbage
    collected.
  * In a cluster, the automatic creation of collections on a call to
    *db._save(ID)* is not supported. This is because one would have no
    way to specify the number or distribution of shards for the newly
    created collection. Therefore we will not offer this feature for
    cluster mode.

