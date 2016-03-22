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
    use /_api/document instead
    Attributes `_from` and `_to` for edges must be passed inside the
    document body and not as URL parameters `from` and `to`

 7. <collection>.BY_EXAMPLE_HASH , <collection>.BY_EXAMPLE_SKIPLIST,
    <collection>.BY_CONDITION_SKIPLIST deleted.
    SimpleQuery.byCondition withdrawn.

 8. `arangodump` arguments `translateIDs`, `failOnUnknown` withdrawn.

 9. <collection>.exists now throws an error if there is a revision
    conflict

10. replacing an edge in an edge collection now requires the specification
    of both the `_from` and `_to` attributes. In 2.8 it was not required
    to specify `_from` and `_to` when replacing an edge.

11. /_api/collection/figures will not return the following result attributes:
    - shapefiles.count
    - shapes.fileSize
    - shapes.count
    - shapes.size
    - attributes.count
    - attributes.size

12. the `checksum` attribute returned by GET /_api/collection/<collection>/figures
    now is returned as a string value. It was returned as a number value in previous
    versions. Additionally the checksum calculation algorithm was changed in 3.0,
    so 3.0 will create different checksums than previous versions for the same data.

13. the HTTP API for modifying documents (/_api/document) does not return the `error`
    attribute with a value of `false` in case an operation succeeds.


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

