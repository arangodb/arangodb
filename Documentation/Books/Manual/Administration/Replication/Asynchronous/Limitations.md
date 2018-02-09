Replication Limitations
=======================

The replication in ArangoDB has a few limitations. Some of these limitations may be 
removed in later versions of ArangoDB:

* there is no feedback from the slaves to the master. If a slave cannot apply an event
  it got from the master, the master will have a different state of data. In this 
  case, the replication applier on the slave will stop and report an error. Administrators
  can then either "fix" the problem or re-sync the data from the master to the slave
  and start the applier again.
* at the moment it is assumed that only the replication applier executes write 
  operations on a slave. ArangoDB currently does not prevent users from carrying out
  their own write operations on slaves, though this might lead to undefined behavior
  and the replication applier stopping.
* when a replication slave asks a master for log events, the replication master will 
  return all write operations for user-defined collections, but it will exclude write
  operations for certain system collections. The following collections are excluded
  intentionally from replication: *_apps*, *_trx*, *_replication*, *_configuration*,
  *_jobs*, *_queues*, *_sessions*, *_foxxlog* and all statistics collections.
  Write operations for the following system collections can be queried from a master: 
  *_aqlfunctions*, *_graphs*, *_users*.
* Foxx applications consist of database entries and application scripts in the file system.
  The file system parts of Foxx applications are not tracked anywhere and thus not 
  replicated in current versions of ArangoDB. To replicate a Foxx application, it is
  required to copy the application to the remote server and install it there using the
  *foxx-manager* utility. 
* master servers do not know which slaves are or will be connected to them. All servers
  in a replication setup are currently only loosely coupled. There currently is no way 
  for a client to query which servers are present in a replication.
* when not using our mesos integration failover must be handled by clients or client APIs.
* there currently is one replication applier per ArangoDB database. It is thus not 
  possible to have a slave apply operations from multiple masters into the same target
  database.
* replication is set up on a per-database level. When using ArangoDB with multiple
  databases, replication must be configured individually for each database.
* the replication applier is single-threaded, but write operations on the master may
  be executed in parallel if they affect different collections. Thus the replication
  applier might not be able to catch up with a very powerful and loaded master.
* replication is only supported between the two ArangoDB servers running the same
  ArangoDB version. It is currently not possible to replicate between different ArangoDB 
  versions.
* a replication applier cannot apply data from itself.
