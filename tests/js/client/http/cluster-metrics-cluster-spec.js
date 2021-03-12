/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, beforeEach, afterEach, it, global, before,  */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const {expect} = require('chai');
const request = require("@arangodb/request");
const db = require("internal").db;


const expectOneBucketChanged = (actual, old) => {
  let foundOne = false;
  for (const [key, value] of Object.entries(actual)) {
    if (old[key] < value) {
      foundOne = true;
    }
    expect(old[key]).to.be.most(value);

  }
  expect(foundOne).to.equal(true);
};

const MetricNames = {
  QUERY_TIME: "arangodb_aql_total_query_time_msec_total",
  PHASE_1_BUCKET: "arangodb_maintenance_phase1_runtime_msec_bucket",
  PHASE_1_COUNT: "arangodb_maintenance_phase1_runtime_msec_count",
  PHASE_2_BUCKET: "arangodb_maintenance_phase2_runtime_msec_bucket",
  PHASE_2_COUNT: "arangodb_maintenance_phase2_runtime_msec_count",
  SHARD_COUNT: "arangodb_shards_number",
  SHARD_LEADER_COUNT: "arangodb_shards_leader_number",
  HEARTBEAT_BUCKET: "arangodb_heartbeat_send_time_msec_bucket",
  HEARTBEAT_COUNT: "arangodb_heartbeat_send_time_msec_count",
  SUPERVISION_BUCKET: "arangodb_agency_supervision_runtime_msec_bucket",
  SUPERVISION_COUNT: "arangodb_agency_supervision_runtime_msec_count"
};

class Watcher {
  beforeCoordinator(metrics) {};
  afterCoordinator(metrics) {};
  beforeDBServer(metrics) {};
  afterDBServer(metrics) {};
  beforeAgent(metrics) {};
  afterAgent(metrics) {};
}

class CoordinatorValueWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeCoordinator(metrics) {
    this._before = metrics[this._metric];
  };

  afterCoordinator(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.greaterThan(this._before);
  };
}


class CoordinatorBucketWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeCoordinator(metrics) {
    this._before = metrics[this._metric];
  };

  afterCoordinator(metrics) {
    const after =  metrics[this._metric];
    expectOneBucketChanged(after, this._before);
  };
}


class DBServerValueWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeDBServer(metrics) {
    this._before = metrics[this._metric];
  };

  afterDBServer(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.greaterThan(this._before);
  };
}


class DBServerBucketWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeDBServer(metrics) {
    this._before = metrics[this._metric];
  };

  afterDBServer(metrics) {
    const after =  metrics[this._metric];
    expectOneBucketChanged(after, this._before);
  };
}


class AgentValueWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeAgent(metrics) {
    this._before = metrics[this._metric];
  };

  afterAgent(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.greaterThan(this._before);
  };
}


class AgentBucketWatcher extends Watcher {
  constructor(metric) {
    super();
    this._metric = metric;
    this._before = null;
  }

  beforeAgent(metrics) {
    this._before = metrics[this._metric];
  };

  afterAgent(metrics) {
    const after =  metrics[this._metric];
    expectOneBucketChanged(after, this._before);
  };
}

class QueryTimeWatcher extends CoordinatorValueWatcher {
  constructor(minChange) {
    super(MetricNames.QUERY_TIME);
    this._minChange = minChange;
  }

  afterCoordinator(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.at.least(this._before + this._minChange);
  };
}

class ShardCountWatcher extends DBServerValueWatcher {
  constructor(change) {
    super(MetricNames.SHARD_COUNT);
    this._change = change;
  }

  afterDBServer(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.equal(this._before + this._change);
  };
}

class ShardLeaderCountWatcher extends DBServerValueWatcher {
  constructor(change) {
    super(MetricNames.SHARD_LEADER_COUNT);
    this._change = change;
  }

  afterDBServer(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.equal(this._before + this._change);
  };
}

