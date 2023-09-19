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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////
(function () {

  'use strict';
  const jsunity = require("jsunity");
  const arangodb = require("@arangodb");
  const db = arangodb.db;
  const { getCompleteMetricsValues } = require('@arangodb/test-helper');
  const isWindows = (require("internal").platform.substr(0, 3) === 'win');
  const collectionName = "test";
  const viewName = "testView";

  let generateAndInsert = (collName) => {
    if( typeof generateAndInsert.counter === 'undefined' ) {
      generateAndInsert.counter = 0;
    }
    if( typeof generateAndInsert.factor === 'undefined' ) {
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
      generateAndInsert.counter++;
    }
    db._collection(collName).save(docs);
  };

  return function (dbName, databaseCreationOptions, collectionCreationOptions, shardingEnabled = false) {
    return {
      setUpAll: function () {
        if (databaseCreationOptions) {
          db._createDatabase(dbName, databaseCreationOptions);
        }
        else {
          db._createDatabase(dbName);
        }
        db._useDatabase(dbName);

        db._create(collectionName, collectionCreationOptions);
        generateAndInsert(collectionName);
        db._createView(viewName, "arangosearch", {
        links: {[collectionName]: {
            includeAllFields: true,
            primarySort: {"fields": [{ "field": "stringValue", "desc": true }]},
            storedValues: ["numericValue"],
            consolidationIntervalMsec: 100,
            commitIntervalMsec: 100,
          }}
        });
        const writers = getCompleteMetricsValues("arangodb_search_writers_memory_usage");
        assertTrue(writers > 0);
        const descriptors = getCompleteMetricsValues("arangodb_search_file_descriptors");
        assertTrue(descriptors > 0, descriptors);
      },
      
      tearDownAll: function () {
        db._useDatabase('_system');
        db._dropDatabase(dbName);
      },

      testArangoSearchMetrics: function () {

        let writers_prev = getCompleteMetricsValues("arangodb_search_writers_memory_usage");
        let writers_curr = writers_prev;
        for (let i = 0; i < 5; i++) {
          generateAndInsert(collectionName);
          db._query(`for d in ${viewName} OPTIONS {waitForSync: true} LIMIT 1 RETURN 1 `);
          writers_curr = getCompleteMetricsValues("arangodb_search_writers_memory_usage");
          assertTrue(writers_prev < writers_curr);
          writers_prev = writers_curr;
        }

        let readers, consolidations, descriptors, mapped, scheduler_memory, num_worker_threads;
        for (let i = 0; i < 45; ++i) {

          generateAndInsert(collectionName);
          db._query(`for d in ${viewName} SEARCH d.numericValue == 100 RETURN d`);

          [readers, consolidations, descriptors, mapped, scheduler_memory, num_worker_threads] = getCompleteMetricsValues([
            "arangodb_search_readers_memory_usage", 
            "arangodb_search_consolidations_memory", 
            "arangodb_search_file_descriptors",
            "arangodb_search_mapped_memory",
            "arangodb_scheduler_stack_memory_usage",
            "arangodb_scheduler_num_worker_threads"
          ]);

          assertEqual(scheduler_memory, num_worker_threads * 4 * 1000000, {scheduler_memory, num_worker_threads});

          if (readers > 0 && consolidations > 0 && descriptors > 0 && (isWindows || mapped > 0)) {
            break;
          }
          require("internal").sleep(0.25);
        }

        assertTrue(readers > 0);
        assertTrue(consolidations > 0);
        assertTrue(descriptors > 0);
        if (!isWindows) {
          assertTrue(mapped > 0);
        }

        {
          db._view(viewName).properties({commitIntervalMsec: 0});
          const oldValue = getCompleteMetricsValues("arangodb_search_writers_memory_usage");
          db._query("FOR d IN " + collectionName + " REMOVE d IN " + collectionName);
          const newValue = getCompleteMetricsValues("arangodb_search_writers_memory_usage");
          assertNotEqual(oldValue, newValue);
          db._view(viewName).properties({commitIntervalMsec: 100});
        }

        db._query(`for d in ${viewName} OPTIONS {waitForSync: true} LIMIT 1 RETURN 1 `);
        for (let i = 0; i < 45; ++i) {
          [readers, consolidations, descriptors, mapped, scheduler_memory, num_worker_threads] = getCompleteMetricsValues([
            "arangodb_search_readers_memory_usage", 
            "arangodb_search_consolidations_memory",
            "arangodb_search_file_descriptors",
            "arangodb_search_mapped_memory",
            "arangodb_scheduler_stack_memory_usage",
            "arangodb_scheduler_num_worker_threads"
          ]);

          if (readers === 0 && consolidations === 0 && descriptors === 0 && (isWindows || mapped === 0)) {
            break;
          }
          require("internal").sleep(0.25);
        }

        assertEqual(scheduler_memory, num_worker_threads * 4 * 1000000, {scheduler_memory, num_worker_threads});

        assertEqual(readers, 0);
        assertEqual(consolidations, 0);
        assertEqual(descriptors, 0);
        assertEqual(mapped, 0);
      }
    };
  };
}());
