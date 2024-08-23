/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, VPACK_TO_V8*/

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Valery Mironov
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const {getEndpointsByType, getRawMetric, getAllMetric} = require("@arangodb/test-helper");
const { checkIndexMetrics } = require("@arangodb/test-helper-common");
const parsePrometheusTextFormat = require("parse-prometheus-text-format");
const _ = require("lodash");
const isEnterprise = require("internal").isEnterprise();

const db = arangodb.db;

function checkMetrics(metrics) {

  assertNotEqual(null, metrics);
  assertNotEqual(undefined, metrics);

  assertNotEqual(undefined, metrics["arangodb_search_num_docs"]);
  assertNotEqual(undefined, metrics["arangodb_search_num_live_docs"]);

  assertNotEqual(undefined, metrics["arangodb_search_num_segments"]);
  assertNotEqual(undefined, metrics["arangodb_search_num_files"]);
  assertNotEqual(undefined, metrics["arangodb_search_index_size"]);
  if (isEnterprise) {
    // 'arangodb_search_num_primary_docs' is available only in enterprise.
    // So make sure that it is exists before checking other metrics.
    assertNotEqual(undefined, metrics["arangodb_search_num_primary_docs"]);
    
    // nested documents are treated like a real documents
    assertEqual(metrics["arangodb_search_num_docs"]["foo1"], 2000);
    assertEqual(metrics["arangodb_search_num_docs"]["foo2"], 4001);
    assertEqual(metrics["arangodb_search_num_docs"]["foo3"], 7002);
  
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo1"], 2000);
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo2"], 4001);
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo3"], 7002);

    assertEqual(metrics["arangodb_search_num_primary_docs"]["foo1"], 1000);
    assertEqual(metrics["arangodb_search_num_primary_docs"]["foo2"], 1001);
    assertEqual(metrics["arangodb_search_num_primary_docs"]["foo3"], 1002);
  } else {
    assertEqual(metrics["arangodb_search_num_docs"]["foo1"], 1000);
    assertEqual(metrics["arangodb_search_num_docs"]["foo2"], 1001);
    assertEqual(metrics["arangodb_search_num_docs"]["foo3"], 1002);
  
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo1"], 1000);
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo2"], 1001);
    assertEqual(metrics["arangodb_search_num_live_docs"]["foo3"], 1002);
  }

  assertEqual(metrics["arangodb_search_num_segments"]["foo1"], 9);
  assertEqual(metrics["arangodb_search_num_segments"]["foo2"], 12);
  assertEqual(metrics["arangodb_search_num_segments"]["foo3"], 1);

  assertEqual(metrics["arangodb_search_num_files"]["foo1"], 54);
  assertEqual(metrics["arangodb_search_num_files"]["foo2"], 72);
  assertEqual(metrics["arangodb_search_num_files"]["foo3"], 6);

  assertTrue(metrics["arangodb_search_index_size"]["foo1"] > 0);
  assertTrue(metrics["arangodb_search_index_size"]["foo2"] > 0);
  assertTrue(metrics["arangodb_search_index_size"]["foo3"] > 0);
}

function checkRawMetrics(txt, empty) {
  let json = parsePrometheusTextFormat(txt);
  let metrics = {};
  json.forEach((entry) => {
    entry.metrics.forEach((entry2) => {
      if (!_.has(entry2, "labels") || !_.has(entry2.labels, "collection")) {
        return;
      }
      if (!_.has(metrics, entry.name)) {
        metrics[entry.name] = {};
      }
      metrics[entry.name][entry2.labels.collection] = entry2.value;
    });
  });
  if (empty) {
    assertEqual(metrics, {});
  } else {
    checkMetrics(metrics);
  }
}

function checkCoordinators(coordinators, mode) {
  assertTrue(coordinators.length > 1);
  for (let i = 1; i < coordinators.length; i++) {
    let c = coordinators[i];
    checkIndexMetrics(function() {
      let txt = getAllMetric(c, mode);
      checkRawMetrics(txt, false);
    });
  }
}

