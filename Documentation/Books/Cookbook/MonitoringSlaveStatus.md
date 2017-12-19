#Monitoring replication slave

**Note**: this recipe is working with ArangoDB 2.5, you need a collectd curl_json plugin with correct boolean type mapping.

##Problem
How to monitor the slave status using the `collectd curl_JSON` plugin.

##Solution
Since arangodb [reports the replication status in JSON](https://docs.arangodb.com/HttpReplications/ReplicationApplier.html#state-of-the-replication-applier),
integrating it with the [collectd curl_JSON plugin](MonitoringWithCollectd.html)
should be an easy exercise. However, only very recent versions of collectd will handle boolean flags correctly.

Our test master/slave setup runs with the the master listening on `tcp://127.0.0.1:8529` and the slave (which we query) listening on `tcp://127.0.0.1:8530`.
They replicate a dabatase by the name `testDatabase`.

Since replication appliers are active per database and our example doesn't use the default `_system`, we need to specify its name in the URL like this: `_db/testDatabase`.

We need to parse a document from a request like this:

    curl --dump - http://localhost:8530/_db/testDatabase/_api/replication/applier-state

If the replication is not running the document will look like that:

```javascript
{
  "state": {
    "running": false,
    "lastAppliedContinuousTick": null,
    "lastProcessedContinuousTick": null,
    "lastAvailableContinuousTick": null,
    "safeResumeTick": null,
    "progress": {
      "time": "2015-11-02T13:24:07Z",
      "message": "applier shut down",
      "failedConnects": 0
    },
    "totalRequests": 1,
    "totalFailedConnects": 0,
    "totalEvents": 0,
    "totalOperationsExcluded": 0,
    "lastError": {
      "time": "2015-11-02T13:24:07Z",
      "errorMessage": "no start tick",
      "errorNum": 1413
    },
    "time": "2015-11-02T13:31:53Z"
  },
  "server": {
    "version": "2.7.0",
    "serverId": "175584498800385"
  },
  "endpoint": "tcp://127.0.0.1:8529",
  "database": "testDatabase"
}
```

A running replication will return something like this:

```javascript
{
  "state": {
    "running": true,
    "lastAppliedContinuousTick": "1150610894145",
    "lastProcessedContinuousTick": "1150610894145",
    "lastAvailableContinuousTick": "1151639153985",
    "safeResumeTick": "1150610894145",
    "progress": {
      "time": "2015-11-02T13:49:56Z",
      "message": "fetching master log from tick 1150610894145",
      "failedConnects": 0
    },
    "totalRequests": 12,
    "totalFailedConnects": 0,
    "totalEvents": 2,
    "totalOperationsExcluded": 0,
    "lastError": {
      "errorNum": 0
    },
    "time": "2015-11-02T13:49:57Z"
  },
  "server": {
    "version": "2.7.0",
    "serverId": "175584498800385"
  },
  "endpoint": "tcp://127.0.0.1:8529",
  "database": "testDatabase"
}
```

We create a simple collectd configuration in `/etc/collectd/collectd.conf.d/slave_testDatabase.conf` that matches our API:

```javascript
TypesDB "/etc/collectd/collectd.conf.d/slavestate_types.db"
<Plugin curl_json>
  # Adjust the URL so collectd can reach your arangod slave instance:
  <URL "http://localhost:8530/_db/testDatabase/_api/replication/applier-state">
   # Set your authentication to that database here:
   # User "foo"
   # Password "bar"
    <Key "state/running">
       Type "boolean"
     </Key>
    <Key "state/totalOperationsExcluded">
       Type "counter"
     </Key>
    <Key "state/totalRequests">
       Type "counter"
     </Key>
    <Key "state/totalFailedConnects">
       Type "counter"
     </Key>
  </URL>
</Plugin>
```

To get nice metric names, we specify our own `types.db` file in `/etc/collectd/collectd.conf.d/slavestate_types.db`:

```
boolean                     value:ABSOLUTE:0:1
```

So, basically `state/running` will give you `0`/`1` if its (not / ) running through the collectd monitor.


**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:** #monitoring #foxx #json
    


