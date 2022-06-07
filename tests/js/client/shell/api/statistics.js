/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");

let  api = "/_admin/";

////////////////////////////////////////////////////////////////////////////////;
// check statistics-description availability;
//////////////////////////////////////////////////////////////////////////////#;
function calculating_statisticsSuite () {
  return {

    test_testing_statistics_description_correct_cmd: function() {
      let cmd = "/_admin/statistics-description";
      let doc = arango.GET_RAW(cmd) ;

      assertEqual(doc.code, 200);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // check statistics-description for wrong user interaction;
    //////////////////////////////////////////////////////////////////////////////#;

    test_testing_statistics_description_wrong_cmd: function() {
      let cmd = "/_admin/statistics-description/asd123";
      let doc = arango.GET_RAW(cmd) ;

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // check statistics availability;
    //////////////////////////////////////////////////////////////////////////////#;

    test_testing_statistics_correct_cmd: function() {
      let cmd = "/_admin/statistics";
      let doc = arango.GET_RAW(cmd) ;
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody['server']['uptime'] > 0);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // check statistics for wrong user interaction;
    //////////////////////////////////////////////////////////////////////////////#;

    test_testing_statistics_wrong_cmd: function() {
      let cmd = "/_admin/statistics/asd123";
      let doc = arango.GET_RAW(cmd) ;

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // check request statistics counting of async requests;
    //////////////////////////////////////////////////////////////////////////////#;

    test_testing_async_requests_: function() {
      // get stats;
      let cmd = "/_admin/statistics";
      let doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let async_requests_1 = doc.parsedBody['http']['requestsAsync'];

      // get version async - should increase async counter;
      cmd = "/_api/version";
      doc = arango.PUT_RAW(cmd, "", { "X-Arango-Async": "true" });
      assertEqual(doc.code, 202);
      // assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      // assertMatch(doc.headers["x-arango-async-id"], /^\d+$/);
      assertEqual(doc.parsedBody);

      sleep(1);

      // get stats;
      cmd = "/_admin/statistics";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let async_requests_2 = doc.parsedBody['http']['requestsAsync'];
      assertEqual(async_requests_1, async_requests_1);

      // get version async - should not increase async counter;
      cmd = "/_api/version";
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      sleep(1);

      // get stats;
      cmd = "/_admin/statistics";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      let async_requests_3 = doc.parsedBody['http']['requestsAsync'];
      assertEqual(async_requests_2, async_requests_3);
    },

  };
}

jsunity.run(calculating_statisticsSuite);
return jsunity.done();
