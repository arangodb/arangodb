Manual Start
============

Setting up a working _Master/Slave_ replication requires at least two ArangoDB
instances:

1. **master:** this is the instance where all data-modification operations should
be directed to.

1. **slave:** this is the instance that replicates, in an asynchronous way, the data
from the _master_. For the replication to happen, a _replication applier_ has to
be started on the slave. The _replication applier_ will fetch data from the _master_'s
_write-ahead log_ and apply its operations locally. One or more slaves can replicate
from the same master.

Generally, one deploys the _master_ on a machine and each _slave_ on an additional,
separate, machine (one per _slave_). In case the _master_ and the _slaves_ are
running on the same machine (tests only), please make sure you use different ports
(and data directories) for the _master_ and the _slaves_.

Please install the _master_ and the _slaves_ as they were, separate,
[single instances](../SingleInstance/README.md). There are no specific differences,
at this stage, between a _master_ a _slave_ and a _single instance_.

Once the ArangoDB _master_ and _slaves_ have been deployed, the replication has
to be started on each of the available _slaves_. This can be done at database level,
or globally.

For further information on how to set up the replication in _master/slave_ environment,
please refer to [this](../../Administration/MasterSlave/SettingUp.md) _Section_.