!CHAPTER JavaScript Interface to Collections

This is an introduction to ArangoDB's interface for collections and how to handle
collections from the JavaScript shell _arangosh_. For other languages see the
corresponding language API.

The most important call is the call to create a new collection.

!SECTION Address of a Collection 

All collections in ArangoDB have a unique identifier and a unique
name. ArangoDB internally uses the collection's unique identifier to look up
collections. This identifier, however, is managed by ArangoDB and the user has
no control over it. In order to allow users to use their own names, each collection
also has a unique name which is specified by the user. To access a collection
from the user perspective, the [collection name](../../Appendix/Glossary.md#collection-name) should be used, i.e.:

!SUBSECTION Collection
`db._collection(collection-name)`

A collection is created by a ["db._create"](DatabaseMethods.md) call.

For example: Assume that the [collection identifier](../../Appendix/Glossary.md#collection-identifier) is *7254820* and the name is
*demo*, then the collection can be accessed as:

    db._collection("demo")

If no collection with such a name exists, then *null* is returned.

There is a short-cut that can be used for non-system collections:

!SUBSECTION Collection name
`db.collection-name`

This call will either return the collection named *db.collection-name* or create
a new one with that name and a set of default properties.

**Note**: Creating a collection on the fly using *db.collection-name* is
not recommend and does not work in _arangosh_. To create a new collection, please
use

!SUBSECTION Create
`db._create(collection-name)`

This call will create a new collection called *collection-name*.
This method is a database method and is documented in detail at [Database Methods](DatabaseMethods.md#create)

!SUBSECTION Synchronous replication

Starting in ArangoDB 3.0, the distributed version offers synchronous
replication, which means that there is the option to replicate all data
automatically within the ArangoDB cluster. This is configured for sharded
collections on a per collection basis by specifying a "replication factor"
when the collection is created. A replication factor of k means that 
altogether k copies of each shard are kept in the cluster on k different
servers, and are kept in sync. That is, every write operation is automatically
replicated on all copies.

This is organised using a leader/follower model. At all times, one of the
servers holding replicas for a shard is "the leader" and all others
are "followers", this configuration is held in the Agency (see 
[Scalability](../../Scalability/README.md) for details of the ArangoDB
cluster architecture). Every write operation is sent to the leader
by one of the coordinators, and then replicated to all followers
before the operation is reported to have succeeded. The leader keeps
a record of which followers are currently in sync. In case of network
problems or a failure of a follower, a leader can and will drop a follower 
temporarily after 3 seconds, such that service can resume. In due course,
the follower will automatically resynchronize with the leader to restore
resilience.

If a leader fails, the cluster Agency automatically initiates a failover
routine after around 15 seconds, promoting one of the followers to
leader. The other followers (and the former leader, when it comes back),
automatically resynchronize with the new leader to restore resilience.
Usually, this whole failover procedure can be handled transparently
for the coordinator, such that the user code does not even see an error 
message.

Obviously, this fault tolerance comes at a cost of increased latency.
Each write operation needs an additional network roundtrip for the
synchronous replication of the followers, but all replication operations
to all followers happen concurrently. This is, why the default replication
factor is 1, which means no replication.

For details on how to switch on synchronous replication for a collection,
see the database method `db._create(collection-name)` in the section about 
[Database Methods](DatabaseMethods.md#create).
