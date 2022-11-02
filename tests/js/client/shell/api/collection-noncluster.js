/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual */

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

let api = "/_api/collection";

function dealing_with_collectionsSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_calculating_the_checksum_for_a_collection: function() {
      let cmd = api + "/" + cn + "/checksum";
      let doc = arango.GET_RAW(cmd);

      // empty collection;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      let r1 = doc.parsedBody['revision'];
      assertEqual(typeof r1, 'string');
      assertNotEqual(r1, "");
      let c1 = doc.parsedBody['checksum'];
      assertEqual(typeof c1, 'string');
      assertEqual(c1, "0");

      // create a new document;
      let body = { "test" : 1 };
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);

      // fetch checksum again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let r2 = doc.parsedBody['revision'];
      assertNotEqual(r2, "");
      assertNotEqual(r2, r1);
      let c2 = doc.parsedBody['checksum'];
      assertEqual(typeof c2, 'string');
      assertNotEqual(c2, "0");
      assertNotEqual(c2, c1);

      // create another document;
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);

      // fetch checksum again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let r3 = doc.parsedBody['revision'];
      assertNotEqual(r3, "");
      assertNotEqual(r3, r1);
      assertNotEqual(r3, r2);
      let c3 = doc.parsedBody['checksum'];
      assertEqual(typeof c3, 'string');
      assertNotEqual(c3, "0");
      assertNotEqual(c3, c1);
      assertNotEqual(c3, c2);

      // fetch checksum <withData>;
      doc = arango.GET_RAW(cmd + "?withData=true");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let r4 = doc.parsedBody['revision'];
      assertEqual(r4, r3);
      let c4 = doc.parsedBody['checksum'];
      assertEqual(typeof c4, 'string');
      assertNotEqual(c4, "0");
      assertNotEqual(c4, c1);
      assertNotEqual(c4, c2);
      assertNotEqual(c4, c3);

      // truncate;
      doc = arango.PUT(`/_api/collection/${cn}/truncate`, "");

      // fetch checksum again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let r5 = doc.parsedBody['revision'];
      assertNotEqual(r5, "");
      let c5 = doc.parsedBody['checksum'];
      assertEqual(typeof c5, 'string');
      assertEqual(c5, "0");
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// properties of a collection;
////////////////////////////////////////////////////////////////////////////////;
function propertiesSuite () {
  let cn = "UnitTestsCollectionKeygen";
  
  return {
    tearDown: function() {
      db._drop(cn);
    },
    test_create_collection_with_explicit_keyOptions_property__autoinc_keygen: function() {
      let cmd = "/_api/collection";
      let body = { "name" : cn, "waitForSync" : false, "type" : 2, "keyOptions" : {"type": "autoincrement", "offset": 7, "increment": 99, "allowUserKeys": false } };
      
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      let cid = doc.parsedBody['id'];

      cmd = api + "/" + cn + "/properties";
      body = { "waitForSync" : true };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertTrue(doc.parsedBody['waitForSync']);
      assertNotEqual(doc.parsedBody['objectId'], 0);
      assertEqual(doc.parsedBody['keyOptions']['type'], "autoincrement");
      assertEqual(doc.parsedBody['keyOptions']['increment'], 99);
      assertEqual(doc.parsedBody['keyOptions']['offset'], 7);
      assertFalse(doc.parsedBody['keyOptions']['allowUserKeys']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// renames a collection;
////////////////////////////////////////////////////////////////////////////////;
function renamingSuite () {
  const cn = "UnitTestsCollectionBasics";
  const cn2 = "UnitTestsCollectionBasics2";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
      db._drop(cn2);
    },
    test_rename_a_collection_by_identifier: function() {
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({ "Hallo" : "World" });
      }
      cid.save(docs);

      assertEqual(db[cn].count(), 10);
      assertEqual(cid.count(), 10);

      let body = { "name" : cn2 };
      let cmd = api + "/" + cn + "/rename";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn2);
      assertEqual(doc.parsedBody['status'], 3);

      assertEqual(db[cn2].count(), 10);

      cmd = api + "/" + cn;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

      cmd = api + "/" + cn2;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['name'], cn2);
      assertEqual(doc.parsedBody['status'], 3);
    },

    test_rename_a_collection_by_identifier_with_conflict: function() {
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({ "Hallo" : "World" });
      }
      cid.save(docs);

      assertEqual(db[cn].count(), 10);
      assertEqual(cid.count(), 10);

      let cid2 = db._create(cn2);
      docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "Hallo" : "World" });
      }
      cid2.save(docs);
      assertEqual(db[cn2].count(), 20);
      assertEqual(cid2.count(), 20);

      let body = { "name" : cn2};
      let cmd = api + "/" + cn + "/rename";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code);

      assertEqual(db[cn].count(), 10);
      assertEqual(cid.count(), 10);
      assertEqual(db[cn2].count(), 20);
      assertEqual(cid2.count(), 20);
    },

    test_rename_a_new_born_collection_by_identifier: function() {
      let body = { "name" : cn2 };
      let cmd = api + "/" + cn + "/rename";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn2);
      assertEqual(doc.parsedBody['status'], 3);

      cmd = api + "/" + cn;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);

      cmd = api + "/" + cn2;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['name'], cn2);
      assertEqual(doc.parsedBody['status'], 3);
    },

    test_rename_a_new_born_collection_by_identifier_with_conflict: function() {
      db._create(cn2);
      let body = { "name" : cn2 };
      let cmd = api + "/" + cn + "/rename";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code);
    }
  };
}

jsunity.run(dealing_with_collectionsSuite);
jsunity.run(propertiesSuite);
jsunity.run(renamingSuite);
return jsunity.done();
