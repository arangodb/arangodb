---
layout: default
description: ArangoDB is a distributed database supporting multiple data models,and can thus be scaled horizontally, that is, by using many servers,typically based on commodity hardware
---
Scaling ArangoDB
================

ArangoDB is a distributed database supporting multiple data models,
and can thus be scaled horizontally, that is, by using many servers,
typically based on commodity hardware. This approach not only delivers 
performance as well as capacity improvements, but also achieves
resilience by means of replication and automatic fail-over. Furthermore,
one can build systems that scale their capacity dynamically up and down 
automatically according to demand.

One can also scale ArangoDB vertically, that is, by using
ever larger servers. There is no built in limitation in ArangoDB,
for example, the server will automatically use more threads if
more CPUs are present. 

However, scaling vertically has the disadvantage that the
costs grow faster than linear with the size of the server, and
none of the resilience and dynamical capabilities can be achieved 
in this way.

Options
-------

Several options are available to scale ArangoDB, each of them has its own pros
and cons:

- [Master/Slave](architecture-deployment-modes-master-slave.html)
- [Active Failover](architecture-deployment-modes-active-failover.html)
- [Cluster](architecture-deployment-modes-cluster.html)
- [Multiple Datacenters](architecture-deployment-modes-dc2-dc.html)