function createClusterWideMetrics() {
  db._create("foo1", {"replicationFactor": 3, "numberOfShards": 9});
  db._create("foo2", {"replicationFactor": 2, "numberOfShards": 12});
  db._create("foo3", {"replicationFactor": 1, "numberOfShards": 1});
  let foo1_values = [];
  let foo2_values = [{"name": "i"}];
  let foo3_values = [{"name": "hate"}, {"name": "js"}];
  for (let i = 0; i !== 1000; ++i) {
    foo1_values.push({"name": i, "nested_1_1": [{"a": -i}]});
    foo2_values.push({"name": i + 100000, "nested_2_1": [{"c": -i}], "nested_2_2": [{"nested_2_1": [{"d": -i + 1}]}]});
    foo3_values.push({"name": i - 100000, 
                        "nested_3_1": [{"e": String(i)}], 
                        "nested_3_2": [{"nested_3_1": [{"f": String(i + 1)}]}], 
                        "nested_3_3": [{"nested_3_2": [{"nested_3_1":[{"h": String(i + 1)}]}]}]
                      });
  }
  db.foo1.save(foo1_values);
  db.foo2.save(foo2_values);
  db.foo3.save(foo3_values);
  if (isEnterprise) {
    db._createView("foov", "arangosearch", {
      "consolidationIntervalMsec": 0,
      "cleanupIntervalStep": 0,
      "links": {
        "foo1": {
          "includeAllFields": true,
          "fields": {
            "nested_1_1": {
              "nested": {
                "a": {}
              }
            }
          }
        },
        "foo2": {
          "includeAllFields": true,
          "fields": {
            "nested_2_1": {
              "nested": {
                "c": {}
              }
            },
            "nested_2_2": {
              "nested": {
                "nested_2_1": {
                  "nested": {
                    "d": {}
                  }
                }
              } 
            }
          }
        },
        "foo3": {
          "includeAllFields": true,
          "fields": {
            "nested_3_1": {
              "nested": {
                "e": {}
              }
            },
            "nested_3_2": {
              "nested": {
                "nested_3_1": {
                  "nested": {
                    "f": {}
                  }
                }
              } 
            },
            "nested_3_3": {
              "nested": {
                "nested_3_2": {
                  "nested": {
                    "nested_3_1": {
                      "nested":{
                        "h": {}
                      }
                    }
                  }
                }
              } 
            }
          }
        }
      }
    });
  } else {
    db._createView("foov", "arangosearch", {
      "consolidationIntervalMsec": 0,
      "cleanupIntervalStep": 0,
      "links": {
        "foo1": {"includeAllFields": true},
        "foo2": {"includeAllFields": true},
        "foo3": {"includeAllFields": true}
      }
    });
  }

  db._query("FOR d IN foov OPTIONS { waitForSync: true } LIMIT 1 RETURN 1");
}

