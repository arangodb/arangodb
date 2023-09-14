/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, assertMatch, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    // 16 MB is the minimum value that can be set on the server
    'dump.max-memory-usage': '16777216',
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const db = internal.db;
const { getMetricSingle } = require('@arangodb/test-helper');

function testSuite() {
  const cn = "UnitTestsCollection";
  const collections = [cn + "1", cn + "2", cn + "3"];

  return {
    setUpAll: function() {
      const n = 100 * 1000;

      // create some test data in 3 collections
      let c1 = db._create(cn + "1");
      let c2 = db._create(cn + "2");
      let c3 = db._create(cn + "3");
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({value1: "testmann" + i, value2: i, value3: "testmann" + i});
        if (docs.length === 2000) {
          c1.insert(docs);
          c2.insert(docs);
          if (i < n / 2) {
            c3.insert(docs);
          }
          docs = [];
        }
      }
    },
    
    testSingleDumpWithoutBlocking: function() {
      const originalBlocked = getMetricSingle("arangodb_dump_threads_blocked_total");

      let config = { 
        docsPerBatch: 10000, 
        prefetchCount: 8, 
        parallelism: 8, 
        shards: collections,
      };
      let res = arango.POST_RAW("/_api/dump/start", config);
      assertEqual(201, res.code);
          
      let id = res.headers["x-arango-dump-id"];
      try {
        assertMatch(/^dump-\d+$/, id);
          
        let tries = 0;
        let previousMemoryUsage = 0;
        let counter = 0;
        while (++tries < 120) {
          // we expect exactly 1 dump to be ongoing
          let ongoing = getMetricSingle("arangodb_dump_ongoing");
          assertEqual(1, ongoing);
          
          // we expect no further blocking by our small dump
          let blocked = getMetricSingle("arangodb_dump_threads_blocked_total");
          assertEqual(originalBlocked, blocked);

          // memory usage should be below 16 MB, but with a bit of leeway. 
          let memoryUsage = getMetricSingle("arangodb_dump_memory_usage");
          assertTrue(memoryUsage <= 16777216 + 1048576, {memoryUsage});
          if (memoryUsage !== previousMemoryUsage) {
            previousMemoryUsage = memoryUsage;
            counter = 0;
          } else {
            ++counter;
            if (counter === 10) {
              // if memory usage stays the same of 10 iterations (= 5 seconds),
              // we abort the loop
              break;
            }
          }
          internal.sleep(0.5);
        }
      } finally {
        arango.DELETE_RAW("/_api/dump/" + id);
     
        // after finishing the dump, the number of ongoing dumps should
        // be back to 0.
        let ongoing = getMetricSingle("arangodb_dump_ongoing");
        assertEqual(0, ongoing);
      }
    },

    testSingleDumpWithBlocking: function() {
      const originalBlocked = getMetricSingle("arangodb_dump_threads_blocked_total");

      // we are telling the server to fetch up to 1024 batches.
      // this will surely exceed the allowed max memory value of 16 MB
      let config = { 
        docsPerBatch: 10000, 
        prefetchCount: 1024, 
        parallelism: 8, 
        shards: collections,
      };
      let res = arango.POST_RAW("/_api/dump/start", config);
      assertEqual(201, res.code);
          
      let id = res.headers["x-arango-dump-id"];
      try {
        assertMatch(/^dump-\d+$/, id);
          
        let tries = 0;
        let blocked = 0;
        while (++tries < 120) {
          // we expect exactly 1 dump to be ongoing
          let ongoing = getMetricSingle("arangodb_dump_ongoing");
          assertEqual(1, ongoing);

          // memory usage should be below 16 MB, but with a bit of leeway. 
          let memoryUsage = getMetricSingle("arangodb_dump_memory_usage");
          assertTrue(memoryUsage <= 16777216 + 1048576, {memoryUsage});
         
          // as we are only waiting here and not fetching any data from the
          // server, we expect the number of blocked threads to increase
          // at some point
          blocked = getMetricSingle("arangodb_dump_threads_blocked_total");
          if (blocked > originalBlocked) {
            break;
          }
          internal.sleep(0.5);
        }
        assertTrue(blocked > originalBlocked, {blocked});
      } finally {
        arango.DELETE_RAW("/_api/dump/" + id);
     
        // after finishing the dump, the number of ongoing dumps should
        // be back to 0.
        let ongoing = getMetricSingle("arangodb_dump_ongoing");
        assertEqual(0, ongoing);
      }
    },
    
    testCheckProgressWithMultipleDumpsThatHitMemoryLimit: function() {
      const originalBlocked = getMetricSingle("arangodb_dump_threads_blocked_total");

      // we are telling the server to fetch up to 512 batches.
      // this will surely exceed the allowed max memory value of 16 MB
      let config = { 
        docsPerBatch: 10000, 
        prefetchCount: 512, 
        parallelism: 8, 
      };

      const n = 3;

      let dumps = [];
      for (let i = 0; i < n; ++i) {
        let shard = cn + (1 + (i % 3));
        config.shards = [ shard ];
        let res = arango.POST_RAW("/_api/dump/start", config);
        assertEqual(201, res.code);
         
        let id = res.headers["x-arango-dump-id"];
        assertMatch(/^dump-\d+$/, id);

        dumps.push({ id, done: false, batchId: 1, lastBatch: null, shard });
      }
       
      try {
        while (true) {
          dumps.filter((d) => !d.done).forEach((d) => {
            let url = "/_api/dump/next/" + d.id + "?batchId=" + d.batchId;
            if (d.lastBatch !== null) {
              url += "&lastBatch=" + d.lastBatch;
            }
            let res = arango.POST_RAW(url, {});
            if (res.code === 204) {
              d.done = true;
              return;
            }
            if (res.code === 500 && 
                res.parsedBody.errorNum === internal.errors.ERROR_RESOURCE_LIMIT.code) {
              // resource limit temporarily exceeded. simply try again in this case.
              return;
            }
            let shard = res.headers["x-arango-dump-shard-id"];
            assertEqual(200, res.code, res);
            assertTrue(d.shard, shard, { shard, res });
            d.lastBatch = d.batchId;
            ++d.batchId;
          });
          // all done
          if (dumps.filter((d) => d.done).length === n) {
            break;
          }
          
          // we expect exactly n dumps to be ongoing
          let ongoing = getMetricSingle("arangodb_dump_ongoing");
          assertEqual(n, ongoing);
          
          // memory usage should be below 16 MB, but with a bit of leeway. 
          let memoryUsage = getMetricSingle("arangodb_dump_memory_usage");
          assertTrue(memoryUsage <= 16777216 + 1048576, {memoryUsage});
        }
      } finally {
        dumps.forEach((d) => {
          arango.DELETE_RAW("/_api/dump/" + d.id);
        });
          
        let blocked = getMetricSingle("arangodb_dump_threads_blocked_total");
        assertTrue(blocked > originalBlocked, { blocked, originalBlocked });
     
        // after finishing the dump, the number of ongoing dumps should
        // be back to 0.
        let ongoing = getMetricSingle("arangodb_dump_ongoing");
        assertEqual(0, ongoing);
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
