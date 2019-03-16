Smart Joins
===========

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

When doing joins in an ArangoDB cluster data has to exchanged between different servers.

Joins between collections in a cluster normally require roundtrips between the shards of 
the different collections for fetching the data. Requests are routed through an extra
coordinator hop.

For example, with two collections *collection1* and *collection2* with 4 shards each,
the coordinator will initially contact the 4 shards of *collection1*. In order to perform
the join, the DBServer nodes which manage the actual data of *collection1* need to pull
the data from the other collection, *collection2*. This causes extra roundtrips via the
coordinator, which will then pull the data for *collection2* from the responsible shards:

    arangosh> db._explain("FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2._key RETURN doc1");

    Query String:
     FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2._key RETURN doc1

    Execution plan:
     Id   NodeType                  Site  Est.   Comment
      1   SingletonNode             DBS      1   * ROOT
      3   EnumerateCollectionNode   DBS      0     - FOR doc2 IN collection2   /* full collection scan, 4 shard(s) */
     14   RemoteNode                COOR     0       - REMOTE
     15   GatherNode                COOR     0       - GATHER
      8   ScatterNode               COOR     0       - SCATTER
      9   RemoteNode                DBS      0       - REMOTE
      7   IndexNode                 DBS      0       - FOR doc1 IN collection1   /* primary index scan, 4 shard(s) */
     10   RemoteNode                COOR     0         - REMOTE
     11   GatherNode                COOR     0         - GATHER
      6   ReturnNode                COOR     0         - RETURN doc1

This is the general query execution, and it makes sense if there is no further
information available about how the data is actually distributed to the individual
shards. It works in case *collection1* and *collection2* have a different amount
of shards, or use different shard keys or strategies. However, it comes with the
additional cost of having to do 4 x 4 requests to perform the join.

Using distributeShardsLike
--------------------------

In the specific case that the two collections have the same number of shards and
the same shards, the data of the two collections can be co-located on the same
server for the same shard key values. In this case the extra hop via the coordinator
is not be necessary.

The query optimizer will remove the extra hop for the join in case it can prove
that data for the two collections is co-located. There are the following requirements
for this:

* using the cluster version of the ArangoDB *Enterprise Edition*
* using two collections with identical sharding. This requires the second collection
  to be created with its *distributeShardsLike* attribute pointing to the first
  collection
* using a single shard key per collection

Here is an example setup for this, using arangosh:

    arangosh> db._create("collection1", {numberOfShards: 4, shardKeys: ["_key"]});
    arangosh> db._create("collection2", {numberOfShards: 4, shardKeys: ["_key"], distributeShardsLike: "collection1"});
    arangosh> for (i = 0; i < 100; ++i) { 
      db.collection1.insert({ _key: "test" + i }); 
      db.collection2.insert({ _key: "test" + i }); 
    }

With the two collections in place like this, an AQL query that uses a FILTER condition
that refers from the shard key of the one collection to the shard key of the other collection
and compares the two shard key values by equality is eligible for the "smart-join"
optimization of the query optimizer:

      arangosh> db._explain("FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2._key RETURN doc1");

      Query String:
       FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2._key RETURN doc1

      Execution plan:
       Id   NodeType                  Site  Est.   Comment
        1   SingletonNode             DBS      1   * ROOT
        3   EnumerateCollectionNode   DBS      0     - FOR doc2 IN collection2   /* full collection scan, 4 shard(s) */
        7   IndexNode                 DBS      0       - FOR doc1 IN collection1   /* primary index scan, 4 shard(s) */
       10   RemoteNode                COOR     0         - REMOTE
       11   GatherNode                COOR     0         - GATHER 
        6   ReturnNode                COOR     0         - RETURN doc1

As can be seen above, the extra hop via the coordinator is gone here, which will mean
less cluster-internal traffic and a faster response time.


