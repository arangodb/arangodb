Repair Jobs
===========

distributeShardsLike
--------------------

Before versions 3.2.12 and 3.3.4 there was a bug in the collection creation
which could lead to a violation of the property that its shards were
distributed on the DBServers exactly as the prototype collection from the
`distributeShardsLike` setting.

**Please read everything carefully before using this API!** 

There is a job that can restore this property safely. However, while the
job is running,
- the `replicationFactor` *must not be changed* for any affected collection or
  prototype collection (i.e. set in `distributeShardsLike`, including
  [SmartGraphs](../../Manual/Graphs/SmartGraphs/index.html)),
- *neither should shards be moved* of one of those prototypes
- and shutdown of DBServers *should be avoided*
during the repairs. Also only one repair job should run at any given time.
Failure to meet those requirements will mostly cause the job to abort, but still
allow to restart it safely. However, changing the `replicationFactor` during
repairs may leave it in a state that is not repairable without manual
intervention!

Shutting down the coordinator which executes the job will abort it, but it can
safely be restarted on another coordinator. However, there may still be a shard
move ongoing even after the job stopped. If the job is started again before the
move is finished, repairing the affected collection will fail, but the repair
can be restarted safely.

If there is any affected collection which `replicationFactor` is equal to
the total number of DBServers, the repairs might abort. In this case, it is
necessary to reduce the `replicationFactor` by one (or add a DBServer). The
job will not do that automatically.

Generally, the job will abort if any of its assumptions fail, at the start
or during the repairs. It can be started again and will resume from the
current state.

### Testing with `GET /_admin/repairs/distributeShardsLike`

Using `GET` will **not** trigger any repairs, but only calculate and return
the operations necessary to repair the cluster. This way, you can also
check if there is something to repair.

```
$ wget -qSO - http://localhost:8529/_admin/repair/distributeShardsLike | jq .
  HTTP/1.1 200 OK
  X-Content-Type-Options: nosniff
  Server: ArangoDB
  Connection: Keep-Alive
  Content-Type: application/json; charset=utf-8
  Content-Length: 53
{
  "error": false,
  "code": 200,
  "message": "Nothing to do."
}
```

In the example above, all collections with `distributeShardsLike` have their
shards distributed correctly. The response if something is broken looks like
this:

```json
{
  "error": false,
  "code": 200,
  "collections": {
    "_system/someCollection": {
      "PlannedOperations": [
        {
          "BeginRepairsOperation": {
            "database": "_system",
            "collection": "someCollection",
            "distributeShardsLike": "aPrototypeCollection",
            "renameDistributeShardsLike": true,
            "replicationFactor": 4
          }
        },
        {
          "MoveShardOperation": {
            "database": "_system",
            "collection": "someCollection",
            "shard": "s2000109",
            "from": "PRMR-6b8c84be-1e80-4085-9065-177c6e31a702",
            "to": "PRMR-d3e62c96-c3f7-4766-bac6-f3bf8026f59a",
            "isLeader": false
          }
        },
        {
          "MoveShardOperation": {
            "database": "_system",
            "collection": "someCollection",
            "shard": "s2000109",
            "from": "PRMR-ee3d7af6-1fbf-4ab7-bfd1-56d0a1c1c9b9",
            "to": "PRMR-6b8c84be-1e80-4085-9065-177c6e31a702",
            "isLeader": true
          }
        },
        {
          "FixServerOrderOperation": {
            "database": "_system",
            "collection": "someCollection",
            "distributeShardsLike": "aPrototypeCollection",
            "shard": "s2000109",
            "distributeShardsLikeShard": "s2000092",
            "leader": "PRMR-6b8c84be-1e80-4085-9065-177c6e31a702",
            "followers": [
              "PRMR-99c2ac17-f417-4710-82aa-8350417dd089",
              "PRMR-3b0b85de-882b-4eb2-bbf2-ef1018bdc81e",
              "PRMR-d3e62c96-c3f7-4766-bac6-f3bf8026f59a"
            ],
            "distributeShardsLikeFollowers": [
              "PRMR-d3e62c96-c3f7-4766-bac6-f3bf8026f59a",
              "PRMR-99c2ac17-f417-4710-82aa-8350417dd089",
              "PRMR-3b0b85de-882b-4eb2-bbf2-ef1018bdc81e"
            ]
          }
        },
        {
          "FinishRepairsOperation": {
            "database": "_system",
            "collection": "someCollection",
            "distributeShardsLike": "aPrototypeCollection",
            "shards": [
              {
                "shard": "s2000109",
                "protoShard": "s2000092",
                "dbServers": [
                  "PRMR-6b8c84be-1e80-4085-9065-177c6e31a702",
                  "PRMR-d3e62c96-c3f7-4766-bac6-f3bf8026f59a",
                  "PRMR-99c2ac17-f417-4710-82aa-8350417dd089",
                  "PRMR-3b0b85de-882b-4eb2-bbf2-ef1018bdc81e"
                ]
              },
              {
                "shard": "s2000110",
                "protoShard": "s2000093",
                "dbServers": [
                  "PRMR-d3e62c96-c3f7-4766-bac6-f3bf8026f59a",
                  "PRMR-ee3d7af6-1fbf-4ab7-bfd1-56d0a1c1c9b9",
                  "PRMR-6b8c84be-1e80-4085-9065-177c6e31a702",
                  "PRMR-99c2ac17-f417-4710-82aa-8350417dd089"
                ]
              },
[...]
            ]
          }
        }
      ],
      "error": false
    }
  }
}
```