class MaintenanceWatcher extends Watcher {
  constructor() {
    super();
    this._p1ValueWatcher = new DBServerValueWatcher(MetricNames.PHASE_1_COUNT);
    this._p2ValueWatcher = new DBServerValueWatcher(MetricNames.PHASE_2_COUNT);
    this._p1BucketWatcher = new DBServerBucketWatcher(MetricNames.PHASE_1_BUCKET);
    this._p2BucketWatcher = new DBServerBucketWatcher(MetricNames.PHASE_2_BUCKET);
  }

  beforeDBServer(metrics) {
    this._p1ValueWatcher.beforeDBServer(metrics);
    this._p2ValueWatcher.beforeDBServer(metrics);
    this._p1BucketWatcher.beforeDBServer(metrics);
    this._p2BucketWatcher.beforeDBServer(metrics);
  };

  afterDBServer(metrics) {
    this._p1ValueWatcher.afterDBServer(metrics);
    this._p2ValueWatcher.afterDBServer(metrics);
    this._p1BucketWatcher.afterDBServer(metrics);
    this._p2BucketWatcher.afterDBServer(metrics);
  };
}

class HeartBeatWatcher extends Watcher {
  constructor() {
    super();
    this._dbhbValueWatcher = new DBServerValueWatcher(MetricNames.HEARTBEAT_COUNT);
    this._dbhbBucketWatcher = new DBServerBucketWatcher(MetricNames.HEARTBEAT_BUCKET);
    this._coordhbValueWatcher = new CoordinatorValueWatcher(MetricNames.HEARTBEAT_COUNT);
    this._coordhbBucketWatcher = new CoordinatorBucketWatcher(MetricNames.HEARTBEAT_BUCKET);
  }

  beforeDBServer(metrics) {
    this._dbhbValueWatcher.beforeDBServer(metrics);
    this._dbhbBucketWatcher.beforeDBServer(metrics);
  };

  afterDBServer(metrics) {
    this._dbhbValueWatcher.afterDBServer(metrics);
    this._dbhbBucketWatcher.afterDBServer(metrics);
  };

  beforeCoordinator(metrics) {
    this._coordhbValueWatcher.beforeCoordinator(metrics);
    this._coordhbBucketWatcher.beforeCoordinator(metrics);
  };

  afterCoordinator(metrics) {
    this._coordhbValueWatcher.afterCoordinator(metrics);
    this._coordhbBucketWatcher.afterCoordinator(metrics);
  };
}

class SupervisionWatcher extends Watcher {
  constructor() {
    super();
    this._svValueWatcher = new AgentValueWatcher(MetricNames.SUPERVISION_COUNT);
    this._svBucketWatcher = new AgentBucketWatcher(MetricNames.SUPERVISION_BUCKET);
  }


  beforeAgent(metrics) {
    this._svValueWatcher.beforeAgent(metrics);
    this._svBucketWatcher.beforeAgent(metrics);
  };

  afterAgent(metrics) {
    this._svValueWatcher.afterAgent(metrics);
    this._svBucketWatcher.afterAgent(metrics);
  };
};




