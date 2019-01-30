Deployment
==========

In this chapter we describe various possibilities to deploy ArangoDB.
In particular for the cluster mode, there are different ways
and we want to highlight their advantages and disadvantages.
We even document in detail, how to set up a cluster by simply starting
various ArangoDB processes on different machines, either directly
or using Docker containers.

- [Single Instance](Single.md)
- [Single Instance vs. Cluster](../Architecture/SingleInstanceVsCluster.md)
- [Cluster](Cluster.md)
  - [DC/OS, Apache Mesos and Marathon](Mesos.md)
  - [Generic & Docker](ArangoDBStarter.md)
  - [Advanced Topics](Advanced.md)
    - [Standalone Agency](Agency.md)
    - [Test setup on a local machine](Local.md)
    - [Starting processes on different machines](Distributed.md)
    - [Launching an ArangoDB cluster using Docker containers](Docker.md)
- [Multiple Datacenters](DC2DC.md)