Smart joins will also work if the shard key of the second collection is not *_key*,
and even for non-unique shard key values, e.g.:

    arangosh> db._create("collection1", {numberOfShards: 4, shardKeys: ["_key"]});
    arangosh> db._create("collection2", {numberOfShards: 4, shardKeys: ["parent"], distributeShardsLike: "collection1"});
    arangosh> db.collection2.ensureIndex({ type: "hash", fields: ["parent"] });
    arangosh> for (i = 0; i < 100; ++i) { 
      db.collection1.insert({ _key: "test" + i }); 
      for (j = 0; j < 10; ++j) {
        db.collection2.insert({ parent: "test" + i });
      }
    }

    arangosh> db._explain("FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2.parent RETURN doc1");
    
    Query String:
     FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2.parent RETURN doc1

    Execution plan:
     Id   NodeType                  Site  Est.   Comment
      1   SingletonNode             DBS      1   * ROOT
      3   EnumerateCollectionNode   DBS   2000     - FOR doc2 IN collection2   /* full collection scan, 4 shard(s) */
      7   IndexNode                 DBS   2000       - FOR doc1 IN collection1   /* primary index scan, 4 shard(s) */
     10   RemoteNode                COOR  2000         - REMOTE
     11   GatherNode                COOR  2000         - GATHER 
      6   ReturnNode                COOR  2000         - RETURN doc1


Using smartJoinAttribute
------------------------

In case the join on the second collection must be performed on a non-shard key
attribute, there is the option to specify a *smartJoinAttribute* for the collection.
Note that setting *distributeShardsLike* is still required here, and that the
single *shardKeys* attribute must be set to `_key:`.

This *smartJoinAttribute* must be populated for all documents in the collection,
and must always contain a string value. The value of the *_key* attribute for each
document must consist of the value of the *smartJoinAttribute*, a colon character
and then some other user-defined key component.

The setup thus becomes:

    arangosh> db._create("collection1", {numberOfShards: 4, shardKeys: ["_key"]});
    arangosh> db._create("collection2", {numberOfShards: 4, shardKeys: ["_key:"], smartJoinAttribute: "parent", distributeShardsLike: "collection1"});
    arangosh> db.collection2.ensureIndex({ type: "hash", fields: ["parent"] });
    arangosh> for (i = 0; i < 100; ++i) { 
      db.collection1.insert({ _key: "test" + i }); 
      db.collection2.insert({ _key: "test" + i + ":" + "ownKey" + i, parent: "test" + i }); 
    }

Failure to populate the *smartGraphAttribute* with a string or not at all will lead
to a document being rejected on insert, update or replace. Similarly, failure to
prefix a document's *_key* attribute value with the value of the *smartGraphAttribute*
will also lead to the document being rejected:

    arangosh> db.collection2.insert({ parent: 123 });
    JavaScript exception in file './js/client/modules/@arangodb/arangosh.js' at 99,7: ArangoError 4008: smart join attribute not given or invalid

    arangosh> db.collection2.insert({ _key: "123:test1", parent: "124" });
    JavaScript exception in file './js/client/modules/@arangodb/arangosh.js' at 99,7: ArangoError 4007: shard key value must be prefixed with the value of the smart join attribute

The join can now be performed via the collection's *smartJoinAttribute*:

    arangosh> db._explain("FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2.parent RETURN doc1")

    Query String:
     FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == doc2.parent RETURN doc1

    Execution plan:
     Id   NodeType                  Site  Est.   Comment
      1   SingletonNode             DBS      1   * ROOT
      3   EnumerateCollectionNode   DBS    101     - FOR doc2 IN collection2   /* full collection scan, 4 shard(s) */
      7   IndexNode                 DBS    101       - FOR doc1 IN collection1   /* primary index scan, 4 shard(s) */
     10   RemoteNode                COOR   101         - REMOTE
     11   GatherNode                COOR   101         - GATHER 
      6   ReturnNode                COOR   101         - RETURN doc1


Restricting smart joins to a single shard
-----------------------------------------

If a FILTER condition is used on one of the shard keys, the optimizer will also try
to restrict the queries to just the required shards:

arangosh> db._explain("FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == 'test' && doc1._key == doc2.value RETURN doc1");

      Query String:
       FOR doc1 IN collection1 FOR doc2 IN collection2 FILTER doc1._key == 'test' && doc1._key == doc2.value 
       RETURN doc1

       Execution plan:
        Id   NodeType        Site  Est.   Comment
         1   SingletonNode   DBS      1   * ROOT
         8   IndexNode       DBS      1     - FOR doc1 IN collection1   /* primary index scan, shard: s2010246 */
         7   IndexNode       DBS      1       - FOR doc2 IN collection2   /* primary index scan, scan only, shard: s2010253 */
        12   RemoteNode      COOR     1         - REMOTE
        13   GatherNode      COOR     1         - GATHER 
         6   ReturnNode      COOR     1         - RETURN doc1
