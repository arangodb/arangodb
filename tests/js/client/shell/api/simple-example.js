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
// by-example query;
////////////////////////////////////////////////////////////////////////////////;
function by_example_querySuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let body = { "name" : cn, "numberOfShards" : 8 };
      arango.POST("/_api/collection", body);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_finds_the_examples: function() {
      let body = { "i" : 1 };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d1 = doc.parsedBody['_id'];

      body = { "i" : 1, "a" : { "j" : 1 } };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d2 = doc.parsedBody['_id'];

      body = { "i" : 1, "a" : { "j" : 1, "k" : 1 } };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d3 = doc.parsedBody['_id'];

      body = { "i" : 1, "a" : { "j" : 2, "k" : 2 } };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d4 = doc.parsedBody['_id'];

      body = { "i" : 2 };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d5 = doc.parsedBody['_id'];

      body = { "i" : 2, "a" : 2 };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d6 = doc.parsedBody['_id'];

      body = { "i" : 2, "a" : { "j" : 2, "k" : 2 } };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);
      let d7 = doc.parsedBody['_id'];

      let cmd = api + "/by-example";
      body = { "collection" : cn, "example" : { "i" : 1 } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 4);
      assertEqual(doc.parsedBody['count'], 4);

      let other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['_id']));
      assertEqual(other.sort(), [d1, d2, d3, d4].sort());

      cmd = api + "/by-example";
      body = { "collection" : cn, "example" : { "a" : { "j" : 1 } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['count'], 1);
      other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['_id']));
      assertEqual(other, [d2]);

      cmd = api + "/by-example";
      body = { "collection" : cn, "example" : { "a.j" : 1 } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertEqual(doc.parsedBody['count'], 2);
      other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['_id']));
      assertEqual(other.sort(), [d2, d3].sort());

      cmd = api + "/first-example";
      body = { "collection" : cn, "example" : { "a.j" : 1, "a.k" : 1 } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['document']['_id'], d3);

      cmd = api + "/first-example";
      body = { "collection" : cn, "example" : { "a.j" : 1, "a.k" : 2 } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_finds_the_first_example__invalid_collection: function() {
      let cmd = api + "/first-example";
      let body = { "collection" : "NonExistingCollection", "example" : { "a" : 1} };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// by-example query with skip / limit;
////////////////////////////////////////////////////////////////////////////////;
function by_example_query_with_skipSuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let body = { "name" : cn, "numberOfShards" : 8 };
      arango.POST("/_api/collection", body);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_finds_the_examples_with_skip: function() {
      let body = { "someAttribute" : "someValue", "someOtherAttribute" : "someOtherValue" };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);

      body = { "someAttribute" : "someValue", "someOtherAttribute2" : "someOtherValue2" };
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      assertEqual(doc.code, 202);

      let cmd = api + "/by-example";
      body = { "collection" : cn, "example" : { "someAttribute" : "someValue" } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertEqual(doc.parsedBody['count'], 2);

      body = { "collection" : cn, "example" : { "someAttribute" : "someValue" }, "skip" : 1 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['count'], 1);

      body = { "collection" : cn, "example" : { "someAttribute" : "someValue" }, "skip" : 1, "limit" : 1 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['count'], 1);

      body = { "collection" : cn, "example" : { "someAttribute" : "someValue" }, "skip" : 2 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 0);
      assertEqual(doc.parsedBody['count'], 0);
    },
  };
}
jsunity.run(by_example_querySuite);
jsunity.run(by_example_query_with_skipSuite);
return jsunity.done();
