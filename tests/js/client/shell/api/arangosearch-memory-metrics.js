/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const { getMetricSingle, getDBServersClusterMetricsByName } = require('@arangodb/test-helper');
const isCluster = require("internal").isCluster();
  
function MemoryMetrics() {
  const collection = "test";
  const view = "testView";
  let x = [];

  let getMetric = (name) => { 
    if (isCluster) {
      let sum = 0;
      let metrics = getDBServersClusterMetricsByName(name)
      assertTrue(metrics.length == 3);
      metrics.forEach( num => {
        sum += num;
      });
      return sum;
    } else {
      return getMetricSingle(name);
    }
  };

  return {
    setUpAll: function () {
      for (let i = 0; i < 10000; i++) {
        x.push({stringValue: "" + i, numericValue: i});
      }
      let c = db._create(collection, {replicationFactor:3, writeConcern:3, numberOfShards : 3});
      c.insert(x);
      db._createView(view, "arangosearch", {
      links: {[collection]: {
          includeAllFields: true,
          primarySort: {"fields": [{ "field": "stringValue", "desc": true }]},
          storedValues: ["numericValue"],
          consolidationIntervalMsec: 100,
          commitIntervalMsec: 100,
        }}
      });
      const writers = getMetric("arangodb_search_writers_memory");
      assertTrue(writers > 0);
      const descriptors = getMetric("arangodb_search_file_descriptors");
      assertTrue(descriptors > 0);
    },
    tearDownAll: function () {
      db._dropView(view);
      db._drop(collection);
    },

    testSimple: function () {
      let c = db._collection(collection);
      for (let i = 0; i < 3; i++) {
        print('ITERATION')
        const oldValue = getMetric("arangodb_search_writers_memory");
        c.insert(x);
        const newValue = getMetric("arangodb_search_writers_memory");
        print(oldValue, newValue);
        assertNotEqual(oldValue, newValue);
      }
      let readers = 0;
      let consolidations = 0;
      let mapped = 0;
      for (let i = 0; i < 100; ++i) {
        if (readers === 0) {
          readers = getMetric("arangodb_search_readers_memory");
        }
        if (consolidations === 0) {
          consolidations = getMetric("arangodb_search_consolidations_memory");
        }
        // TODO(MBkkt) linux only check?
        mapped = getMetric("arangodb_search_mapped_memory");
        if (readers > 0 && consolidations > 0) {
          break;
        }
        require("internal").sleep(0.1);
      }
      assertTrue(readers > 0);
      assertTrue(consolidations > 0);
      {
        const oldValue = getMetric("arangodb_search_writers_memory");
        db._query("FOR d IN " + collection + " REMOVE d IN " + collection);
        const newValue = getMetric("arangodb_search_writers_memory");
        assertNotEqual(oldValue, newValue);
      }
    },
  };
}

jsunity.run(MemoryMetrics);

return jsunity.done();
