ArangoDB API changes for Version 3.0
====================================

Overview
--------

### Breaking changes to existing API:

 1. Cap-constraints are withdrawn

 2. first() and last() are withdrawn

    <collection>.first() and <collection>.last() are withdrawn
 
 3. Automatic creation of collections with first use

 4. Pre Version 1.4 API compatibility withdrawn

 5. Query-parameters "policy" and "rev" withdrawn in replace()/update()

 6. /_api/edge withdrawn

 7. <collection>.BY_EXAMPLE_HASH , <collection>.BY_EXAMPLE_SKIPLIST,
    <collection>.BY_CONDITION_SKIPLIST deleted.
    SimpleQuery.byCondition withdrawn.

 8. `arangodump` arguments `translateIDs`, `failOnUnknown` withdrawn.

 9. <collection>.exists now throws an error if there is a revision
    conflict

### New capabilities:

 1. Babies for document queries.

 2. _from and _to in edges can be changed.

 3. _key, _from, _to can be indexed like normal attributes.

Explanations
------------

### Breaking changes

 1. Cap-constraints are withdrawn

    For 3.1 we plan to implement full MVCC and then at the latest it is
    no longer well-defined what a cap constraint means. Furthermore, in
    cluster mode it is really hard to implement as defined.

 2. <collection>.first() and <collection>.last() are withdrawn

    Harmonization between single server and cluster. Cluster
    implementation would be very difficult and inefficient. Even the
    single server implementation simplifies without this (one doubly
    linked list less) and saves memory.

 3. Automatic creation of collections with first use

    Never worked in cluster, harmonization between single server and
    cluster, In cluster would be undefined, since number of shards,
    shard keys, replication factor and replication quorum would not be
    specified.

 4. Pre Version 1.4 API compatibility withdrawn

 5. Query-parameters "policy" and "rev" withdrawn in PUT /_api/document/<id>

    "policy" was non-sensical (just do not specify "rev")

 6. /_api/edge withdrawn

    Works now all over /_api/document
    This is much cleaner and one can use the new baby-capabilities for
    edges. Less code, drivers can be adjusted relatively easily by switching
    to /_api/document.

 7. <collection>.BY_EXAMPLE_HASH , <collection>.BY_EXAMPLE_SKIPLIST,
    <collection>.BY_CONDITION_SKIPLIST deleted

    These were never documented.

    <collection>.{byExample,byExampleHash,byExampleSkiplist,byConditionSkiplist}
    are still there and now do AQL internally. byExampleHash and
    byExampleSkiplist just call byExample.
    
    SimpleQuery.byCondition withdrawn.

 8. `arangodump` arguments `translateIDs`, `failOnUnknown` withdrawn.

 9. <collection>.exists now throws an error if there is a revision conflict

    With the old behaviour (return false) one cannot distinguish whether
    a document does not exist at all or whether the queries revision is
    outdated.
    
### New capabilities:

 1. Babies for document queries.

    Higher performance, necessary for synchronous replication, good for
    clean API abstraction for transactions.

 2. _from and _to in edges can be changed.

    Comes with the VelocyPack change.

 3. _key, _from, _to can be indexed like normal attributes.

    Comes with the VelocyPack change.

