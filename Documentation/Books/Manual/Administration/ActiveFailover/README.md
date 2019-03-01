Active Failover Administration
==============================

This _Section_ includes information related to the administration of an _Active Failover_
setup.

For a general introduction to the ArangoDB _Active Failover_ setup, please refer
to the _Active Failover_ [chapter](../../Architecture/DeploymentModes/ActiveFailover/README.md).

Introduction
------------

The _Active Failover_ setup requires almost no manual administration.

You may still need to replace, upgrade or remove individual nodes
in an _Active Failover_ setup.


Determining the current _Leader_
--------------------------------

It is possible to determine the _leader_ by asking any of the involved single-server
instances. Just send a request to the `/_api/cluster/endpoints` REST API.

```bash
curl http://server.domain.org:8530/_api/cluster/endpoints
{
  "error": false,
  "code": 200,
  "endpoints": [
    {
      "endpoint": "tcp://[::1]:8530"
    },
    {
      "endpoint": "tcp://[::1]:8531"
    }
  ]
}
```

This API will return you all available endpoints, the first endpoint is defined to
be the current _Leader_. This endpoint is always available and will not be blocked
with a `HTTP/1.1 503 Service Unavailable` response on a _Follower_

Reading from Follower
---------------------

Followers in the active-failover setup are in a read-only mode. It is possible to read from these
followers by adding a `X-Arango-Allow-Dirty-Read: true` header on each request. Responses will then automatically
contain the `X-Arango-Potential-Dirty-Read` header so that clients can reject accidental dirty reads.

Depending on the driver support for your specific programming language, you should be able to enable this option.

Upgrading / Replacing / Removing a _Leader_
-------------------------------------------

A _Leader_ is the active server which can receive all read and write operations
in an _Active-Failover_ setup.

Upgrading or removing a _Leader_ can be a little tricky, because as soon as you
stop the leader's process you will trigger a failover situation. This can be intended
here, but you will probably want to halt all writes to the _leader_ for a certain
amount of time to allow the _follower_ to catch up on all operations.

After you have ensured that the _follower_ is sufficiently caught up, you can 
stop the _leader_ process via the shutdown API or by sending a `SIGTERM` signal
to the process (i.e. `kill <process-id>`). This will trigger an orderly shutdown,
and should trigger an immediate switch to the _follower_. If your client drivers
are configured correctly, you should notice almost no interruption in your
applications.

Once you upgraded the local server via the `--database.auto-upgrade` option,
you can add it again to the _Active Failover_ setup. The server will resync automatically
with the new _Leader_ and become a _Follower_.

Upgrading / Replacing / Removing a _Follower_
---------------------------------------------

A _Follower_ is the passive server which tries to mirror all the data stored in
the _Leader_.

To upgrade a _follower_ you only need to stop the process and start it
with `--database.auto-upgrade`. The server process will automatically resync
with the master after a restart.

The clean way of removing a _Follower_ is to first start a replacement _Follower_
(otherwise you will lose resiliency). To start a _Follower_ please have a look
into our [deployment guide](../../Deployment/ActiveFailover/README.md).
After you have your replacement ready you can just kill the process and remove it.
