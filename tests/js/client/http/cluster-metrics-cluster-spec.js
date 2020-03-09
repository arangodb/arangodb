/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, beforeEach, afterEach, it, global */
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
  HEARTBEAT_BUCKET: "heartbeat_send_time_msec_bucket",
  QUERY_TIME: "arangodb_aql_total_query_time_ms",
  PHASE_1_BUCKET: "maintenance_phase1_runtime_msec_bucket",
  PHASE_1_COUNT: "maintenance_phase1_runtime_msec_count",
  PHASE_2_BUCKET: "maintenance_phase2_runtime_msec_bucket",
  PHASE_2_COUNT: "maintenance_phase2_runtime_msec_count", 
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
    this._metric = metric
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
    this._metric = metric
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
    this._metric = metric
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
    this._metric = metric
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


class QueryTimeWatcher extends CoordinatorValueWatcher {
  constructor(minChange) {
    super(MetricNames.QUERY_TIME);
    this._minChange = minChange;
  }

  afterCoordinator(metrics) {
    const after =  metrics[this._metric];
    expect(after).to.be.greaterThan(this._before + this._minChange);
  };
}

class MaintenanceWatcher extends Watcher {
  constructor() {
    super();
    this._p1ValueWatcher = new DBServerValueWatcher(MetricNames.PHASE_1_COUNT);
    this._p2ValueWatcher = new DBServerValueWatcher(MetricNames.PHASE_2_COUNT);
    this._p1BuckerWatcher = new DBServerBucketWatcher(MetricNames.PHASE_1_BUCKET);
    this._p2BuckerWatcher = new DBServerBucketWatcher(MetricNames.PHASE_2_BUCKET);
  }

  beforeDBServer(metrics) {
    this._p1ValueWatcher.beforeDBServer(metrics);
    this._p2ValueWatcher.beforeDBServer(metrics);
    this._p1BuckerWatcher.beforeDBServer(metrics);
    this._p2BuckerWatcher.beforeDBServer(metrics);
  };

  afterDBServer(metrics) {
    this._p1ValueWatcher.afterDBServer(metrics);
    this._p2ValueWatcher.afterDBServer(metrics);
    this._p1BuckerWatcher.afterDBServer(metrics);
    this._p2BuckerWatcher.afterDBServer(metrics);
  };

}




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
    
    const prometheusToJson = (prometheus) => {
      const lines = prometheus.split('\n').filter((s) => !s.startsWith('#') && s !== '');
      const res = {};
      for (const l of lines) {
        const [key, count] = l.split(' ');
        if (key.indexOf('{le="') !== -1) {
          // Bucket case
          // We only check for:
          // identifier{le="range"}
          // For all other buckt-types this code will fail
          const [base, bucket] = key.split('{le="');
          res[base] = res[base] || {};
          res[base][bucket.substr(0, bucket.length -2)] = parseFloat(count);
        } else {
          // evertyhing else
          res[key] = parseFloat(count);
        }

      }
      return res;
    };

    const loadMetrics = (role, idx) =>  {
      const url = `${servers.get(role)[idx]}/_admin/metrics`;
      
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
            lhs[key][bucket] += content 
          }
        } else {
          lhs[key] += rhs[key];
        }
      }
      return lhs;
    }

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

    it('maintenance', () => {
      try {
        runTest(() => {
          db._create("UnitTestCollection", {numberOfShards: 9});
        },
        [new MaintenanceWatcher()]
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
});