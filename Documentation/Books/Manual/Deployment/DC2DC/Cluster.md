<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# ArangoDB cluster

There are several ways to start an ArangoDB cluster. In this section we will focus
on our recommended way to start ArangoDB: the ArangoDB _Starter_.

_Datacenter to datacenter replication_ requires the `rocksdb` storage engine. The
example setup described in this section will have `rocksdb` enabled. If you choose
to deploy with a different strategy keep in mind to set the storage engine.

For the other possibilities to deploy an ArangoDB cluster please refer to
[this](../Cluster/README.md) section.

The _Starter_ simplifies things for the operator and will coordinate a distributed
cluster startup across several machines and assign cluster roles automatically.

When started on several machines and enough machines have joined, the _Starters_
will start _Agents_, _Coordinators_ and _DBservers_ on these machines.

When running the _Starter_ will supervise its child tasks (namely _Coordinators_,
_DBservers_ and _Agents_) and restart them in case of failure.

To start the cluster using a `systemd` unit file use the following:

```text
[Unit]
Description=Run the ArangoDB Starter
After=network.target

[Service]
Restart=on-failure
EnvironmentFile=/etc/arangodb.env
EnvironmentFile=/etc/arangodb.env.local
Environment=DATADIR=/var/lib/arangodb/cluster
ExecStartPre=/usr/bin/sh -c "mkdir -p ${DATADIR}"
ExecStart=/usr/bin/arangodb \
    --starter.address=${PRIVATEIP} \
    --starter.data-dir=${DATADIR} \
    --starter.join=${STARTERENDPOINTS} \
    --server.storage-engine=rocksdb \
    --auth.jwt-secret=${CLUSTERSECRETPATH}
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
```

Note that we set `rocksdb` in the unit service file.

## Cluster authentication

The communication between the cluster nodes use a token (JWT) to authenticate.
This must be shared between cluster nodes.

Sharing secrets is obviously a very delicate topic. The above workflow assumes
that the operator will put a secret in a file named `${CLUSTERSECRETPATH}`.

We recommend to use a dedicated system for managing secrets like HashiCorps' `Vault` or the
secret management of `DC/OS`.

## Required ports

As soon as enough machines have joined, the _Starter_ will begin starting _Agents_,
_Coordinators_ and _DBservers_.

Each of these tasks needs a port to communicate. Please make sure that the following
ports are available on all machines:

- `8529` for Coordinators
- `8530` for DBservers
- `8531` for Agents

The _Starter_ itself will use port `8528`.

## Recommended deployment environment

Since the _Agents_ are so critical to the availability of both the ArangoDB and the ArangoSync cluster,
it is recommended to run _Agents_ on dedicated machines. Consider these machines "pets".

_Coordinators_ and _DBServers_ can be deployed on other machines that should be considered "cattle".
