Configuration
=============

Requirements
------------

Synchronous replication requires an operational ArangoDB cluster.

Enabling synchronous replication
--------------------------------

Synchronous replication can be enabled per collection. When creating a
collection you may specify the number of replicas using the
*replicationFactor* parameter. The default value is set to `1` which
effectively *disables* synchronous replication. 

Example:

    127.0.0.1:8530@_system> db._create("test", {"replicationFactor": 3})

In the above case, any write operation will require 2 replicas to
report success from now on. 

Preparing growth
----------------

You may create a collection with higher replication factor than
available. When additional db servers become available the shards are
automatically replicated to the newly available machines. 

Multiple replicas of the same shard can never coexist on the same db
server instance.
