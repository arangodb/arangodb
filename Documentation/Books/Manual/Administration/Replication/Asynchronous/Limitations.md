Replication Limitations
=======================

The replication in ArangoDB has a few limitations. Some of these limitations may be 
removed in later versions of ArangoDB:

* there is no feedback from the Follower servers to the Leader. If a Follower cannot apply an event
  it got from the Leader, the Leader will have a different state of data. In this
  case, the replication applier will stop and report an error. Administrators
  can then either "fix" the problem or re-sync the data from the master to the slave
  and start the applier again (This will happen automatically by default).
* at the moment it is assumed that only the replication applier executes write 
  operations on a slave. ArangoDB currently does not prevent users from carrying out
  their own write operations on slaves, though this might lead to undefined behavior
  and the replication applier stopping.
* The following collections are excluded
  intentionally from replication: *_trx*, *_replication*, *_configuration*,
  *_jobs*, *_queues*, *_sessions*, *_foxxlog*, *_frontend*, *_fishbowl* and all statistics collections.
  Write operations for the following system collections can be queried from a master: 
  *_aqlfunctions*, *_graphs*, *_users*.
* Leader servers do not know which Followers are or will be connected to them. All servers
  in a replication setup are currently only loosely coupled. There currently is no way 
  for a client to query which servers are present in a replication.
* when not using our mesos integration failover must be handled by clients or client APIs.
* It is currently not possible to have a follower apply operations from multiple Leaders into the
  same target database.
* the replication applier is single-threaded, but write operations may
  be executed in parallel on the Leader server. Thus the replication
  applier on Followers might not be able to catch up with a very powerful and loaded Leader.
* replication is only supported between the two ArangoDB servers running the same
  ArangoDB version. It is currently not possible to replicate between different ArangoDB 
  versions.
* a replication applier cannot apply data from itself.
