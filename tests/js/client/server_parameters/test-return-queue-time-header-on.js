/* jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB Inc, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2021, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'http.return-queue-time-header': 'true',
    'javascript.v8-contexts': '4'
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;

function testSuite () {
  const header = "x-arango-queue-time-seconds";

  let jobs = [];

  return {
    setUp: function () {
      jobs = [];
    },

    tearDown: function () {
      jobs.forEach((job) => {
        // note: some of the async tasks here may already be gone when we try to clean up.
        // that can be ignored.
        arango.PUT_RAW("/_api/job/" + job + "/cancel", '');
        arango.DELETE_RAW("/_api/job/" + job, '');
      });
    },

    testQueueTimeHeaderReturned: function () {
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.headers.hasOwnProperty(header));
      let value = result.headers[header];
      assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
    },

    testQueueTimeHeaderNonZero: function () {
      const pendingAtStart = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pendingAtStart), pendingAtStart);
      const n = 250;

      // this test has to be a bit vague, because the dequeue time
      // is only updated from time to time on the server side,
      // using some randomness.
      // so we send lots of requests to the server asynchronously,
      // to have most of them queued for a while.
      for (let i = 0; i < n; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(0.5);', { "x-arango-async": "store" });
        assertTrue(result.headers.hasOwnProperty('x-arango-async-id'));
        jobs.push(result.headers['x-arango-async-id']);
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      // now wait until some of the requests have been processed,
      // and (hopefully) the dequeue time was updated at least once.
      let tries = 0;
      while (tries++ < 60) {
        let pending = arango.GET("/_api/job/pending?count=999999");
        if (pending.length - pendingAtStart.length > n - 100 &&
            pending.length - pendingAtStart.length <= n - 60) {
          break;
        }
        require("internal").sleep(0.5);
      }

      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.headers.hasOwnProperty(header));
      let value = result.headers[header];
      assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      value = parseFloat(value);
      assertTrue(value > 0, value);
    },

    testRejectBecauseOfTooHighQueueTime: function () {
      const pendingAtStart = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pendingAtStart), pendingAtStart);
      const n = 250;

      for (let i = 0; i < n; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(0.5);', { "x-arango-async": "store" });
        jobs.push(result.headers['x-arango-async-id']);
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      let tries = 0;
      while (tries++ < 60) {
        let pending = arango.GET("/_api/job/pending?count=999999");
        if (pending.length - pendingAtStart.length > n - 100 &&
            pending.length - pendingAtStart.length <= n - 60) {
          break;
        }
        require("internal").sleep(0.5);
      }

      let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0.00001" });
      assertEqual(412, result.code);
      assertEqual(errors.ERROR_QUEUE_TIME_REQUIREMENT_VIOLATED.code, result.errorNum);

      // try with much higher queue time. this should succeed
      result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "5000" });
      assertEqual("arango", result.server);
    },

    testNotRejectedBecauseOfQueueTimeZero: function () {
      const pendingAtStart = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pendingAtStart), pendingAtStart);
      const n = 250;

      for (let i = 0; i < n; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(0.5);', { "x-arango-async": "store" });
        jobs.push(result.headers['x-arango-async-id']);
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      let tries = 0;
      while (tries++ < 60) {
        let pending = arango.GET("/_api/job/pending?count=999999");
        if (pending.length - pendingAtStart.length > n - 100 &&
            pending.length - pendingAtStart.length <= n - 60) {
          break;
        }
        require("internal").sleep(0.5);
      }

      let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0" });
      assertEqual("arango", result.server);
    },

    testSendInvalidTimeValues: function () {
      ["null", "-1", "-99.99", "", "0000", "1.1.1", "9999999999999999999999999999999999999", "quetzalcoatl"].forEach((broken) => {
        let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": broken });
        assertEqual("arango", result.server);
      });
    }

  };
}

jsunity.run(testSuite);
return jsunity.done();