If something is to be repaired, the response will have the property
`collections` with an entry `<db>/<collection>` for each collection which
has to be repaired. Each collection also as a separate `error` property
which will be `true` iff an error occurred for this collection (and `false`
otherwise). If `error` is `true`, the properties `errorNum` and
`errorMessage` will also be set, and in some cases also `errorDetails`
with additional information on how to handle a specific error.

### Repairing with `POST /_admin/repairs/distributeShardsLike`

As this job possibly has to move a lot of data around, it can take a while
depending on the size of the affected collections. So this should *not
be called synchronously*, but only via
[Async Results](../../HTTP/AsyncResultsManagement/index.html): i.e., set the
header `x-arango-async: store` to put the job into background and get
its results later. Otherwise the request will most probably result in a
timeout and the response will be lost! The job will still continue unless
the coordinator is stopped, but there is no way to find out if it is
still running, or get success or error information afterwards.

Starting the job in background can be done like so:

```
$ wget --method=POST --header='x-arango-async: store' -qSO - http://localhost:8529/_admin/repair/distributeShardsLike 
  HTTP/1.1 202 Accepted
  X-Content-Type-Options: nosniff
  X-Arango-Async-Id: 152223973119118
  Server: ArangoDB
  Connection: Keep-Alive
  Content-Type: text/plain; charset=utf-8
  Content-Length: 0
```

This line is of notable importance:
```
  X-Arango-Async-Id: 152223973119118
```
as it contains the job id which can be used to fetch the state and results
of the job later. `GET`ting `/_api/job/pending` and `/_api/job/done` will list
job ids of jobs that are pending or done, respectively.

This can also be done with the `GET` method for testing.

The job api must be used to fetch the state and results. It will return
a `204` while the job is running. The actual response will be returned
only once, after that the job is deleted and the api will return a `404`.
It is therefore recommended to write the response directly to a file for
later inspection. Fetching the result is done by calling `/_api/job` via
`PUT`: 

```
$ wget --method=PUT -qSO - http://localhost:8529/_api/job/152223973119118 | jq .
  HTTP/1.1 200 OK
  X-Content-Type-Options: nosniff
  X-Arango-Async-Id: 152223973119118
  Server: ArangoDB
  Connection: Keep-Alive
  Content-Type: application/json; charset=utf-8
  Content-Length: 53
{
  "error": false,
  "code": 200,
  "message": "Nothing to do."
}
```

The final response will look like the response of the `GET` call.
If an error occurred the response should contain details on how to proceed.
If in doubt, ask as on Slack: https://arangodb.com/community/
