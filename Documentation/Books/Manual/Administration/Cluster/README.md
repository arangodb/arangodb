Cluster Administration
======================

This _Section_ includes information related to the administration of an ArangoDB Cluster.

For a general introduction to the ArangoDB Cluster, please refer to the
Cluster [chapter](../../Architecture/DeploymentModes/Cluster/README.md).

There is also a detailed
[Cluster Administration Course](https://www.arangodb.com/arangodb-cluster-course/)
for download.

Please check the following talks as well:

| # | Date            | Title                                                                       | Who                                     | Link                                                                                                            |
|---|-----------------|-----------------------------------------------------------------------------|-----------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| 1 | 10th April 2018 | Fundamentals and Best Practices of ArangoDB Cluster Administration          | Kaveh Vahedipour, ArangoDB Cluster Team | [Online Meetup Page](https://www.meetup.com/online-ArangoDB-meetup/events/248996022/) & [Video](https://www.youtube.com/watch?v=RQ33fkgUg64) |
| 2 | 29th May 2018   | Fundamentals and Best Practices of ArangoDB Cluster Administration: Part II | Kaveh Vahedipour, ArangoDB Cluster Team | [Online Meetup Page](https://www.meetup.com/online-ArangoDB-meetup/events/250869684/) & [Video](https://www.youtube.com/watch?v=jj7YpTaL3pI) |


Enabling synchronous replication
--------------------------------

For an introduction about _Synchronous Replication_ in Cluster, please refer
to the [_Cluster Architecture_](../../Architecture/DeploymentModes/Cluster/Architecture.md#synchronous-replication) section. 

Synchronous replication can be enabled per _collection_. When creating a
_collection_ you may specify the number of _replicas_ using the
*replicationFactor* parameter. The default value is set to `1` which
effectively *disables* synchronous replication among _DBServers_. 

Whenever you specify a _replicationFactor_ greater than 1 when creating a
collection, synchronous replication will be activated for this collection. 
The Cluster will determine suitable _leaders_ and _followers_ for every 
requested _shard_ (_numberOfShards_) within the Cluster.

Example:

```
127.0.0.1:8530@_system> db._create("test", {"replicationFactor": 3})
```

In the above case, any write operation will require 3 replicas to
report success from now on. 

Preparing growth
----------------

You may create a _collection_ with higher _replication factor_ than
available _DBServers_. When additional _DBServers_ become available 
the _shards_ are automatically replicated to the newly available _DBServers_. 

To create a _collection_ with higher _replication factor_ than
available _DBServers_ please set the option _enforceReplicationFactor_ to _false_, 
when creating the collection from _ArangoShell_ (the option is not available
from the web interface), e.g.:

```
db._create("test", { replicationFactor: 4 }, { enforceReplicationFactor: false });
```

The default value for _enforceReplicationFactor_ is true. 

**Note:** multiple _replicas_ of the same _shard_ can never coexist on the same
_DBServer_ instance.

Sharding
--------

For an introduction about _Sharding_ in Cluster, please refer to the
[_Cluster Architecture_](../../Architecture/DeploymentModes/Cluster/Architecture.md#sharding) section. 

Number of _shards_ can be configured at _collection_ creation time, e.g. the UI,
or the _ArangoDB Shell_:

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4});
```

To configure a custom _hashing_ for another attribute (default is __key_):

```
127.0.0.1:8529@_system> db._create("sharded_collection", {"numberOfShards": 4, "shardKeys": ["country"]});
```

The example above, where 'country' has been used as _shardKeys_ can be useful
to keep data of every country in one shard, which would result in better
performance for queries working on a per country base. 

It is also possible to specify multiple `shardKeys`. 

Note however that if you change the shard keys from their default `["_key"]`, then finding
a document in the collection by its primary key involves a request to
every single shard. Furthermore, in this case one can no longer prescribe
the primary key value of a new document but must use the automatically
generated one. This latter restriction comes from the fact that ensuring
uniqueness of the primary key would be very inefficient if the user
could specify the primary key.

On which DBServer in a Cluster a particular _shard_ is kept is undefined.
There is no option to configure an affinity based on certain _shard_ keys.

Unique indexes (hash, skiplist, persistent) on sharded collections are
only allowed if the fields used to determine the shard key are also
included in the list of attribute paths for the index:

| shardKeys | indexKeys |             |
|----------:|----------:|------------:|
| a         | a         |     allowed |
| a         | b         | not allowed |
| a         | a, b      |     allowed |
| a, b      | a         | not allowed |
| a, b      | b         | not allowed |
| a, b      | a, b      |     allowed |
| a, b      | a, b, c   |     allowed |
| a, b, c   | a, b      | not allowed |
| a, b, c   | a, b, c   |     allowed |

Sharding strategy
-----------------

strategy to use for the collection. Since ArangoDB 3.4 there are
different sharding strategies to select from when creating a new 
collection. The selected *shardingStrategy* value will remain
fixed for the collection and cannot be changed afterwards. This is
important to make the collection keep its sharding settings and
always find documents already distributed to shards using the same
initial sharding algorithm.

The available sharding strategies are:
- `community-compat`: default sharding used by ArangoDB community
  versions before ArangoDB 3.4
- `enterprise-compat`: default sharding used by ArangoDB enterprise
  versions before ArangoDB 3.4
- `enterprise-smart-edge-compat`: default sharding used by smart edge
  collections in ArangoDB enterprise versions before ArangoDB 3.4
- `hash`: default sharding used by ArangoDB 3.4 for new collections
  (excluding smart edge collections)
- `enterprise-hash-smart-edge`: default sharding used by ArangoDB 3.4 
  for new smart edge collections

If no sharding strategy is specified, the default will be `hash` for
all collections, and `enterprise-hash-smart-edge` for all smart edge
collections (requires the *Enterprise Edition* of ArangoDB). 
Manually overriding the sharding strategy does not yet provide a 
benefit, but it may later in case other sharding strategies are added.


Moving/Rebalancing _shards_
---------------------------

A _shard_ can be moved from a _DBServer_ to another, and the entire shard distribution
can be rebalanced using the correponding buttons in the web [UI](../../Programs/WebInterface/Cluster.md).

Replacing/Removing a _Coordinator_
----------------------------------

_Coordinators_ are effectively stateless and can be replaced, added and
removed without more consideration than meeting the necessities of the
particular installation. 

To take out a _Coordinator_ stop the
_Coordinator_'s instance by issueing `kill -SIGTERM <pid>`.

Ca. 15 seconds later the cluster UI on any other _Coordinator_ will mark
the _Coordinator_ in question as failed. Almost simultaneously, a trash bin
icon will appear to the right of the name of the _Coordinator_. Clicking
that icon will remove the _Coordinator_ from the coordinator registry.

Any new _Coordinator_ instance that is informed of where to find any/all
agent/s, `--cluster.agency-endpoint` `<some agent endpoint>` will be
integrated as a new _Coordinator_ into the cluster. You may also just
restart the _Coordinator_ as before and it will reintegrate itself into
the cluster.

Replacing/Removing a _DBServer_
-------------------------------

_DBServers_ are where the data of an ArangoDB cluster is stored. They
do not publish a we UI and are not meant to be accessed by any other
entity than _Coordinators_ to perform client requests or other _DBServers_
to uphold replication and resilience.

The clean way of removing a _DBServer_ is to first releave it of all
its responsibilities for shards. This applies to _followers_ as well as
_leaders_ of shards. The requirement for this operation is that no
collection in any of the databases has a `relicationFactor` greater or
equal to the current number of _DBServers_ minus one. For the pupose of
cleaning out `DBServer004` for example would work as follows, when
issued to any _Coordinator_ of the cluster:

`curl <coord-ip:coord-port>/_admin/cluster/cleanOutServer -d '{"id":"DBServer004"}'`

After the _DBServer_ has been cleaned out, you will find a trash bin
icon to the right of the name of the _DBServer_ on any _Coordinators_'
UI. Clicking on it will remove the _DBServer_ in questiuon from the
cluster.

Firing up any _DBServer_ from a clean data directory by specifying the
any of all agency endpoints will integrate the new _DBServer_ into the
cluster.

To distribute shards onto the new _DBServer_ either click on the
`Distribute Shards` button at the bottom of the `Shards` page in every
database.

