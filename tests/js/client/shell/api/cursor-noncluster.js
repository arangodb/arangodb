/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertIdentical, assertNotIdentical, assertNotNull */

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
// / @author Wilfried Goesgens
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
  const cn2 = "testCollection2";
  const cn3 = "testCollection3";
  const queryCache = require("@arangodb/aql/cache");
  let mode = "demand";
  return {

    setUp: function() {
      db._create(cn);
      db._create(cn2);
      db._create(cn3);

      let res = queryCache.properties();
      mode = res.mode;
      res = queryCache.properties({"mode": "demand"});
      assertEqual(res.mode, "demand");
      res = queryCache.clear();
      assertEqual(res.code, 200);
    },

    tearDown: function() {
      const res = queryCache.properties({"mode": mode});
      assertEqual(res.mode, mode);
      db._drop(cn);
      db._drop(cn2);
      db._drop(cn3);
    },

    test_testing_streaming_transaction_aborted_set: function() {
      let res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn2}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn3}`).data;
      assertEqual(res.code, 201);
      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      let trx = db._createTransaction({collections: {write: [cn, cn2], read: cn3}});
      let trxStatus = trx.status();
      assertTrue(trxStatus.hasOwnProperty("id"));
      assertTrue(trxStatus.hasOwnProperty("status"));
      const trxId = trxStatus.id;
      assertEqual(trxStatus.status, "running");
      try {
        res = trx.query(`UPDATE { _key: "a", value: 1 } IN ${cn}`).data;
        assertEqual(res.code, 201);
        res = trx.query(`UPDATE { _key: "a", value: 1 } IN ${cn2}`).data;
        assertEqual(res.code, 201);


        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn3} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = trx.query(`UPDATE { _key: "a", value: 2 } IN ${cn}`).data;
        assertEqual(res.code, 201);

        res = trx.query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 2);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);
      } finally {
        res = trx.abort();
        assertTrue(res.hasOwnProperty("id"));
        assertTrue(res.hasOwnProperty("status"));
        assertEqual(res.id, trxId);
        assertEqual(res.status, "aborted");
        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn3} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);
      }
    },


    test_testing_streaming_transaction_committed_set: function() {
      let res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn2}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn3}`).data;
      assertEqual(res.code, 201);
      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      let trx = db._createTransaction({collections: {write: [cn, cn2], read: cn3}});
      let trxStatus = trx.status();
      assertTrue(trxStatus.hasOwnProperty("id"));
      assertTrue(trxStatus.hasOwnProperty("status"));
      const trxId = trxStatus.id;
      assertEqual(trxStatus.status, "running");

      let trx2 = db._createTransaction({collections: {read: cn}});
      trxStatus = trx2.status();
      assertTrue(trxStatus.hasOwnProperty("id"));
      assertTrue(trxStatus.hasOwnProperty("status"));
      const trx2Id = trxStatus.id;
      assertEqual(trxStatus.status, "running");

      try {
        res = trx.query(`UPDATE { _key: "a", value: 2 } IN ${cn}`).data;
        assertEqual(res.code, 201);

        res = trx.query(`UPDATE { _key: "a", value: 1 } IN ${cn2}`).data;
        assertEqual(res.code, 201);

        res = trx2.query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = trx.query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 2);

        res = trx.query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 1);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn3} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn3} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);
      } finally {
        res = trx.commit();
        assertTrue(res.hasOwnProperty("id"));
        assertTrue(res.hasOwnProperty("status"));
        assertEqual(res.id, trxId);
        assertEqual(res.status, "committed");

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 2);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 2);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 2);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 1);

        res = trx2.query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 2);

        res = db._query(`FOR doc IN ${cn3} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);

        res = trx2.commit();
        assertTrue(res.hasOwnProperty("id"));
        assertTrue(res.hasOwnProperty("status"));
        assertEqual(res.id, trx2Id);
        assertEqual(res.status, "committed");
      }
    },

    test_testing_js_transaction_aborted_set: function() {
      let res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn2}`).data;
      assertEqual(res.code, 201);
      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      try {
        res = db._executeTransaction({
          collections: {write: [cn], read: [cn2]},
          action: function() {
            const arangodb = require("@arangodb");
            const db = arangodb.db;
            db._query("UPDATE { _key: 'a', value: 1 } IN testCollection");
            const err = new arangodb.ArangoError();
            err.errorNum = arangodb.ERROR_TRANSACTION_ABORTED;
            err.errorMessage = "Aborting transaction that updated value to 1";
            throw err;
          }
        });
      } catch (err) {
        assertEqual(errors.ERROR_TRANSACTION_ABORTED.code, err.errorNum);
        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
        assertEqual(res.code, 201);
        assertFalse(res.cached);
        assertEqual(res.result[0].value, 0);

        res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
        assertEqual(res.code, 201);
        assertTrue(res.cached);
        assertEqual(res.result[0].value, 0);
      }
    },

    test_testing_js_transaction_committed_set: function() {
      let res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn}`).data;
      assertEqual(res.code, 201);
      res = db._query(`INSERT { _key: "a", value: 0 } IN ${cn2}`).data;
      assertEqual(res.code, 201);
      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 0);
      res = db._executeTransaction({
        collections: {write: [cn], read: [cn2]},
        action: function() {
          const arangodb = require("@arangodb");
          const db = arangodb.db;
          const res = db._query("UPDATE { _key: 'a', value: 1 } IN testCollection");
          return "updated";
        }
      });
      assertEqual(res, "updated");
      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 1);

      res = db._query(`FOR doc IN ${cn} FILTER doc._key == "a" RETURN doc`, {}, {"cache": false}).data;
      assertEqual(res.code, 201);
      assertFalse(res.cached);
      assertEqual(res.result[0].value, 1);

      res = db._query(`FOR doc IN ${cn2} FILTER doc._key == "a" RETURN doc`, {}, {"cache": true}).data;
      assertEqual(res.code, 201);
      assertTrue(res.cached);
      assertEqual(res.result[0].value, 0);
    },
  };
}

jsunity.run(testing_the_query_cacheSuite);
jsunity.run(testing_the_query_cache_in_transcationSuite);
return jsunity.done();
