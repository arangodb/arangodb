/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertMatch */

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


function wait_for_get(cmd, code, maxWait) {
  while (true) {
    let doc = arango.GET_RAW(cmd);
    if (doc.code === code) {
      return doc;
    }
    maxWait -= 1;
    if (maxWait === 0) {
      return false;
    }
    sleep(1);
  }
}

function wait_for_put(cmd, code, maxWait) {
  while (true) {
    let doc = arango.PUT_RAW(cmd, "");
    if (doc.code === code) {
      return doc;
    }
    maxWait -= 1;
    if (maxWait === 0) {
      return false;
    }
    sleep(1);
  }
}

function dealing_with_async_requestsSuite () {
  return {
    tearDownAll: function() {
      arango.DELETE('/_api/query/slow');
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // checking methods;
    ////////////////////////////////////////////////////////////////////////////////;

    test_checks_whether_asyncEQfalse_returns_status_202: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "false" });

      assertEqual(doc.code, 200);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertTrue(doc.parsedBody !== "");
    },

    test_checks_whether_asyncEQtrue_returns_status_202: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "true" });

      assertEqual(doc.code, 202);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertTrue(doc.parsedBody !== "");
    },

    test_checks_whether_asyncEQ1_returns_status_200: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "1" });

      assertEqual(doc.code, 200);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertTrue(doc.parsedBody !== "");
    },

    test_checks_whether_HEAD_returns_status_202: function() {
      let cmd = "/_api/version";
      let doc = arango.HEAD_RAW(cmd, { "X-Arango-Async": "true" });

      assertEqual(doc.code, 202);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_whether_POST_returns_status_202: function() {
      let cmd = "/_api/version";
      let doc = arango.POST_RAW(cmd, "", { "X-Arango-Async": "true" });

      assertEqual(doc.code, 202);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_whether_an_invalid_location_returns_status_202: function() {
      let cmd = "/_api/not-existing";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "true" });

      assertEqual(doc.code, 202);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_whether_a_failing_action_returns_status_202: function() {
      let cmd = "/_admin/execute";
      let body = "fail();";
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "true" });

      assertEqual(doc.code, 202);
      assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_the_responses_when_the_queue_fills_up: function() {
      let cmd = "/_api/version";

      for (let i = 1; i < 500; i++) {
        let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "true" });
        assertEqual(doc.code, 202);
        assertFalse(doc.headers.hasOwnProperty("x-arango-async-id"));
        assertEqual(doc.parsedBody, undefined);
      }
    },

    test_checks_whether_setting_x_arango_async_to_store_returns_a_job_id: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      assertMatch(/^\d+$/, doc.headers["x-arango-async-id"]);
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_whether_job_api_returns_200_for_ready_job: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = wait_for_get(cmd, 200, 20);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody, undefined);

      //  should be able to query the status again;
      doc = wait_for_get(cmd, 200, 20);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody, undefined);

      //  should also be able to fetch the result of the job;
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["server"], "arango");

      //  now it should be gone;
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 404);
    },

    test_checks_whether_job_api_returns_204_for_non_ready_job: function() {
      let cmd = "/_api/cursor";;
      let body = '{"query": "FOR i IN 1..10 LET x = sleep(10.0) FILTER i == 5 RETURN 42"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = wait_for_get(cmd, 204, 20);
      assertEqual(doc.code, 204);
      assertEqual(doc.parsedBody, undefined);
    },

    test_checks_whether_job_api_returns_404_for_non_existing_job: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/123" + id + "123"; // assume this id is invalid
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 404);
    },

    test_checks_whether_job_api_returns_done_job_in_pending_and_done_list: function() {
      let cmd = "/_api/cursor";;
      let body = '{"query": "FOR i IN 1..10 LET x = sleep(10.0) FILTER i == 5 RETURN 42"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/done";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody.hasOwnProperty('id'));

      cmd = "/_api/job/pending";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody.hasOwnProperty('id'));
    },

    test_checks_whether_job_api_returns_non_ready_job_in_pending_and_done_lists: function() {
      let cmd = "/_api/cursor";;
      let body = '{"query": "FOR i IN 1..10 LET x = sleep(10.0) FILTER i == 5 RETURN 42"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/pending";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody.hasOwnProperty('id'));

      cmd = "/_api/job/done";
      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody.hasOwnProperty('id'));
    },

    test_checks_whether_we_can_cancel_an_AQL_query_1: function() {
      let cmd = "/_api/cursor";
      let body = '{"query": "for x in 1..1000 let a = sleep(0.5) filter x == 1 return x"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 204);

      cmd = "/_api/job/" + id + "/cancel";
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 410, 20);
      assertEqual(doc.code, 410);
    },

    test_checks_whether_we_can_cancel_an_AQL_query_2: function() {
      let cmd = "/_api/cursor";
      let body = '{"query": "for x in 1..10000 for y in 1..10000 let a = sleep(0.01) filter x == 1 && y == 1 return x"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 204, 20);
      assertEqual(doc.code, 204);

      cmd = "/_api/job/" + id + "/cancel";
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 410, 20);
      assertEqual(doc.code, 410);
    },

    test_checks_whether_we_can_cancel_an_AQL_query_3: function() {
      let cmd = "/_api/cursor";
      let body = '{"query": "for x in 1..10000 for y in 1..10000 let a = sleep(0.01) filter x == 1 && y == 1 return x"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 204, 20);
      assertEqual(doc.code, 204);

      cmd = "/_api/job/" + id + "/cancel";
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 410, 20);
      assertEqual(doc.code, 410);
    },

    test_checks_whether_we_can_cancel_a_transaction: function() {
      let cmd = "/_api/transaction";
      let body = '{"collections": {"write": "_graphs"}, "action": "function() {var i = 0; while (i < 90000000000) {i++; require(\\"internal\\").wait(0.1); } return i;}"}';
      let doc = arango.POST_RAW(cmd, body, { "X-Arango-Async": "store" });

      assertEqual(doc.code, 202);
      assertTrue(doc.headers.hasOwnProperty(("x-arango-async-id")));
      let id = doc.headers["x-arango-async-id"];
      assertMatch(/^\d+$/, id);
      assertEqual(doc.parsedBody, undefined);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 204, 20);
      assertEqual(doc.code, 204);

      cmd = "/_api/job/" + id + "/cancel";
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);

      cmd = "/_api/job/" + id;
      doc = wait_for_put(cmd, 410, 20);
      assertEqual(doc.code, 410);
    },
  };
}

jsunity.run(dealing_with_async_requestsSuite);
return jsunity.done();
