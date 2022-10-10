/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertIdentical, assertNotIdentical, assertNotNull */

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
const contentType = forceJson ? "application/json" : "application/x-velocypack";
const db = require("@arangodb").db;
const errors = require('@arangodb').errors;

const jsunity = require("jsunity");
let api = "/_api/cursor";

////////////////////////////////////////////////////////////////////////////////;
// query cache;
////////////////////////////////////////////////////////////////////////////////;
function testing_the_query_cacheSuite() {
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but not from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but not from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      let stats = doc.parsedBody['extra']['stats'];
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);

      // should see same result, but now from cache;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertFalse(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
      let stats = doc.parsedBody['extra']['stats'];

      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 201);
      result = doc.parsedBody['result'];
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
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
      assertEqual(result, [1, 2, 3, 4, 5]);
      assertTrue(doc.parsedBody['cached']);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('stats')));
      assertTrue(doc.parsedBody['extra']['stats'].hasOwnProperty(('fullCount')));
      assertEqual(doc.parsedBody['extra']['stats'], stats);
      assertTrue(doc.parsedBody['extra'].hasOwnProperty(('warnings')));
      assertEqual(doc.parsedBody['extra']['warnings'], []);
    },

  };
}

function testing_the_query_cache_in_transcationSuite() {
  const cn = "testCollection";
  let mode = "demand";
  return {

    setUp: function() {
      let res = arango.POST_RAW(`/_api/collection`, {"name": cn});
      assertEqual(res.code, 200);
      res = arango.GET_RAW("/_api/query-cache/properties");
      mode = res.parsedBody['mode'];
      arango.PUT_RAW("/_api/query-cache/properties", "{ \"mode\" : \"demand\" }");
      arango.DELETE_RAW("/_api/query-cache");
    },

    tearDown: function() {
      arango.PUT_RAW("/_api/query-cache/properties", "{ \"mode\" : \"${mode}\" }");
      db._drop(cn);
    },

    test_testing_streaming_transaction_aborted_set: function() {
      let trx = null;
      let res = arango.POST_RAW("/_api/cursor", {"query": `UPSERT { _key: "a" } INSERT { _key: "a", value: 0 } UPDATE { value: 0 } IN ${cn}`});
      assertEqual(res.code, 201);
      res = arango.POST_RAW("/_api/cursor", {"query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`});
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 0);
      res = arango.POST_RAW("/_api/transaction/begin", {collections: {write: cn}});
      assertEqual(res.code, 201);
      assertNotUndefined(res.parsedBody.result.id);
      trx = res.parsedBody.result.id;
      assertNotNull(trx);
      try {
        res = arango.POST_RAW("/_api/cursor", {"query": `UPDATE { _key: "a", value: 1 } IN ${cn}`}, {"x-arango-trx-id": trx});

        assertEqual(res.code, 201);
        assertFalse(res.body.error);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);


        res = arango.POST_RAW("/_api/cursor", {"query": `UPDATE { _key: "a", value: 2 } IN ${cn}`}, {"x-arango-trx-id": trx});
        assertEqual(res.code, 201);
        assertFalse(res.error);


        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        }, {"x-arango-trx-id": trx});
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 2);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": false
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);

      } finally {
        res = arango.DELETE("/_api/transaction/" + encodeURIComponent(trx), {}, {});
        assertEqual(res.code, 200);
        assertEqual(res.result.id, trx);
        assertEqual(res.result.status, "aborted");
        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": false
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);
      }
    },

    test_testing_streaming_transaction_committed_set: function() {
      let trx = null;
      let res = arango.POST_RAW("/_api/cursor", {"query": `UPSERT { _key: "a" } INSERT { _key: "a", value: 0 } UPDATE { value: 0 } IN ${cn}`});
      assertEqual(res.code, 201);
      res = arango.POST_RAW("/_api/cursor", {"query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`});
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 0);
      res = arango.POST_RAW("/_api/transaction/begin", {collections: {write: cn}});
      assertEqual(res.code, 201);
      assertNotUndefined(res.parsedBody.result.id);
      trx = res.parsedBody.result.id;
      assertNotNull(trx);
      try {
        res = arango.POST_RAW("/_api/cursor", {"query": `UPDATE { _key: "a", value: 1 } IN ${cn}`}, {"x-arango-trx-id": trx});

        assertEqual(res.code, 201);
        assertFalse(res.body.error);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);

        res = arango.POST_RAW("/_api/cursor", {"query": `UPDATE { _key: "a", value: 2 } IN ${cn}`}, {"x-arango-trx-id": trx});
        assertEqual(res.code, 201);
        assertFalse(res.error);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        }, {"x-arango-trx-id": trx});
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 2);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": true
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": false
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 0);
      } finally {
        res = arango.PUT_RAW("/_api/transaction/" + encodeURIComponent(trx), {}, {});
        assertEqual(res.code, 200);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result.status, "committed");

        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "cache": false
        });
        assertEqual(res.code, 201);
        assertFalse(res.error);
        assertEqual(res.parsedBody.result[0].value, 2);
      }
    },

    test_testing_streaming_js_transaction_aborted_set: function() {
      let res = arango.POST_RAW("/_api/cursor", {"query": `UPSERT { _key: "a" } INSERT { _key: "a", value: 0 } UPDATE { value: 0 } IN ${cn}`});
      assertEqual(res.code, 201);
      res = arango.POST_RAW("/_api/cursor", {"query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`});
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 0);
      try {
        res = arango.POST_RAW("/_api/transaction",
          {
            "collections": {"write": [cn]},
            "action": `function() { const arangodb = require("@arangodb"); const db = arangodb.db; db._query("UPDATE { _key: 'a', value: 1 } IN ${cn}"); db._query("RETURN SLEEP(3)"); let d = new Date(); d = String(d.getSeconds()).padStart(2, "0") + "." + String(d.getMilliseconds())[0]; const err = new arangodb.ArangoError(); err.errorNum = arangodb.ERROR_TRANSACTION_ABORTED; err.errorMessage = "\\n" + d + "  Aborting transaction that updated value to 1\\n"; throw err;}`
          });
        assertEqual(res.code, 410);
        assertTrue(res.parsedBody.error);
        throw(res.parsedBody.errorNum);
      } catch (err) {
        assertEqual(errors.ERROR_TRANSACTION_ABORTED.code, err);
        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "bindVars": {},
          "cache": true
        });
        assertEqual(res.code, 201);
        assertEqual(res.parsedBody.result[0].value, 0);
        res = arango.POST_RAW("/_api/cursor", {
          "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
          "bindVars": {},
          "cache": false
        });
        assertEqual(res.code, 201);
        assertEqual(res.parsedBody.result[0].value, 0);
      }
    },

    test_testing_js_transaction_committed_set: function() {
      let res = arango.POST_RAW("/_api/cursor", {"query": `UPSERT { _key: "a" } INSERT { _key: "a", value: 0 } UPDATE { value: 0 } IN ${cn}`});
      assertEqual(res.code, 201);
      res = arango.POST_RAW("/_api/cursor", {"query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`});
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 0);
      res = arango.POST_RAW("/_api/transaction",
        {
          "collections": {"write": [cn]},
          "action": `function() { const arangodb = require("@arangodb"); const db = arangodb.db; db._query("UPDATE { _key: 'a', value: 2 } IN ${cn}"); db._query("RETURN SLEEP(3)"); }`
        });
      assertEqual(res.code, 200);
      assertFalse(res.parsedBody.error);
      res = arango.POST_RAW("/_api/cursor", {
        "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
        "bindVars": {},
        "cache": true
      });
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 2);
      res = arango.POST_RAW("/_api/cursor", {
        "query": `FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`,
        "bindVars": {},
        "cache": false
      });
      assertEqual(res.code, 201);
      assertEqual(res.parsedBody.result[0].value, 2);
    },
  };
}

jsunity.run(testing_the_query_cacheSuite);
jsunity.run(testing_the_query_cache_in_transcationSuite);
return jsunity.done();
