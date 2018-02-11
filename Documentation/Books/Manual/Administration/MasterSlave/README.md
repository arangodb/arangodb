# Master/Slave administration

 

...



### Replication configuration

The replication is turned off by default. In order to create a master-slave setup,
the so-called _replication applier_ needs to be enabled on the slave databases.

Replication is configured on a per-database level. If multiple database are to be 
replicated, the replication must be set up individually per database.

The replication applier on the slave can be used to perform a one-time synchronization
with the master (and then stop), or to perform an ongoing replication of changes. To
resume replication on slave restart, the *autoStart* attribute of the replication 
applier must be set to *true*. 


...
