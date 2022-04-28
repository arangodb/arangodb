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


let api = "/_api/simple";

////////////////////////////////////////////////////////////////////////////////;
// any query;
////////////////////////////////////////////////////////////////////////////////;
function any_querySuite () {
  let cn = "UnitTestsCollectionSimple";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);

      let body = { "name" : cn, "numberOfShards" : 8 };
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);
      cid = doc.parsedBody['id'];
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_get_any_document__wrong_collection: function() {
      let cmd = api + "/any";
      let body = { "collection" : "NonExistingCollection" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_get_any_document__empty_collection: function() {
      let cmd = api + "/any";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['document'], null);
    },

    test_get_any_document__single_document_collection: function() {
      arango.POST_RAW(`/_api/document?collection=${cn}`, { "n" : 30 });

      let cmd = api + "/any";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['document']['n'], 30);
    },

    test_get_any_document__non_empty_collection: function() {
      let docs = [];
      for (let i = 0; i < 10; i++) {
        docs.push({ "n" : i });
      }
      db[cn].save(docs);

      let cmd = api + "/any";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['document']['n'], 'number');
    },
  };
}
jsunity.run(any_querySuite);
return jsunity.done();
