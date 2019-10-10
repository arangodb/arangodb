---
layout: default
description: This is an introduction to ArangoDB's HTTP replication interface
---
HTTP Interface for Replication
==============================

Replication
-----------

This is an introduction to ArangoDB's HTTP replication interface.
The replication architecture and components are described in more details in 
[Replication](../architecture-replication.html).

The HTTP replication interface serves four main purposes:
- fetch initial data from a server (e.g. for a backup, or for the initial synchronization 
  of data before starting the continuous replication applier)
- querying the state of a master
- fetch continuous changes from a master (used for incremental synchronization of changes)
- administer the replication applier (starting, stopping, configuring, querying state) on 
  a slave

Please note that if a per-database setup is used (as opposed to server-level replication,
available since v3.3.0), then the replication system must be configured individually per
database, and replicating the data of multiple databases will require multiple operations.