function cleanupClusterWideMetrics() {
  db._dropView("foov");
  db._drop("foo1");
  db._drop("foo2");
  db._drop("foo3");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////
function CoordinatorMetricsTestSuite() {
  return {
    setUpAll: function () {
      createClusterWideMetrics();
    },

    tearDownAll: function () {
      cleanupClusterWideMetrics();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief CoordinatorMetricsTestSuite tests
    ////////////////////////////////////////////////////////////////////////////////
    testMetricsParameterValidation: function () {
      let coordinators = getEndpointsByType("coordinator");
      let dbservers = getEndpointsByType("dbserver");
      let servers = [coordinators, dbservers];
      for (let j = 0; j < servers.length; j++) {
        for (let i = 0; i < servers[j].length; i++) {
          let server = servers[j][i];
          let res = getRawMetric(server, '?mode=invalid');
          assertEqual(res.code, 400);
        }
        for (let i = 0; i < servers[j].length; i++) {
          let server = servers[j][i];
          let res = getRawMetric(server, '?type=invalid');
          assertEqual(res.code, 400);
        }
        for (let i = 0; i < servers[j].length; i++) {
          let server = servers[j][i];
          let res = getRawMetric(server, '?type=invalid&mode=invalid');
          assertEqual(res.code, 400);
        }
      }
      for (let i = 0; i < coordinators.length; i++) {
        let server = coordinators[i];
        let res = getRawMetric(server, '?serverId=invalid');
        assertEqual(res.code, 404);
      }
      for (let i = 0; i < coordinators.length; i++) {
        let server = coordinators[i];
        let res = getRawMetric(server, '?serverId=invalid&mode=invalid');
        assertEqual(res.code, 400);
      }
      for (let i = 0; i < coordinators.length; i++) {
        let server = coordinators[i];
        let res = getRawMetric(server, '?serverId=invalid&type=invalid');
        assertEqual(res.code, 400);
      }
      for (let i = 0; i < coordinators.length; i++) {
        let server = coordinators[i];
        let res = getRawMetric(server, '?serverId=invalid&type=invalid&mode=invalid');
        assertEqual(res.code, 400);
      }
    },

    testMetricsLocalMode: function () {
      let coordinators = getEndpointsByType("coordinator");
      for (let i = 0; i < coordinators.length; i++) {
        let coordinator = coordinators[i];
        let res = getRawMetric(coordinator, '?mode=local');
        assertEqual(res.code, 200);
      }
    },

    testIResearchJsonMetrics: function () {
      let dbservers = getEndpointsByType("dbserver");
      let metrics = {};
      for (let i = 0; i < dbservers.length; i++) {
        let dbserver = dbservers[i];
        let txt = getAllMetric(dbserver, '?type=db_json');
        let json = VPACK_TO_V8(txt);
        for (let j = 0; j < json.length; j += 3) {
          let name = json[j];
          let raw_labels = json[j + 1];
          let collection_pattern = "collection=\"";
          let collection_pos = raw_labels.search(collection_pattern);
          let labels = raw_labels.substr(collection_pos + collection_pattern.length, 4);
          let value = json[j + 2];
          if (!_.has(metrics, name)) {
            metrics[name] = {};
          }
          if (!_.has(metrics[name], labels)) {
            metrics[name][labels] = 0;
          }
          metrics[name][labels] += value;
        }
      }
      checkMetrics(metrics);
      assertEqual(metrics["arangodb_search_cleanup_time"]["foo1"], 0);
      assertEqual(metrics["arangodb_search_cleanup_time"]["foo2"], 0);
      assertEqual(metrics["arangodb_search_cleanup_time"]["foo3"], 0);

      assertTrue(metrics["arangodb_search_commit_time"]["foo1"] > 0);
      assertTrue(metrics["arangodb_search_commit_time"]["foo2"] > 0);
      assertTrue(metrics["arangodb_search_commit_time"]["foo3"] > 0);

      assertEqual(metrics["arangodb_search_consolidation_time"]["foo1"], 0);
      assertEqual(metrics["arangodb_search_consolidation_time"]["foo2"], 0);
      assertEqual(metrics["arangodb_search_consolidation_time"]["foo3"], 0);

      assertEqual(metrics["arangodb_search_num_failed_cleanups"]["foo1"], 0);
      assertEqual(metrics["arangodb_search_num_failed_cleanups"]["foo2"], 0);
      assertEqual(metrics["arangodb_search_num_failed_cleanups"]["foo3"], 0);

      assertEqual(metrics["arangodb_search_num_failed_commits"]["foo1"], 0);
      assertEqual(metrics["arangodb_search_num_failed_commits"]["foo2"], 0);
      assertEqual(metrics["arangodb_search_num_failed_commits"]["foo3"], 0);

      assertEqual(metrics["arangodb_search_num_failed_consolidations"]["foo1"], 0);
      assertEqual(metrics["arangodb_search_num_failed_consolidations"]["foo2"], 0);
      assertEqual(metrics["arangodb_search_num_failed_consolidations"]["foo3"], 0);
    },

    testIResearchMetricStatsForce: function () {
      let coordinators = getEndpointsByType("coordinator");
      let c = coordinators[0];
      cleanupClusterWideMetrics();
      getAllMetric(c, '?mode=write_global'); // write something not valid
      createClusterWideMetrics();
      let txt = getAllMetric(c, '?mode=write_global');
      checkRawMetrics(txt, false);
      checkCoordinators(coordinators, '?mode=read_global');
      require("internal").sleep(1);
      checkCoordinators(coordinators, '?mode=trigger_global');
    },

    testIResearchMetricStats: function () {
      let coordinators = getEndpointsByType("coordinator");
      cleanupClusterWideMetrics();
      let c = coordinators[0];
      getAllMetric(c, '?mode=write_global'); // write something not valid
      createClusterWideMetrics();
      let txt = getAllMetric(c, '?mode=write_global');
      checkRawMetrics(txt, false);
      checkCoordinators(coordinators, '?mode=trigger_global');
      require("internal").sleep(1);
      checkCoordinators(coordinators, '?mode=read_global');
    },

    testIResearchMetricStatsSleep: function () {
      let coordinators = getEndpointsByType("coordinator");
      cleanupClusterWideMetrics();
      let c = coordinators[0];
      getAllMetric(c, '?mode=write_global'); // write something not valid
      createClusterWideMetrics();
      getAllMetric(c, '?mode=trigger_global');
      require("internal").sleep(1);
      checkCoordinators(coordinators, '?mode=trigger_global');
      require("internal").sleep(1);
      checkCoordinators(coordinators, '?mode=read_global');
    },

    testMetricsHandlerNotCrash: function () {
      let type = ['?e0=e0', '?type=invalid', '?type=db_json', '?type=cd_json', '?type=last'];
      let mode = ['&e1=e1', '&mode=invalid', '&mode=local', '&mode=trigger_global', '&mode=read_global', '&mode=write_global'];
      let serverId = ['&e2=e2', '&serverId=invalid'];
      let coordinators = getEndpointsByType("coordinator");
      let dbservers = getEndpointsByType("dbserver");
      let servers = [coordinators, dbservers];
      let f = (a, b) => [].concat(...a.map(a => b.map(b => [].concat(a, b))));
      let cartesian = (a, b, ...c) => b ? cartesian(f(a, b), ...c) : a;
      let params = cartesian(type, mode, serverId);
      for (let k = 0; k < params.length; k++) {
        let param = params[k][0] + params[k][1] + params[k][2];
        // require('internal').print(param);
        for (let j = 0; j < servers.length; j++) {
          for (let i = 0; i < servers[j].length; i++) {
            let server = servers[j][i];
            let res = getRawMetric(server, param);
            // require('internal').print(res.errorMessage);
          }
        }
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CoordinatorMetricsTestSuite);

return jsunity.done();
