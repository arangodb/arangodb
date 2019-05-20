/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertFalse, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test async requests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');

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
        require("internal").sleep(0.5);
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
  };
}


jsunity.run(AsyncRequestSuite);

return jsunity.done();
