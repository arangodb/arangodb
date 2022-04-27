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
// all query;
////////////////////////////////////////////////////////////////////////////////;
function all_query_will_take_a_while_when_using_sslSuite () {
  let cn = "UnitTestsCollectionSimple";
  return {
    setUpAll: function() {
      db._create(cn, { "numberOfShards" : 8 });

      let docs=[];
      for (var i = 0; i < 1500; ++i) {
        docs.push({ n: i });
      }
      db[cn].save(docs);
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_get_all_documents: function() {
      let cmd = api + "/all";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1000);
      assertEqual(doc.parsedBody['count'], 1500);
    },

    test_get_all_documents_with_limit: function() {
      let cmd = api + "/all";
      let body = { "collection" : cn, "limit" : 100 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 100);
      assertEqual(doc.parsedBody['count'], 100);
    },

    test_get_all_documents_with_negative_skip: function() {
      let cmd = api + "/all";
      let body = { "collection" : cn, "skip" : -100 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE.code);
    },

    test_get_all_documents_with_skip: function() {
      let cmd = api + "/all";
      let body = { "collection" : cn, "skip" : 1400 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 100);
      assertEqual(doc.parsedBody['count'], 100);
    },

    test_get_all_documents_with_skip_and_limit: function() {
      let cmd = api + "/all";
      let body = { "collection" : cn, "skip" : 1400, "limit" : 2 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertEqual(doc.parsedBody['count'], 2);
    },
  };
}
jsunity.run(all_query_will_take_a_while_when_using_sslSuite);
return jsunity.done();
