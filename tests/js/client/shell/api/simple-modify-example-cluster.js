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
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);
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
      let ids = [];
      doc.parsedBody['result'].forEach(oneDoc => { ids.push(oneDoc['_id']); });
      assertEqual(ids.sort(), [d1,d2,d3,d4].sort());

      cmd = api + "/by-example";
      body = { "collection" : cn, "example" : { "a" : { "j" : 1 } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['count'], 1);
      ids = [];
      doc.parsedBody['result'].forEach(oneDoc => { ids.push(oneDoc['_id']); });
      assertEqual(ids.sort(), [d2].sort());

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
      ids = [];
      doc.parsedBody['result'].forEach(oneDoc => { ids.push(oneDoc['_id']); });
      assertEqual(ids.sort(), [d2,d3].sort());

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
    }
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
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_finds_the_examples_skip: function() {
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
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// remove-by-example query;
////////////////////////////////////////////////////////////////////////////////;
function remove_by_example_querySuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let body = { "name" : cn, "numberOfShards" : 8 };
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);

      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      db[cn].save(docs);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_removes_the_examples: function() {
      let cmd = api + "/remove-by-example";
      let body = { "collection" : cn, "example" : { "value" : 1 } };
      let doc = arango.PUT_RAW(cmd, body);

      // remove first;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 1);

      // remove again;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 0);


      // remove other doc;
      body = { "collection" : cn, "example" : { "value" : 2 } };
      doc = arango.PUT_RAW(cmd, body);

      // remove first;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 1);

      // remove again;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 0);


      // remove others;
      for (let i = 3; i < 8; i ++) {
        body = { "collection" : cn, "example" : { "value" : i } };
        doc = arango.PUT_RAW(cmd, body);

        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(doc.parsedBody['deleted'], 1);
        
        doc = arango.PUT_RAW(cmd, body);
        assertEqual(doc.parsedBody['deleted'], 0);
      }

      // remove non-existing values;
      [
        21, 22, 100, 101, 99, '"meow"', '""', '"null"'
      ].forEach(value => {
        body = { "collection" : cn, "example" : { "value" : value }, "newValue" : { } };
        doc = arango.PUT_RAW(cmd, body);
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(doc.parsedBody['deleted'], 0, doc);
      });

      // remove non-existing values;

      // remove non-existing attributes;
      [
        "value2", "value3", "fox", "meow"
      ].forEach(value => {
        body = { "collection" : cn, "example" : { "value" : value }, "newValue" : { } };
        doc = arango.PUT_RAW(cmd, body);
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(doc.parsedBody['deleted'], 0);
      });

      // insert 10 identical documents;
      let docs = [];
      for (let i = 0; i < 10; i ++) {
        docs.push({ "value99" : 7, "value98" : 1 });
      }
      db[cn].save(docs);
 
      // miss them;
      body = { "collection" : cn, "example" : { "value99" : 7, "value98" : 2} };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 0);

      // miss them again;
      body = { "collection" : cn, "example" : { "value99" : 70, "value98" : 1} };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 0);

      // now remove them;
      body = { "collection" : cn, "example" : { "value99" : 7, "value98" : 1} };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 10);

      // remove again;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 0);
    },

    test_removes_the_examples__with_limit: function() {
      let cmd = api + "/remove-by-example";
      let body = { "collection" : cn, "example" : { "value2" : 99 }, "limit" : 5 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 501);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 501);
      assertEqual(doc.parsedBody['errorNum'], 1470);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// replace-by-example query;
////////////////////////////////////////////////////////////////////////////////;
function replace_by_example_querySuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let body = { "name" : cn, "numberOfShards" : 8 };
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);
      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      db[cn].save(docs);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_replaces_the_examples: function() {
      let cmd = api + "/replace-by-example";
      let body = { "collection" : cn, "example" : { "value" : 1 }, "newValue" : { "foo" : "bar" } };
      let doc = arango.PUT_RAW(cmd, body);

      // replace one;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 1);

      // replace other;
      body = { "collection" : cn, "example" : { "value" : 2 }, "newValue" : { "foo" : "baz" } };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 1);

      // replace all others;
      body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "moo" : "fox" } };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 18);

      // remove non-existing values;
      [
        21, 22, 100, 101, 99, '"meow"', '""', '"null"'
      ].forEach(value => {
        body = { "collection" : cn, "example" : { "value" : value }, "newValue" : { } };
        doc = arango.PUT_RAW(cmd, body);
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(doc.parsedBody['replaced'], 0);
      });
    },

    test_replaces_the_examples__with_limit: function() {
      let cmd = api + "/replace-by-example";
      let body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "foo" : "bar" }, "limit" : 5 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 501);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 501);
      assertEqual(doc.parsedBody['errorNum'], 1470);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// update-by-example query;
////////////////////////////////////////////////////////////////////////////////;
function update_by_example_querySuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let body = { "name" : cn, "numberOfShards" : 8 };
      let doc = arango.POST_RAW("/_api/collection", body);
      assertEqual(doc.code, 200);
      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      db[cn].save(docs);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_updates_the_examples: function() {
      let cmd = api + "/update-by-example";
      let body = { "collection" : cn, "example" : { "value" : 1 }, "newValue" : { "foo" : "bar" } };
      let doc = arango.PUT_RAW(cmd, body);

      // update one;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      // update other;
      body = { "collection" : cn, "example" : { "value" : 2 }, "newValue" : { "foo" : "baz" } };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      // update other, overwrite;
      body = { "collection" : cn, "example" : { "value" : 3 }, "newValue" : { "foo" : "baz", "value" : 12 } };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      // update other, remove;
      body = { "collection" : cn, "example" : { "value" : 12 }, "newValue" : { "value2" : null }, "keepNull" : false };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 2);

      // update all but the 2 from before;
      body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "moo" : "fox" } };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 18);

      // update non-existing values;
      [
        100, 101, 99, '"meow"', '""', '"null"'
      ].forEach(value => {
        body = { "collection" : cn, "example" : { "value" : value }, "newValue" : { } };
        doc = arango.PUT_RAW(cmd, body);
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(doc.parsedBody['updated'], 0, doc);
      });
    },

    test_updates_the_examples__with_limit: function() {
      let cmd = api + "/update-by-example";
      let body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "foo" : "bar", "value2" : 17 }, "limit" : 5 };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 501);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 501);
      assertEqual(doc.parsedBody['errorNum'], 1470);
    }
  };
}

jsunity.run(by_example_querySuite);
jsunity.run(by_example_query_with_skipSuite);
jsunity.run(remove_by_example_querySuite);
jsunity.run(replace_by_example_querySuite);
jsunity.run(update_by_example_querySuite);
return jsunity.done();
