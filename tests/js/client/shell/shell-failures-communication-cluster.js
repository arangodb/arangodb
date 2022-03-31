/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure scenarios
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
///////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");

function shellCommunicationsFailureSuite () {
  'use strict';
  const cn = "UnitTestsAhuacatlCommFailures";

  return {
    setUpAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    setUp: function () {
      internal.debugClearFailAt();
    },

    tearDownAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
    },

    tearDown: function () {
      internal.debugClearFailAt();
    },

    testRetryOnLeaderRefusal: function() {
      // This tests if the coordinator retries an insert request if
      // a leader refuses to do the insert by sending a 421:
      internal.debugSetFailAt("documents::insertLeaderRefusal");
      let res = db._connection.POST_RAW("/_api/document/" + cn, {ThisIsTheRetryOnLeaderRefusalTest:12},
                                        {"x-arango-async": "store"});
      assertTrue(res.headers.hasOwnProperty("x-arango-async-id"));
      let id = res.headers["x-arango-async-id"];
      let inq = db._connection.PUT_RAW("/_api/job/" + id, {});
      // The leader refuses to write with 421, we are in retry loop on coordinator.
      assertEqual(204, inq.code);
      assertFalse(inq.error);
      internal.wait(2.0);
      inq = db._connection.PUT_RAW("/_api/job/" + id, {});
      // The leader still refuses to write with 421, we are in retry loop on coordinator.
      assertEqual(204, inq.code);
      assertFalse(inq.error);
      // Allow the leader to write
      internal.debugClearFailAt("documents::insertLeaderRefusal");
      let startTime = new Date();
      while (new Date() - startTime < 10000) {
        // Eventually the write will succeed
        inq = db._connection.PUT_RAW("/_api/job/" + id, {});
        if (inq.code !== 204) {
          break;
        }
        internal.wait(0.5);
      }
      assertEqual(202, inq.code);
      assertFalse(inq.error);
    },
  };
}
 
if (internal.debugCanUseFailAt()) {
  jsunity.run(shellCommunicationsFailureSuite);
}

return jsunity.done();
