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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");


function queryAnalysisSuite () {
  let api = "/_api/query";
  let queryEndpoint ="/_api/cursor";
  let queryPrefix = "api-cursor";
  let properties = `${api}/properties`;
  let current = `${api}/current`;
  let slow = `${api}/slow`;

  let queryBody = { query:  "FOR x IN 1..5 LET y = SLEEP(2) RETURN x"};
  let fastQueryBody = { query: "FOR x IN 1..1 LET y = SLEEP(0.2) RETURN x"};
  let longQueryBody = { query: "FOR x IN 1..1 LET y = SLEEP(0.2) LET a = y LET b = a LET c = b LET d = c LET e = d LET f = e  RETURN x"};
  let queryWithBindBody = {query: "FOR x IN 1..5 LET y = SLEEP(@value) RETURN x", bindVars: {value: 4}};

  function send_queries (count = 1, async = "true"){
    for (let i = 0; i < count; i++) {
      arango.POST(queryEndpoint, queryBody, {"X-Arango-Async": async });
    }
  }

  function send_fast_queries (count = 1, async = "true") {
    for (let i = 0; i < count; i++) {
      arango.POST(queryEndpoint, fastQueryBody, { "X-Arango-Async": async });
    }
  }

  function send_queries_with_bind (count = 1, async = "true"){
    for (let i = 0; i < count; i++) {
      arango.POST(queryEndpoint, queryWithBindBody, {"X-Arango-Async": async });
    }
  }

  function contains_query(body, testq = queryBody.query) {
    let ret = false;
    body.forEach(q => {
      if (q["query"] === testq) {
        ret = q;
      }
    });
    return ret;
  }

  function wait_for_query(query, type, maxWait) {
    while (true) {
      let doc;
      if (type === "slow") {
        doc = arango.GET_RAW(slow);
      } else if (type === "current") {
        doc = arango.GET_RAW(current);
      }
      assertEqual(doc.code, 200);
      let found = contains_query(doc.parsedBody, query);
      if (found) {
        return found;
      }
      maxWait -= 1;
      if (maxWait === 0) {
        return false;
      }
      sleep(1);
    }
  }

  return {
    setUp: function() {
      arango.PUT(properties, { enable: true, slowQueryThreshold: 20, trackSlowQueries: true});
      arango.DELETE(slow);
    },


    tearDown: function() {
      // Let the queries finish;
      let count = 0;
      while (true) {
        let res = arango.GET(current);
        if (res.length === 0) {
          break;
        }
        res.forEach( q => {
          if (q["query"].search('SLEEP') >= 0) {
            arango.DELETE(api + '/' + q["id"]);
          }
        });
        count += 1;
        if (count === 10) {
          break;
        }
        sleep(1);
      }
    },
    test_should_activate_tracking: function() {
      let doc = arango.PUT_RAW(properties, {enable: true});
      assertEqual(doc.code, 200);
    },


    test_should_track_running_queries: function() {
      send_queries();
      let found = wait_for_query(queryBody.query, "current", 10);
      assertTrue(found);
      
      assertTrue(found.hasOwnProperty("id"));
      assertMatch(/^\d+$/, found["id"]);
      assertTrue(found.hasOwnProperty("query"));
      assertEqual(found["query"], queryBody.query);
      assertTrue(found.hasOwnProperty("bindVars"));
      assertEqual(found["bindVars"], {});
      assertTrue(found.hasOwnProperty("runTime"));
      assertEqual(typeof found["runTime"], 'number');
      assertTrue(found.hasOwnProperty("started"));
      assertTrue(found.hasOwnProperty("state"));
      assertEqual(typeof found["state"], 'string');
    },

    test_should_track_running_queries__with_bind_parameters: function() {
      send_queries_with_bind();
      let found = wait_for_query(queryWithBindBody.query, "current", 10);
      assertTrue(found);
      assertTrue(found.hasOwnProperty("id"));
      assertMatch(/^\d+$/, found["id"]);
      assertTrue(found.hasOwnProperty("query"));
      assertEqual(found["query"], queryWithBindBody.query);
      assertTrue(found.hasOwnProperty("bindVars"));
      assertEqual(found["bindVars"], {"value": 4});
      assertTrue(found.hasOwnProperty("runTime"));
      assertEqual(typeof found["runTime"], 'number');
      assertTrue(found.hasOwnProperty("started"));
      assertTrue(found.hasOwnProperty("state"));
      assertEqual(typeof found["state"], 'string');
    },

    test_should_track_slow_queries_by_threshold: function() {
      send_fast_queries(1, "false");
      let found = wait_for_query(fastQueryBody.query, "slow", 1);
      assertFalse(found);

      arango.PUT(properties, {slowQueryThreshold: 0.1});

      send_fast_queries(1, "false");
      found = wait_for_query(fastQueryBody.query, "current", 1);
      assertFalse(found);
      found = wait_for_query(fastQueryBody.query, "slow", 1);
      assertTrue(found);
      assertTrue(found.hasOwnProperty("query"));
      assertEqual(found["query"], fastQueryBody.query);
      assertEqual(found["bindVars"], {});
      assertTrue(found.hasOwnProperty("runTime"));
      assertEqual(typeof found["runTime"], 'number');
      assertTrue(found.hasOwnProperty("started"));
      assertEqual(found["state"], "finished");
    },

    test_should_track_at_most_n_slow_queries: function() {
      let max = 3;
      arango.PUT_RAW(properties, {slowQueryThreshold: 0.1, maxSlowQueries: max});
      send_fast_queries(6, "false");

      let doc = arango.GET_RAW(slow);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody.length, max);
    },
    
    test_should_not_track_slow_queries_if_turned_off: function() {
      let found = wait_for_query(fastQueryBody.query, "slow", 1);
      assertFalse(found);

      arango.PUT(properties, {slowQueryThreshold: 0.1, trackSlowQueries: false});
      send_fast_queries(1, "false");

      found = wait_for_query(fastQueryBody.query, "slow", 1);
      assertFalse(found);
    },

    test_should_truncate_the_query_string_to_at_least_64_bytes: function() {
      arango.PUT(properties, {slowQueryThreshold: 2, maxQueryStringLength: 12});
      let doc = arango.GET_RAW(properties);
      assertEqual(doc.parsedBody["maxQueryStringLength"], 64);
    },

    test_should_truncate_the_query_string: function() {
      arango.PUT(properties, {slowQueryThreshold: 0.1, maxQueryStringLength: 64});
      arango.POST_RAW(queryEndpoint, longQueryBody);
      let doc = arango.GET_RAW(slow);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody.length, 1);
      // This string is exactly 64 bytes long;
      let shortened = "FOR x IN 1..1 LET y = SLEEP(0.2) LET a = y LET b = a LET c = b L";
      let found = contains_query(doc.parsedBody, shortened + "...");
      assertTrue(found);
      assertTrue(found.hasOwnProperty("query"));
      assertEqual(found["query"], shortened + "...");
      assertEqual(found["bindVars"], {});
    },

    test_should_properly_truncate_UTF8_symbols: function() {
      arango.PUT(properties, {slowQueryThreshold: 0.1, maxQueryStringLength: 64});
      arango.POST(queryEndpoint, {query: 'FOR x IN 1..1 LET y = SLEEP(0.2) LET a= y LET b= a LET c= "ööööööö" RETURN c'});
      let doc = arango.GET_RAW(slow);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody.length, 1);
      // This string is exactly 64 bytes long;
      let shortened = "FOR x IN 1..1 LET y = SLEEP(0.2) LET a= y LET b= a LET c= \"öö";
      let found = contains_query(doc.parsedBody, shortened + "...");
      assertTrue(found);
      assertTrue(found.hasOwnProperty("query"));
      assertEqual(found["query"], shortened + "...");
      assertEqual(found["bindVars"], {});
    },

    test_should_be_able_to_kill_a_running_query: function() {
      send_queries();
      sleep(3);
      let doc = arango.GET_RAW(current);
      let found = false;
      let id = "";
      doc.parsedBody.forEach( q => {
        if (q["query"] === queryBody.query) {
          found = true;
          id = q["id"];
        }
      });
      assertTrue(found);
      doc = arango.DELETE_RAW(api + "/" + id);
      assertEqual(doc.code, 200);
      sleep(5);
      doc = arango.GET_RAW(current);
      found = contains_query(doc.parsedBody, queryBody.query);
      assertFalse(found);
    }
  };
}

jsunity.run(queryAnalysisSuite);
return jsunity.done();
