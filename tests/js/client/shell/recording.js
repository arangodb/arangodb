/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, print */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const internal = require('internal');
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" : "application/x-velocypack";

////////////////////////////////////////////////////////////////////////////////
// Test recording APIs
////////////////////////////////////////////////////////////////////////////////

function recordingAPIsSuite() {
  const cursorApi = "/_api/cursor";
  const apiCallsApi = "/_admin/server/api-calls";
  const aqlQueriesApi = "/_admin/server/aql-queries";
  const cn = "recording_test_collection";

  function checkApiCallsRecording() {
    let doc = arango.GET_RAW(apiCallsApi);
    assertEqual(doc.code, 200); 
    // Recording is enabled and we have permissions
    assertEqual(doc.headers['content-type'], contentType);
    assertFalse(doc.parsedBody['error']);
    assertNotUndefined(doc.parsedBody['result']);
    assertNotUndefined(doc.parsedBody.result['calls']);
    assertTrue(Array.isArray(doc.parsedBody.result.calls));
    
    for (let query of doc.parsedBody.result.calls) {
      assertTrue(typeof(query.timeStamp) === "string");
      assertTrue(typeof(query.requestType) === "string");
      assertTrue(typeof(query.path) === "string");
      assertTrue(typeof(query.database) === "string");
    }
    return doc.parsedBody.result.calls;
  }

  function checkAqlQueriesRecording() {
    let doc = arango.GET_RAW(aqlQueriesApi);
    assertEqual(doc.code, 200);
    // Recording is enabled and we have permissions
    assertEqual(doc.headers['content-type'], contentType);
    assertFalse(doc.parsedBody['error']);
    assertNotUndefined(doc.parsedBody['result']);
    assertNotUndefined(doc.parsedBody.result['queries']);
    assertTrue(Array.isArray(doc.parsedBody.result.queries));
    
    for (let query of doc.parsedBody.result.queries) {
      assertTrue(typeof(query.timeStamp) === "string");
      assertTrue(typeof(query.database) === "string");
      assertTrue(typeof(query.query) === "string");
      assertTrue(typeof(query.bindVars) === "object");
    }
    return doc.parsedBody.result.queries;
  }

  return {
    setUpAll: function () {
      // Create a test collection
      db._drop(cn);
      let c = db._create(cn);
      
      // Insert some test data
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({"_key": `test${i}`, "value": i, "name": `item${i}`});
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    test_api_calls_endpoint_exists: function () {
      let doc = arango.GET_RAW(apiCallsApi);
      
      // API should exist. We accept only success (200).
      assertEqual(doc.code, 200,
                 `Expected API calls endpoint to exist, got ${doc.code}`);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_aql_queries_endpoint_exists: function () {
      let doc = arango.GET_RAW(aqlQueriesApi);
      
      // API should exist. We accept only success (200).
      assertEqual(doc.code, 200,
                 `Expected AQL queries endpoint to exist, got ${doc.code}`);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_recording_apis_after_cursor_operations: function () {
      // Execute some cursor operations to generate recordable activity
      let cursorBody = {
        "query": `FOR doc IN ${cn} FILTER doc.value >= 5 RETURN doc`,
        "count": true,
        "batchSize": 5
      };
      
      // Create a cursor
      let cursorResponse = arango.POST_RAW(cursorApi, cursorBody);
      assertEqual(cursorResponse.code, 201);
      assertFalse(cursorResponse.parsedBody['error']);
      
      // If we have a cursor with more data, fetch the next batch
      if (cursorResponse.parsedBody['hasMore'] && cursorResponse.parsedBody['id']) {
        let cursorId = cursorResponse.parsedBody['id'];
        let nextBatchResponse = arango.POST_RAW(`${cursorApi}/${cursorId}`, "");
        assertTrue(nextBatchResponse.code === 200);
      }

      // Now check the recording APIs, just for plausibility:
      let apiCalls = checkApiCallsRecording();
      // There should be some call to the `/_api/cursor` path:
      let found = false;
      for (let c of apiCalls) {
        if (c.path === "/_api/cursor") {
          found = true;
          break;
        }
      }
      assertTrue(found, `Did not find /_api/cursor path in report: ${JSON.stringify(apiCalls)}`);

      let queries = checkAqlQueriesRecording();
      found = false;
      for (let q of queries) {
        if (q.query === cursorBody.query) {
          found = true;
          break;
        }
      }
      assertTrue(found, `Did not find our query in report: ${JSON.stringify(queries)}`);
    },

    test_recording_apis_with_different_queries: function () {
      // Execute different types of queries to see if they get recorded
      const queries = [
        `FOR doc IN ${cn} LIMIT 3 RETURN doc._key`,
        `FOR doc IN ${cn} FILTER doc.name == 'item1' RETURN doc`,
        `FOR doc IN ${cn} SORT doc.value DESC LIMIT 1 RETURN doc.value`
      ];

      for (let query of queries) {
        let body = {"query": query, "count": false};
        let response = arango.POST_RAW(cursorApi, body);
        assertEqual(response.code, 201);
        assertFalse(response.parsedBody['error']);
      }

      // Check if the operations were recorded
      checkApiCallsRecording();
      let queriesSeen = checkAqlQueriesRecording();
      for (let qq of queries) {
        let found = false;
        for (let q of queriesSeen) {
          if (q.query === qq) {
            found = true;
            break;
          }
        }
        assertTrue(found, `Did not find our query '${qq}' in report: ${JSON.stringify(queriesSeen)}`);
      }
    },

    test_recording_apis_method_restrictions: function () {
      // Test that these APIs only accept GET requests
      let postResponse = arango.POST_RAW(apiCallsApi, "{}");
      assertEqual(postResponse.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(postResponse.parsedBody['error']);
      
      let putResponse = arango.PUT_RAW(apiCallsApi, "{}");
      assertEqual(putResponse.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(putResponse.parsedBody['error']);
      
      let deleteResponse = arango.DELETE_RAW(apiCallsApi);
      assertEqual(deleteResponse.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(deleteResponse.parsedBody['error']);

      // Same for AQL queries API
      let postResponse2 = arango.POST_RAW(aqlQueriesApi, "{}");
      assertEqual(postResponse2.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(postResponse2.parsedBody['error']);
      
      let putResponse2 = arango.PUT_RAW(aqlQueriesApi, "{}");
      assertEqual(putResponse2.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(putResponse2.parsedBody['error']);
      
      let deleteResponse2 = arango.DELETE_RAW(aqlQueriesApi);
      assertEqual(deleteResponse2.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertTrue(deleteResponse2.parsedBody['error']);
    },

    test_large_bind_parameters_not_recorded: function () {
      // Test that bind parameters larger than 1024 bytes are not included in recording
      
      // Create a large string parameter that's clearly over 1024 bytes in VelocyPack
      // We'll use 2000 characters to be well over the limit
      let largeString = 'x'.repeat(2000);
      let largeArray = [];
      for (let i = 0; i < 100; i++) {
        largeArray.push(`large_item_${i}_with_some_extra_content_to_make_it_bigger`);
      }
      
      let query = `FOR doc IN ${cn} FILTER doc.name == @name AND doc.value IN @values RETURN doc`;
      let bindVars = {
        "name": largeString,
        "values": largeArray
      };

      let cursorBody = {
        "query": query,
        "bindVars": bindVars,
        "count": false
      };
      
      // Execute the query with large bind parameters
      let cursorResponse = arango.POST_RAW(cursorApi, cursorBody);
      assertEqual(cursorResponse.code, 201);
      assertFalse(cursorResponse.parsedBody['error']);

      // Check the recording - the query should be there but bindVars should be empty
      let queriesSeen = checkAqlQueriesRecording();
      let found = false;
      for (let q of queriesSeen) {
        if (q.query === query) {
          found = true;
          // Verify that bindVars is empty due to size limit
          assertTrue(typeof(q.bindVars) === "object");
          assertEqual(Object.keys(q.bindVars).length, 0, 
                     `Expected empty bindVars for large query, but got: ${JSON.stringify(q.bindVars)}`);
          break;
        }
      }
      assertTrue(found, `Did not find our query with large bind parameters in report: ${JSON.stringify(queriesSeen)}`);
    },

    test_query_recording_memory_limit: function () {
      // Test that query recording has a memory limit and only keeps recent queries
      
      // Create a string of approximately 900 bytes for bind parameters
      let largeString = 'x'.repeat(900);
      
      const totalQueries = 30000;
      const firstBatch = 100;  // Check that first 100 queries are gone
      const lastBatch = 100;   // Check that last 100 queries are present
      
      print(`Executing ${totalQueries} queries with large bind parameters...`);
      
      // Execute 30,000 queries, each with a unique identifier
      for (let i = 0; i < totalQueries; i++) {
        let uniqueQueryString = `FOR doc IN ${cn} FILTER doc.name == @name /* query_${i} */ RETURN doc`;
        let bindVars = {
          "name": largeString
        };

        let cursorBody = {
          "query": uniqueQueryString,
          "bindVars": bindVars,
          "count": false
        };
        
        let cursorResponse = arango.POST_RAW(cursorApi, cursorBody);
        assertEqual(cursorResponse.code, 201);
        assertFalse(cursorResponse.parsedBody['error']);
        
        // Print progress every 5000 queries
        if ((i + 1) % 5000 === 0) {
          print(`Executed ${i + 1} queries...`);
        }
      }
      
      print("Waiting 1 second before checking recording...");
      internal.sleep(1);
      
      // Check the recording API
      let queriesSeen = checkAqlQueriesRecording();
      print(`Found ${queriesSeen.length} recorded queries`);
      
      // Verify that the first 100 queries are NOT found
      let firstBatchFound = 0;
      for (let i = 0; i < firstBatch; i++) {
        let expectedQueryString = `FOR doc IN ${cn} FILTER doc.name == @name /* query_${i} */ RETURN doc`;
        for (let q of queriesSeen) {
          if (q.query === expectedQueryString) {
            firstBatchFound++;
            break;
          }
        }
      }
      
      // Verify that the last 100 queries ARE found
      let lastBatchFound = 0;
      for (let i = totalQueries - lastBatch; i < totalQueries; i++) {
        let expectedQueryString = `FOR doc IN ${cn} FILTER doc.name == @name /* query_${i} */ RETURN doc`;
        for (let q of queriesSeen) {
          if (q.query === expectedQueryString) {
            lastBatchFound++;
            break;
          }
        }
      }
      
      print(`First batch found: ${firstBatchFound}/${firstBatch}, Last batch found: ${lastBatchFound}/${lastBatch}`);
      
      // We expect the first queries to be gone due to memory limit
      assertTrue(firstBatchFound < firstBatch / 2, 
                `Expected most of the first ${firstBatch} queries to be gone due to memory limit, but found ${firstBatchFound}`);
      
      // We expect the last queries to be present
      assertTrue(lastBatchFound > lastBatch / 2, 
                `Expected most of the last ${lastBatch} queries to be present, but only found ${lastBatchFound}`);
      
      print("Query recording memory limit test completed successfully");
    }
  };
}

jsunity.run(recordingAPIsSuite);
return jsunity.done(); 
