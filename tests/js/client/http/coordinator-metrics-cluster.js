/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const {getEndpointsByType, getMetricRaw} = require("@arangodb/test-helper");
const parsePrometheusTextFormat = require("parse-prometheus-text-format");
const _ = require("lodash");

const db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function CoordinatorMetricsTestSuite() {
  return {
    setUpAll: function () {
      db._create("foo1", {"replicationFactor": 3, "numberOfShards": 9});
      db._create("foo2", {"replicationFactor": 2, "numberOfShards": 12});
      db._create("foo3", {"replicationFactor": 1, "numberOfShards": 1});
      let foo1_values = [];
      let foo2_values = [{"name": "i"}];
      let foo3_values = [{"name": "hate"}, {"name": "js"}];
      for (let i = 0; i !== 100; ++i) {
        foo1_values.push({"name": i});
        foo2_values.push({"name": i + 100000});
        foo3_values.push({"name": i - 100000});
      }
      db.foo1.save(foo1_values);
      db.foo2.save(foo2_values);
      db.foo3.save(foo3_values);
      db._createView("foov", "arangosearch", {
        "consolidationIntervalMsec": 0,
        "links": {
          "foo1": {"includeAllFields": true},
          "foo2": {"includeAllFields": true},
          "foo3": {"includeAllFields": true}
        }
      });
      db._query("FOR d IN foov OPTIONS { waitForSync: true } LIMIT 1 RETURN 1");
    },

    tearDownAll: function () {
      db._dropView("foov");
      db._drop("foo1");
      db._drop("foo2");
      db._drop("foo3");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief CoordinatorMetricsTestSuite tests
////////////////////////////////////////////////////////////////////////////////

    testIResearchLinkStats: function () {
      let coordinators = getEndpointsByType("coordinator");
      let c = coordinators[0];
      {// make metrics actual
        getMetricRaw(c);
        require('internal').sleep(2);
      }
      let txt = getMetricRaw(c);
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

      assertEqual(metrics["arangodb_search_num_docs"]["foo1"], 100);
      assertEqual(metrics["arangodb_search_num_docs"]["foo2"], 101);
      assertEqual(metrics["arangodb_search_num_docs"]["foo3"], 102);

      assertEqual(metrics["arangodb_search_num_live_docs"]["foo1"], 100);
      assertEqual(metrics["arangodb_search_num_live_docs"]["foo2"], 101);
      assertEqual(metrics["arangodb_search_num_live_docs"]["foo3"], 102);

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
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CoordinatorMetricsTestSuite);

return jsunity.done();
