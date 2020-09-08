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
  QUERY_TIME: "arangodb_aql_total_query_time_msec",
  PHASE_1_BUCKET: "arangodb_maintenance_phase1_runtime_msec_bucket",
  PHASE_1_COUNT: "arangodb_maintenance_phase1_runtime_msec_count",
  PHASE_2_BUCKET: "arangodb_maintenance_phase2_runtime_msec_bucket",
  PHASE_2_COUNT: "arangodb_maintenance_phase2_runtime_msec_count",
  SHARD_COUNT: "arangodb_shards_total_count",
  SHARD_LEADER_COUNT: "arangodb_shards_leader_count",
  HEARTBEAT_BUCKET: "arangodb_heartbeat_send_time_msec_bucket",
  HEARTBEAT_COUNT: "arangodb_heartbeat_send_time_msec_count",
  SUPERVISION_BUCKET: "arangodb_agency_supervision_runtime_msec_bucket",
  SUPERVISION_COUNT: "arangodb_agency_supervision_runtime_msec_count",
  SLOW_QUERY_COUNT: "arangodb_aql_slow_query",
  CLIENT_CONNECTIONS: "arangodb_client_connection_statistics_client_connections"
};


class Watcher {
  constructor(metric) {
    this._metric = metric;
    this._before = null;
    this._after = null;
  }

  before(metrics) {
    this._before = metrics[this._metric];
  };

  after(metrics) {
    this._after =  metrics[this._metric];
  };

  check(){
    expect(this._after).to.be.greaterThan(this._before);
  };
}

class BucketWatcher extends Watcher{  

  check(){
    expectOneBucketChanged(this._after, this._before);
  };
}

class QueryTimeWatcher extends Watcher {
  constructor(minChange) {
    super(MetricNames.QUERY_TIME);
    this._minChange = minChange;
  };

  check (){    
    expect(this._after).to.be.at.least(this._before + this._minChange);
  };
}


class SlowQueryCountWatcher extends Watcher {
  constructor(minChange) {
    super(MetricNames.SLOW_QUERY_COUNT);
    this._minChange = minChange;
  };
  
  check (){
    expect(this._after).to.be.equal(this._before + this._minChange);    
  };
}


class ConnecntionCountWatcher extends Watcher {
  constructor(minChange) {
    super(MetricNames.CLIENT_CONNECTIONS);
    this._minChange = minChange;
  }

  check (){
    expect(this._after).to.be.at.least(this._before + this._minChange);
  };
}


class ShardCountWatcher extends Watcher {
  constructor(change) {
    super(MetricNames.SHARD_COUNT);
    this._change = change;
  }

  check (){
    expect(this._after).to.be.equal(this._before + this._change);
  };
}


class ShardLeaderCountWatcher extends Watcher {
  constructor(change) {
    super(MetricNames.SHARD_LEADER_COUNT);
    this._change = change;
  }

  check (){
    expect(this._after).to.be.equal(this._before + this._change);
  }
}

class MaintenanceWatcher {
  constructor() {
    this._p1ValueWatcher = new Watcher(MetricNames.PHASE_1_COUNT);
    this._p2ValueWatcher = new Watcher(MetricNames.PHASE_2_COUNT);
    this._p1BucketWatcher = new BucketWatcher(MetricNames.PHASE_1_BUCKET);
    this._p2BucketWatcher = new BucketWatcher(MetricNames.PHASE_2_BUCKET);
  }

  before(metrics) {
    this._p1ValueWatcher.before(metrics);
    this._p2ValueWatcher.before(metrics);
    this._p1BucketWatcher.before(metrics);
    this._p2BucketWatcher.before(metrics);
  };

  after(metrics) {
    this._p1ValueWatcher.after(metrics);
    this._p2ValueWatcher.after(metrics);
    this._p1BucketWatcher.after(metrics);
    this._p2BucketWatcher.after(metrics);
  };

  check(){
    this._p1ValueWatcher.check();
    this._p2ValueWatcher.check();
    this._p1BucketWatcher.check();
    this._p2BucketWatcher.check();
  }
}

class HeartBeatWatcher {
  constructor() {
    this._ValueWatcher = new Watcher(MetricNames.HEARTBEAT_COUNT);
    this._BucketWatcher = new BucketWatcher(MetricNames.HEARTBEAT_BUCKET);
  }

  before(metrics) {
    this._ValueWatcher.before(metrics);
    this._BucketWatcher.before(metrics);
  };

  after(metrics) {
    this._ValueWatcher.after(metrics);
    this._BucketWatcher.after(metrics);
  };

  check(){
    this._ValueWatcher.check();
    this._BucketWatcher.check();
  }
}

class SupervisionWatcher {
  constructor() {
    this._svValueWatcher = new Watcher(MetricNames.SUPERVISION_COUNT);
    this._svBucketWatcher = new BucketWatcher(MetricNames.SUPERVISION_BUCKET);
  }

  before(metrics) {
    this._svValueWatcher.before(metrics);
    this._svBucketWatcher.before(metrics);
  };

  after(metrics) {
    this._svValueWatcher.after(metrics);
    this._svBucketWatcher.after(metrics);
  };

  check(){
    this._svValueWatcher.check();
    this._svBucketWatcher.check();
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

  const runTest = (action, watchers, role) => {
    const metricsBefore = loadAllMetrics(role);
    watchers.forEach(w => {w.before(metricsBefore);});

    action();
    const metricsAfter = loadAllMetrics(role);
    watchers.forEach(w => {
      w.after(metricsAfter);
      w.check();
    });      
  };

  it('aql query time', () => {
    runTest(() => {
      const query = `return sleep(1)`;
      db._query(query);
    }, [new QueryTimeWatcher(1000)], 'coordinator');
  });

  it('aql slow query count ', () => {
    runTest(() => {
      const queries = require("@arangodb/aql/queries");
      const oldThreshold = queries.properties().slowQueryThreshold;
      queries.properties({slowQueryThreshold: 1});
      db._query(`return sleep(1)`);
      queries.properties({slowQueryThreshold: oldThreshold});       
    }, [new SlowQueryCountWatcher(1)], 'coordinator');
  });
    
  // it('client connections count ', () => {
  //   runTest(() => {
  //     //conect
  //     const url = `${servers.get("coordinator")[0]}/_admin/metrics`;

  //     const res = request({
  //       json: true,
  //       method: 'GET',
  //       url
  //     });
  //     expect(res.statusCode).to.equal(200);

  //   }, [new ConnecntionCountWatcher(1)]);
  // });

  it('collection and index', () => {
    try {
      runTest(() => {
        db._create("UnitTestCollection", {numberOfShards: 9, replicationFactor: 2}, undefined, {waitForSyncReplication: true});
        require("internal").wait(10.0); // database servers update their shard count in phaseOne. So lets wait until all have done their next phaseOne.
      },
      [new MaintenanceWatcher(), new ShardCountWatcher(18), new ShardLeaderCountWatcher(9)],
      "dbserver"
      );
      runTest(() => {
        db["UnitTestCollection"].ensureHashIndex("temp");
      },
      [new MaintenanceWatcher()],
      "dbserver"
      );
    } finally {
      db._drop("UnitTestCollection");
    }
  });

  it('at least 1 heartbeat and supervision per second', () => {
    runTest(() => {
      require("internal").wait(1.0);
    }, [new HeartBeatWatcher()], "dbserver");

    runTest(() => {
      require("internal").wait(1.0);
    }, [new HeartBeatWatcher()], "coordinator");

    runTest(() => {
      require("internal").wait(1.0);
    }, [new SupervisionWatcher()], "agent");

  });

    
});
