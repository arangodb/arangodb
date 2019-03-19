Satellite Collections
=====================

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

When doing joins in an ArangoDB cluster data has to be exchanged between different servers.

Joins will be executed on a coordinator. It will prepare an execution plan
and execute it. When executing, the coordinator will contact all shards of the
starting point of the join and ask for their data. The database servers carrying
out this operation will load all their local data and then ask the cluster for
the other part of the join. This again will be distributed to all involved shards
of this join part.

In sum this results in much network traffic and slow results depending of the
amount of data that has to be sent throughout the cluster.

Satellite collections are collections that are intended to address this issue.

They will facilitate the synchronous replication and replicate all its data
to all database servers that are part of the cluster.

This enables the database servers to execute that part of any join locally.

This greatly improves performance for such joins at the costs of increased
storage requirements and poorer write performance on this data.

To create a satellite collection set the *replicationFactor* of this collection
to "satellite".

Using arangosh:

    arangosh> db._create("satellite", {"replicationFactor": "satellite"});

A full example
--------------

    arangosh> var explain = require("@arangodb/aql/explainer").explain
    arangosh> db._create("satellite", {"replicationFactor": "satellite"})
    arangosh> db._create("nonsatellite", {numberOfShards: 8})
    arangosh> db._create("nonsatellite2", {numberOfShards: 8})

Let's analyse a normal join not involving satellite collections:

```
arangosh> explain("FOR doc in nonsatellite FOR doc2 in nonsatellite2 RETURN 1")

Query string:
 FOR doc in nonsatellite FOR doc2 in nonsatellite2 RETURN 1

Execution plan:
 Id   NodeType                  Site       Est.   Comment
  1   SingletonNode             DBS           1   * ROOT
  4   CalculationNode           DBS           1     - LET #2 = 1   /* json expression */   /* const assignment */
  2   EnumerateCollectionNode   DBS           0     - FOR doc IN nonsatellite   /* full collection scan */
 12   RemoteNode                COOR          0       - REMOTE
 13   GatherNode                COOR          0       - GATHER
  6   ScatterNode               COOR          0       - SCATTER
  7   RemoteNode                DBS           0       - REMOTE
  3   EnumerateCollectionNode   DBS           0       - FOR doc2 IN nonsatellite2   /* full collection scan */
  8   RemoteNode                COOR          0         - REMOTE
  9   GatherNode                COOR          0         - GATHER
  5   ReturnNode                COOR          0         - RETURN #2

Indexes used:
 none

Optimization rules applied:
 Id   RuleName
  1   move-calculations-up
  2   scatter-in-cluster
  3   remove-unnecessary-remote-scatter
```

All shards involved querying the `nonsatellite` collection will fan out via the
coordinator to the shards of `nonsatellite`. In sum 8 shards will open 8 connections
to the coordinator asking for the results of the `nonsatellite2` join. The coordinator
will fan out to the 8 shards of `nonsatellite2`. So there will be quite some
network traffic.

Let's now have a look at the same using satellite collections:

```
arangosh> db._query("FOR doc in nonsatellite FOR doc2 in satellite RETURN 1")

Query string:
 FOR doc in nonsatellite FOR doc2 in satellite RETURN 1

Execution plan:
 Id   NodeType                  Site       Est.   Comment
  1   SingletonNode             DBS           1   * ROOT
  4   CalculationNode           DBS           1     - LET #2 = 1   /* json expression */   /* const assignment */
  2   EnumerateCollectionNode   DBS           0     - FOR doc IN nonsatellite   /* full collection scan */
  3   EnumerateCollectionNode   DBS           0       - FOR doc2 IN satellite   /* full collection scan, satellite */
  8   RemoteNode                COOR          0         - REMOTE
  9   GatherNode                COOR          0         - GATHER
  5   ReturnNode                COOR          0         - RETURN #2

Indexes used:
 none

Optimization rules applied:
 Id   RuleName
  1   move-calculations-up
  2   scatter-in-cluster
  3   remove-unnecessary-remote-scatter
  4   remove-satellite-joins
```

In this scenario all shards of nonsatellite will be contacted. However
as the join is a satellite join all shards can do the join locally
as the data is replicated to all servers reducing the network overhead
dramatically.

Caveats
-------

The cluster will automatically keep all satellite collections on all servers in sync
by facilitating the synchronous replication. This means that write will be executed
on the leader only and this server will coordinate replication to the followers.
If a follower doesn't answer in time (due to network problems, temporary shutdown etc.)
it may be removed as a follower. This is being reported to the Agency.

The follower (once back in business) will then periodically check the Agency and know
that it is out of sync. It will then automatically catch up. This may take a while
depending on how much data has to be synced. When doing a join involving the satellite
you can specify how long the DBServer is allowed to wait for sync until the query
is being aborted.

Check [Accessing Cursors](../HTTP/AqlQueryCursor/AccessingCursors.html)
for details.

During network failure there is also a minimal chance that a query was properly
distributed to the DBServers but that a previous satellite write could not be
replicated to a follower and the leader dropped the follower. The follower however
only checks every few seconds if it is really in sync so it might indeed deliver
stale results.
