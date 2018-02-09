# Cluster administration

This Section includes information related to the administration of an ArangoDB Cluster.

For a general introduction to the ArangoDB Cluster, please refer to the Cluster [chapter](../../Scalability/Cluster/README.md).

## Replacing/Removing a Coordinator

_Coordinators_ are effectively stateless and can be replaced, added and
removed without more consideration than meeting the necessities of the
particular installation. 

To take out a _Coordinator_ stop the
_Coordinator_'s instance by issueing `kill -SIGTERM <pid>`.

Ca. 15 seconds later the cluster UI on any other _Coordinator_ will mark
the _Coordinator_ in question as failed. Almost simultaneously, a trash bin
icon will appear to the right of the name of the _Coordinator_. Clicking
that icon will remove the _Coordinator_ from the coordinator registry.

Any new _Coordinator_ instance that is informed of where to find any/all
agent/s, `--cluster.agency-endpoint` `<some agent endpoint>` will be
integrated as a new _Coordinator_ into the cluster. You may also just
restart the _Coordinator_ as before and it will reintegrate itself into
the cluster.

## Replacing/Removing a DBServer

_DBServers_ are where the data of an ArangoDB cluster is stored. They
do not publish a we UI and are not meant to be accessed by any other
entity than _Coordinators_ to perform client requests or other _DBServers_
to uphold replication and resilience.

The clean way of removing a _DBServer_ is to first releave it of all
its responsibilities for shards. This applies to _followers_ as well as
_leaders_ of shards. The requirement for this operation is that no
collection in any of the databases has a `relicationFactor` greater or
equal to the current number of _DBServers_ minus one. For the pupose of
cleaning out `DBServer004` for example would work as follows, when
issued to any _Coordinator_ of the cluster:

`curl <coord-ip:coord-port>/_admin/cluster/cleanOutServer -d '{"id":"DBServer004"}'`

After the _DBServer_ has been cleaned out, you will find a trash bin
icon to the right of the name of the _DBServer_ on any _Coordinators_'
UI. Clicking on it will remove the _DBServer_ in questiuon from the
cluster.

Firing up any _DBServer_ from a clean data directory by specifying the
any of all agency endpoints will integrate the new _DBServer_ into the
cluster.

To distribute shards onto the new _DBServer_ either click on the
`Distribute Shards` button at the bottom of the `Shards` page in every
database.

