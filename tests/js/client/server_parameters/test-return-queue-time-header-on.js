/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertFalse, assertTrue, assertMatch, arango */

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
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'http.return-queue-time-header': 'true',
    'javascript.v8-contexts': '4',
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const internal = require('internal');
const isCluster = internal.isCluster();
const { getEndpointsByType } = require('@arangodb/test-helper');
let IM = global.instanceManager;
let { instanceRole } = require('@arangodb/testutils/instance');

function testSuite() {  
  const header = "x-arango-queue-time-seconds";
  
  let waitForJobsToFinish = () => {
    let tries = 30;
    let pending = [];
    while (tries-- > 0) {
      pending = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pending), pending);
      if (pending.length === 0) {
        return;
      }
      internal.sleep(1);
    }
    assertFalse(true, pending);
  };

  let waitForPending = (pendingAtStart, min, max) => {
    let pending = {};
    let tries = 0;
    while (tries++ < 120) {
      pending = arango.GET_RAW("/_api/job/pending?count=999999");
      let jobs = pending.parsedBody;
      // must have processed at least (n - max) jobs and 
      // must have at least (n - min) jobs left
      if (jobs.length - pendingAtStart.length >= min && 
          jobs.length - pendingAtStart.length <= max) {
        return pending;
      }

      require("internal").sleep(0.25);
    }
    assertFalse(true, { tries, pending, pendingAtStart, min, max });
  };

  let jobs = [];
  let endpoints = [];

  return {
    setUpAll: function() {
      if (isCluster) {
        IM.debugSetFailAt("Scheduler::alwaysSetDequeueTime", instanceRole.coordinator);
      } else {
        IM.debugSetFailAt("Scheduler::alwaysSetDequeueTime", instanceRole.single);
      }
    },

    tearDownAll: function() {
      IM.debugClearFailAt();
    },

    setUp: function() {
      jobs = [];
    },

    tearDown: function() {
      jobs.forEach((job) => {
        // note: some of the async tasks here may already be gone when we try to clean up.
        // that can be ignored.
        arango.PUT_RAW("/_api/job/" + job + "/cancel", '');
        arango.DELETE_RAW("/_api/job/" + job, '');
      });
      // don't leak our jobs into other tests/testsuites
      waitForJobsToFinish();
    },

    testQueueTimeHeaderReturned : function() {
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.headers.hasOwnProperty(header));
      let value = result.headers[header];
      assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
    },
    
    testQueueTimeHeaderNonZero : function() {
      const pendingAtStart = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pendingAtStart), pendingAtStart);
      assertEqual([], pendingAtStart);
      const n = 250;

      // this test has to be a bit vague, because the dequeue time
      // is only updated from time to time on the server side, 
      // using some randomness.
      // so we send lots of requests to the server asynchronously,
      // to have most of them queued for a while.
      let queueTime = 0;
      let tries = 10;
      while (tries-- > 0) {
        for (let i = 0; i < n; ++i) {
          let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(0.25);', { "x-arango-async": "store" });
          assertTrue(result.headers.hasOwnProperty('x-arango-async-id'));
          jobs.push(result.headers['x-arango-async-id']);
          assertTrue(result.headers.hasOwnProperty(header));
          let value = result.headers[header];
          assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
        }

        // now wait until some of the requests have been processed,
        // and (hopefully) the dequeue time was updated at least once.
        let pending = waitForPending(pendingAtStart, 1, n - 100);
      
        // process queue time value in last answer we got
        assertTrue(pending.headers.hasOwnProperty(header));
        let value = pending.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
        queueTime = parseFloat(value);

        if (queueTime > 0) {
          break;
        }
      }
      assertTrue(queueTime > 0, queueTime); 
      assertTrue(tries > 0, tries);
    },
    
    testRejectBecauseOfTooHighQueueTime : function() {
      const pendingAtStart = arango.GET("/_api/job/pending?count=999999");
      assertTrue(Array.isArray(pendingAtStart), pendingAtStart);
      const n = 250;

      for (let i = 0; i < n; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(1.0);', { "x-arango-async": "store" });
        jobs.push(result.headers['x-arango-async-id']);
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }
      
      // now wait until some of the requests have been processed,
      // and (hopefully) the dequeue time was updated at least once.
      let tries = 0;
      let result = {};
      while (++tries <= 100) {
        waitForPending(pendingAtStart, 1, n - 30);

        result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0.00000001" });
        if (result.code === 412) {
          break;
        }

        internal.sleep(0.25);
      }
        
      assertEqual(412, result.code, JSON.stringify(result));
      assertEqual(errors.ERROR_QUEUE_TIME_REQUIREMENT_VIOLATED.code, result.errorNum);
      
      // try with much higher queue time. this should succeed
      result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "5000" });
      assertEqual("arango", result.server);
    },
    
    testNotRejectedBecauseOfQueueTimeZero : function() {
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

      waitForPending(pendingAtStart, 1, n - 30);
      
      let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0" });
      assertEqual("arango", result.server);
    },
    
    testSendInvalidTimeValues : function() {
      ["null", "-1", "-99.99", "", "0000", "1.1.1", "9999999999999999999999999999999999999", "quetzalcoatl"].forEach((broken) => {
        let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": broken });
        assertEqual("arango", result.server);
      });
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
