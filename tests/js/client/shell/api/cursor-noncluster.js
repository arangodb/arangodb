/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertIdentical, assertNotIdentical */

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
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";

const jsunity = require("jsunity");
let api = "/_api/cursor";

////////////////////////////////////////////////////////////////////////////////;
// query cache;
////////////////////////////////////////////////////////////////////////////////;
function testing_the_query_cacheSuite () {
  return {
    setUp: function() {
      let doc = arango.GET_RAW("/_api/query-cache/properties");
      let mode = doc.parsedBody['mode'];
      arango.PUT_RAW("/_api/query-cache/properties", "{ \"mode\" : \"demand\" }");

      arango.DELETE_RAW("/_api/query-cache");
    },

    tearDown: function() {
      arango.PUT_RAW("/_api/query-cache/properties", "{ \"mode\" : \"${mode}\" }");
    },

    test_testing_without_cache_attribute_set: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..5 RETURN i\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but not from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_explicitly_disabled_cache: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..5 RETURN i\", \"cache\" : false }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but not from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_enabled_cache: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..5 RETURN i\", \"cache\" : true }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      let stats = doc.parsedBody['extra']['stats'];
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_clearing_the_cache: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..5 RETURN i\", \"cache\" : true }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // now clear cache;
      arango.DELETE_RAW("/_api/query-cache");

      // query again. now response should not come from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
      let stats = doc.parsedBody['extra']['stats'];

      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_fullCount_off: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..10000 LIMIT 5 RETURN i\", \"cache\" : true, \"fullCount\" : false }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertFalse(doc.parsedBody['extra']['stats'].hasOwnProperty('fullCount'));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
      let stats = doc.parsedBody['extra']['stats'];

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertFalse(doc.parsedBody['extra']['stats'].hasOwnProperty('fullCount'));
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_fullCount_on: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..10000 LIMIT 5 RETURN i\", \"cache\" : true, \"options\": { \"fullCount\" : true } }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra']['stats'].hasOwnProperty(('fullCount')));
      assertEqual(doc.parsedBody['extra']['stats']['fullCount'], 10000);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
      let stats = doc.parsedBody['extra']['stats'];

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra']['stats'].hasOwnProperty(('fullCount')));
      assertEqual(doc.parsedBody['extra']['stats']['fullCount'], 10000);
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

    test_testing_fullCount_on_off: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..10000 LIMIT 5 RETURN i\", \"cache\" : true, \"options\": { \"fullCount\" : true } }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra']['stats'].hasOwnProperty(('fullCount')));
      assertEqual(doc.parsedBody['extra']['stats']['fullCount'], 10000);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
      let stats = doc.parsedBody['extra']['stats'];

      // toggle fullcount value;
      body = "{ \"query\" : \"FOR i IN 1..10000 LIMIT 5 RETURN i\", \"cache\" : true, \"options\": { \"fullCount\" : false } }";
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertFalse(doc.parsedBody['extra']['stats'].hasOwnProperty('fullCount'));
      assertNotIdentical(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // toggle fullcount value yet once more;
      body = "{ \"query\" : \"FOR i IN 1..10000 LIMIT 5 RETURN i\", \"cache\" : true, \"options\": { \"fullCount\" : true } }";
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [ 1, 2, 3, 4, 5 ]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra']['stats'].hasOwnProperty(('fullCount')));
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

  };
}
jsunity.run(testing_the_query_cacheSuite);
return jsunity.done();