describe('_admin/metrics', () => {

  const getServers = () => {
    const endpointToURL = (endpoint) => {
      if (endpoint.substr(0, 6) === 'ssl://') {
        return 'https://' + endpoint.substr(6);
      }
      var pos = endpoint.indexOf('://');
      if (pos === -1) {
        return 'http://' + endpoint;
      }
      return 'http' + endpoint.substr(pos);
    };
    const {instanceInfo} = global;
    const list = new Map();
    list.set("coordinator", []);
    list.set("dbserver", []);
    list.set("agent", []);
    for (const d of instanceInfo.arangods) {
      const {role, endpoint} = d;
      list.get(role).push(endpointToURL(endpoint));
    }
    return list;
  };

  let servers;

  before(() => {
    servers = getServers();
  });

  const extractKeyAndLabel = (key) => {
    const start = key.indexOf('{');
    const labels = new Map();
    if (start === -1) {
      return [key, labels];
    }
    const labelPart = key.substring(start + 1, key.length - 1);
    for (const l of labelPart.split(",")) {
      const [lab, val] = l.split("=");
      labels.set(lab,val);
    }
    return [
      key.substring(0, start),
      labels
    ];
  };

    const prometheusToJson = (prometheus) => {
      const lines = prometheus.split('\n').filter((s) => !s.startsWith('#') && s !== '');
      const res = {};
      for (const l of lines) {
        const [keypart, count] = l.split(' ');
        const [key, labels] = extractKeyAndLabel(keypart);
        if (labels.has("le")) {
          // Bucket case
          // We only check for:
          // identifier{le="range"}
          // For all other bucket-types this code will fail
          res[key] = res[key] || {};
          res[key][labels.get("le")] = parseFloat(count);
        } else {
          // evertyhing else
          // We ignore other labels for now.
          res[key] = parseFloat(count);
        }

      }
      return res;
    };

    const loadMetrics = (role, idx) =>  {
      const url = `${servers.get(role)[idx]}/_admin/metrics/v2`;

      const res = request({
        json: true,
        method: 'GET',
        url
      });
      expect(res.statusCode).to.equal(200);
      return prometheusToJson(res.body);
    };

    const joinMetrics = (lhs, rhs) => {
      if (Object.entries(lhs).length === 0) {
        return rhs;
      }
      for (const [key, value] of Object.entries(rhs)) {
        if (value instanceof Object) {
          for (const [bucket, content] of Object.entries(value)) {
            lhs[key][bucket] += content;
          }
        } else {
          lhs[key] += rhs[key];
        }
      }
      return lhs;
    };

    const loadAllMetrics = (role) => {
      return servers.get(role).map((_, i) => loadMetrics(role, i)).reduce(joinMetrics, {});
     };

    const runTest = (action, watchers) => {
      const coordBefore = loadAllMetrics("coordinator");
      watchers.forEach(w => {w.beforeCoordinator(coordBefore);});

      const dbBefore = loadAllMetrics("dbserver");
      watchers.forEach(w => {w.beforeDBServer(dbBefore);});

      const agentsBefore = loadAllMetrics("agent");
      watchers.forEach(w => {w.beforeAgent(agentsBefore);});

      action();
      const coordAfter = loadAllMetrics("coordinator");
      watchers.forEach(w => {w.afterCoordinator(coordAfter);});

      const dbAfter = loadAllMetrics("dbserver");
      watchers.forEach(w => {w.afterDBServer(dbAfter);});

      const agentsAfter = loadAllMetrics("agent");
      watchers.forEach(w => {w.afterAgent(agentsAfter);});
    };

    it('aql query time', () => {
      runTest(() => {
        const query = `return sleep(1)`;
        db._query(query);
      }, [new QueryTimeWatcher(1000)]);
    });

    it('collection and index', () => {
      try {
        runTest(() => {
          db._create("UnitTestCollection", {numberOfShards: 9, replicationFactor: 2}, undefined, {waitForSyncReplication: true});
          require("internal").wait(10.0); // database servers update their shard count in phaseOne. So lets wait until all have done their next phaseOne.
        },
        [new MaintenanceWatcher(), new ShardCountWatcher(18), new ShardLeaderCountWatcher(9)]
        );
        runTest(() => {
          db["UnitTestCollection"].ensureHashIndex("temp");
        },
        [new MaintenanceWatcher()]
        );
      } finally {
        db._drop("UnitTestCollection");
      }
    });

    it('at least 1 heartbeat and supervision per second', () => {
      runTest(() => {
        require("internal").wait(1.0);
      }, [new HeartBeatWatcher(), new SupervisionWatcher()]);
    });
});
