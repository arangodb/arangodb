!CHAPTER Configuration

!SUBSECTION Requirements

Synchronous replication requires an operational ArangoDB cluster.

!SUBSECTION Enabling synchronous replication

Synchronous replication can be enabled per collection. When creating you can specify the number of replicas using *replicationFactor*. The default is `1` which effectively *disables* synchronous replication.

Example:

    127.0.0.1:8530@_system> db._create("test", {"replicationFactor": 3})

Any write operation will require 2 replicas to report success from now on.