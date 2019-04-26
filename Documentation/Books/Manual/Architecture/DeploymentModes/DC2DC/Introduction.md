<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Introduction

At some point in the grows of a database, there comes a need for replicating it
across multiple datacenters.

Reasons for that can be:

- Fallback in case of a disaster in one datacenter
- Regional availability
- Separation of concerns

And many more.

Starting from version 3.3, ArangoDB supports _datacenter to datacenter
replication_, via the _ArangoSync_ tool.

ArangoDB's _datacenter to datacenter replication_ is a solution that enables you
to asynchronously replicate the entire structure and content in an ArangoDB Cluster
in one place to a Cluster in another place. Typically it is used from one datacenter
to another.
<br/>It is not a solution for replicating single server instances.

![ArangoDB DC2DC](dc2dc.png)

The replication done by _ArangoSync_ is **asynchronous**. That means that when
a client is writing data into the source datacenter, it will consider the
request finished before the data has been replicated to the other datacenter.
The time needed to completely replicate changes to the other datacenter is
typically in the order of seconds, but this can vary significantly depending on
load, network & computer capacity.

_ArangoSync_ performs replication in a **single direction** only. That means that
you can replicate data from cluster _A_ to cluster _B_ or from cluster _B_ to
cluster _A_, but never at the same time.
<br/>Data modified in the destination cluster **will be lost!**

Replication is a completely **autonomous** process. Once it is configured it is
designed to run 24/7 without frequent manual intervention.
<br/>This does not mean that it requires no maintenance or attention at all.
<br/>As with any distributed system some attention is needed to monitor its operation
and keep it secure (e.g. certificate & password rotation).

Once configured, _ArangoSync_ will replicate both **structure and data** of an
**entire cluster**. This means that there is no need to make additional configuration
changes when adding/removing databases or collections.
<br/>Also meta data such as users, Foxx application & jobs are automatically replicated.

A message queue is used for replication. You can use either of the following:

- **DirectMQ** (recommended):
  Message queue developed by ArangoDB in Go. Tailored for DC2DC replication.
  Available since ArangoSync version 0.5.0.
- **Kafka**:
  Complex general purpose message queue system. Requires Java and potentially
  fine-tuning. A too small message size can cause problems with ArangoSync.
