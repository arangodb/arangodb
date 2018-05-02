Starting Manually
=================

We are going to start two single server instances and one _Agency_.

First we need to start the _Agent_:

```bash
    arangod \
        --agency.activate true \
        --agency.endpoint tcp://agency.domain.org:4001 \
        --agency.my-address tcp://agency.domain.org:4001 \
        --agency.pool-size 1 \
        --agency.size 1 \
        --database.directory dbdir/data4001 \
        --javascript.v8-contexts 1 \
        --server.endpoint tcp://agency.domain.org:4001 \
        --server.statistics false \
        --server.threads 16 \
        --log.file dbdir/4001.log \
        --log.level INFO \
        | tee dbdir/4001.stdout 2>&1 &
```

Next we are going to start the _leader_ (wait until this server is fully started)

```bash
    arangod \
      --database.directory dbdir/data8530 \
      --cluster.agency-endpoint tcp://agency.domain.org:4001 \
      --cluster.my-address tcp://leader.domain.org:4001 \
      --server.endpoint tcp://leader.domain.org:4001 \
      --cluster.my-role SINGLE \
      --replication.active-failover true \
      --log.file dbdir/8530.log \
      --server.statistics true \
      --server.threads 5 \
      | tee cluster/8530.stdout 2>&1 &
```

After the _leader_ server is fully started then you can add additional _followers_,
with basically the same startup parameters (except for their address and database directory)

```bash
    arangod \
      --database.directory dbdir/data8531 \
      --cluster.agency-endpoint tcp://agency.domain.org:4001 \
      --cluster.my-address tcp://follower.domain.org:4001 \
      --server.endpoint tcp://follower.domain.org:4001 \
      --cluster.my-role SINGLE \
      --replication.active-failover true \
      --log.file dbdir/8531.log \
      --server.statistics true \
      --server.threads 5 \
      | tee cluster/8531.stdout 2>&1 &
```
