# Active Failover

This _Chapter_ introduces ArangoDB's _Active Failover_ environment.

An active failover is defined as:
- One ArangoDB Single-Server instance which is read / writeable by clients called **Leader**
- An ArangoDB Single-Server instance, which is passive and not read or writeable called **Follower**
- At least one Agency node, acting as a "witness" to determine which server becomes the leader in a failure situation

![Simple Leader / Follower setup, with a single node agency](leader-follower.png)

The advantage compared to a traditional Master-Slave setup is that there is an active third party 
which observes and supervises all involved server processes. _Follower_ instances can rely on the 
agency to determine the correct _leader_ server. This setup is made **resilient** by the fact
that all our official ArangoDB drivers can now automatically determine the correct _leader_ server
and redirect requests appropriately. Furthermore Foxx Services do also automatically perform
a failover: Should your _leader_ instance fail (which is also the _Foxxmaster_) the newly elected
_leader_ will reinstall all Foxx services and resume executing queued [Foxx tasks](../../Foxx/Scripts.md).
[Database users](../../Administration/ManagingUsers/README.md) which were created on the _leader_ will also be valid on the newly elected _leader_ (always depending on the condition that they were synced already).

For further information about _Active Failover_ in ArangoDB, please refer to the following sections:

- [Deployment](../../Deployment/ActiveFailover/README.md)
- [Administration](../../Administration/ActiveFailover/README.md)

**Note:** _Asynchronous Failover_, _Resilient Single_, _Active-Passive_ or _Hot Standby_ are other terms that have been used to define the _Active Failover_ environment. Starting from version 3.3 _Active Failover_ is the preferred term to identify such environment.


