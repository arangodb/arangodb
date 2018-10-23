Cluster Troubleshooting
=======================

* Cluster frontend is unresponsive
  * Check if the _Coordinator_/s in question are started up.
  * Check if the _Agency_ is up and a _leader_ has been elected. If not
    ensure that all or a majority of _Agents_ are up and running.
  * Check if all processes have been started up with the same
    `JWT_SECRET`. If not ensure that the `JWT_SECRET` used across
    the cluster nodes is identical for every process.
  * Check if all cluster nodes have been started with SSL either
    dis- or enabled. If not decide what mode of operation you would
    like to run your cluster in, and consistently stick with for all
    _Agents_, _Coordinators_ and _DBServers_.
  * Check if network communication between the cluster nodes is such
    that all processes can directly access their peers. Do not
    operate proxies between the cluster nodes.

* Cluster front end announces errors on any number of nodes
  * This is an indication that the _Agency_ is running but either
    _Coordinators_ or _DBServers_ are disconnected or shut
    down. Establish network connection to or start the according
    nodes.
  * Make sure that the nodes in question share the same `JWT_SECRET`
    and SSL operation mode with the functioning nodes.

Dig deeper into cluster troubleshooting by going through the
[ArangoDB Cluster Administration Course](https://www.arangodb.com/arangodb-cluster-course/).
