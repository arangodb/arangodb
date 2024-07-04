/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertFalse, assertEqual */

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
/// @author Jan Steemann
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require("@arangodb");
const internal = require("internal");

const db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AsyncRequestSuite () {
  'use strict';
  return {
    testAsyncRequest() {
      let res = arango.GET_RAW("/_api/version", { "x-arango-async" : "true" });
      assertEqual(202, res.code);
      assertFalse(res.headers.hasOwnProperty("x-arango-async-id"));
    },
    
    testAsyncRequestStore() {
      let res = arango.GET_RAW("/_api/version", { "x-arango-async" : "store" });
      assertEqual(202, res.code);
      assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
      const id = res.headers["x-arango-async-id"];
     
      let tries = 0;
      while (++tries < 30) {
        res = arango.PUT_RAW("/_api/job/" + id, "");
        if (res.code === 200) {
          break;
        }
        internal.sleep(0.5);
      }

      assertEqual(200, res.code);
    },

    testAsyncRequestQueueFull() {
      let res = arango.PUT_RAW("/_admin/debug/failat/queueFull", "");
      if (res.code !== 200) {
        // abort test - failure mode is not activated on server
        return;
      }
      try {
        res = arango.GET_RAW("/_api/version", { "x-arango-async" : "true" });
        assertEqual(503, res.code);
      } finally {
        arango.DELETE("/_admin/debug/failat/queueFull");
      }
    },

    testAsyncHeadRequest() {
      const cn = "testAsyncHeadRequestCollection";
      const testKey = "testAsyncHeadRequestDocument";
      db._drop(cn);

      try {
        let col = db._create(cn);
        col.insert({_key: testKey});

        // Make sure we can retrieve the document length via a HEAD request
        const keyUrl = `/_api/document/${cn}/${testKey}`;
        let res = arango.HEAD_RAW(keyUrl);
        assertEqual(200, res.code, `Document with key ${testKey}: ${JSON.stringify(res)}`);
        assertTrue(res.headers["content-length"] !== "0", `Document with key ${testKey}: ${JSON.stringify(res)}`);

        // Sending an async HEAD request should work, but the content-length should be 0.
        // Otherwise, the PUT request will hang.
        res = arango.HEAD_RAW(keyUrl, { "x-arango-async" : "store" });
        assertEqual(202, res.code, `Document with key ${testKey}: ${JSON.stringify(res)}`);
        assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
        const jobId = res.headers["x-arango-async-id"];

        // Loop until the async job is ready
        let tries = 0;
        while (++tries < 30) {
          res = arango.PUT_RAW("/_api/job/" + jobId, "");
          if (res.code === 200) {
            break;
          }
          internal.sleep(0.5);
        }

        assertEqual(200, res.code, `Job ID ${jobId}: ${JSON.stringify(res)}`);
        assertTrue(res.headers["content-length"] === "0", `Job ID ${jobId}: ${JSON.stringify(res)}`);
      } finally {
        db._drop(cn);
      }
    },
  };
}


jsunity.run(AsyncRequestSuite);

return jsunity.done();
