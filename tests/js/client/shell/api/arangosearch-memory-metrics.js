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
const isWindows = (require("internal").platform.substr(0, 3) === 'win');
  
function MemoryMetrics() {
  const collectionName = "test";
  const viewName = "testView";

  let getMetric = (name) => {
    if (isCluster) {
      let metrics = getDBServersClusterMetricsByName(name)
      assertTrue(metrics.length == 3);
      // print(typeof name)
      if (typeof name == "string") {
        let sum = 0;
        metrics.forEach( num => {
          sum += num;
        });
        return sum;
      } else {
        return metrics.reduce((acc, metrics) => acc.map((sum, i) => sum + metrics[i]), new Array(metrics[0].length).fill(0));
      }
    } else {
      return getMetricSingle(name);
    }
  };

  let generateAndInsert = (collName) => {
    if( typeof generateAndInsert.counter == 'undefined' ) {
      generateAndInsert.counter = 0;
    }
    if( typeof generateAndInsert.factor == 'undefined' ) {
      generateAndInsert.factor = 0;
    }
    generateAndInsert.factor++;

    let docs = [];
    for (let i = 0; i < 1000 * generateAndInsert.factor; i++) {
      let custom_field = "field_" + generateAndInsert.counter;
      let d = {
        'stringValue': "" + generateAndInsert.counter, 
        'numericValue': generateAndInsert.counter
      };
      d[custom_field] = generateAndInsert.counter;
      docs.push(d);
      generateAndInsert.counter++
    }
    db._collection(collName).save(docs);
  };

  return {
    setUpAll: function () {
      db._create(collectionName, {replicationFactor:3, writeConcern:3, numberOfShards : 3});
      generateAndInsert(collectionName)
      db._createView(viewName, "arangosearch", {
      links: {[collectionName]: {
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
      db._dropView(viewName);
      db._drop(collectionName);
    },

    testSimple: function () {

      for (let i = 0; i < 5; i++) {
        let writersOldest = getMetric("arangodb_search_writers_memory");
        generateAndInsert(collectionName)
        db._query(`for d in ${viewName} OPTIONS {waitForSync: true} LIMIT 1 RETURN 1 `);
        let writersOlder = getMetric("arangodb_search_writers_memory");
        assertNotEqual(writersOldest, writersOlder);
      }

      let readers, consolidations, descriptors, mapped;
      for (let i = 0; i < 100; ++i) {

        generateAndInsert(collectionName)
        db._query(`for d in ${viewName} SEARCH d.numericValue == 100 RETURN d`);

        [readers, consolidations, descriptors, mapped] = getMetric([
          "arangodb_search_readers_memory", 
          "arangodb_search_consolidations_memory", 
          "arangodb_search_file_descriptors",
          "arangodb_search_mapped_memory"
        ]);

        if (readers > 0 && consolidations > 0 && descriptors > 0 && (isWindows || mapped > 0)) {
          break;
        }
      }

      assertTrue(readers > 0);
      assertTrue(consolidations > 0);
      if (!isWindows) {
        assertTrue(mapped > 0);
      }

      {
        const oldValue = getMetric("arangodb_search_writers_memory");
        db._query("FOR d IN " + collectionName + " REMOVE d IN " + collectionName);
        const newValue = getMetric("arangodb_search_writers_memory");
        assertNotEqual(oldValue, newValue);
      }

      db._query(`for d in ${viewName} OPTIONS {waitForSync: true} LIMIT 1 RETURN 1 `);
      [readers, consolidations, descriptors, mapped] = getMetric([
        "arangodb_search_readers_memory", 
        "arangodb_search_consolidations_memory",
        "arangodb_search_file_descriptors",
        "arangodb_search_mapped_memory"
      ]);

      assertTrue(readers == 0);
      assertTrue(consolidations == 0);
      assertTrue(descriptors == 0);
      assertTrue(mapped == 0);
    },
  };
}

jsunity.run(MemoryMetrics);

return jsunity.done();
