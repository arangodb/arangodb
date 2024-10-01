/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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
      let cid = db._create(cn);
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
      db._create(cn);
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
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      c.save(docs);
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

      // remove some;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 5);

      // remove some more;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 5);

      // remove the rest;
      body = { "collection" : cn, "example" : { "value2" : 99 }, "limit" : 50 };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['deleted'], 10);
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
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      c.save(docs);
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

      // replace some;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 5);

      // replace some more;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 5);

      // replace the rest;
      body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "fox" : "box" }, "limit" : 50 };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['replaced'], 10);
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
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 20; i++) {
        docs.push({ "value" : i, "value2" : 99 });
      }
      c.save(docs);
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

      // update some;
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 5);

      // update some more;
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 5);

      // update the rest;
      body = { "collection" : cn, "example" : { "value2" : 99 }, "newValue" : { "fox" : "box" }, "limit" : 50 };
      doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 10);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// update-by-example query;
////////////////////////////////////////////////////////////////////////////////;
function update_by_example_query__mergeObjectsSuite () {
  let cn = "UnitTestsCollectionByExample";
  return {
    setUp: function() {
      let c = db._create(cn);
      c.save({ "_key" : "one", "value" : "test" });
      c.save({ "_key" : "two", "value" : { "test" : "foo" } });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_updates_the_example__empty_update_value: function() {
      let cmd = api + "/update-by-example";

      // update two;
      let body = { "collection" : cn, "example" : { "value" : { "test" : "foo" } }, "newValue" : { "value" : { } } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/two`, "");
      assertEqual(doc.parsedBody['value'], { "test": "foo" });

      // update with mergeObjects = false;
      body = { "collection" : cn, "example" : { "value" : { "test" : "foo" } }, "mergeObjects" : false, "newValue" : { "value" : { } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/two`, "");
      assertEqual(doc.parsedBody['value'], { });
    },

    test_updates_the_example__mergeObjects_not_specified: function() {
      let cmd = api + "/update-by-example";

      // update one;
      let body = { "collection" : cn, "example" : { "value" : "test" }, "newValue" : { "value" : { "foo" : "bar" } } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      // this will simply overwrite the value;
      doc = arango.GET_RAW(`/_api/document/${cn}/one`);
      assertEqual(doc.parsedBody['value'], { "foo": "bar" });

      // update two  ;
      body = { "collection" : cn, "example" : { "value" : { "test" : "foo" } }, "mergeObjects" : true, "newValue" : { "value" : { "bark" : "bart" } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/two`);
      assertEqual(doc.parsedBody['value'], { "test": "foo", "bark": "bart" });
    },

    test_updates_the_example__mergeObjects_EQ_true: function() {
      let cmd = api + "/update-by-example";

      // update one;
      let body = { "collection" : cn, "example" : { "value" : "test" }, "mergeObjects" : true, "newValue" : { "value" : { "foo" : "bar" } } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      // this will simply overwrite the value;
      doc = arango.GET_RAW(`/_api/document/${cn}/one`);
      assertEqual(doc.parsedBody['value'], { "foo": "bar" });

      // update two  ;
      body = { "collection" : cn, "example" : { "value" : { "test" : "foo" } }, "mergeObjects" : true, "newValue" : { "value" : { "bark" : "bart" } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/two`);
      assertEqual(doc.parsedBody['value'], { "test": "foo", "bark": "bart" });
    },

    test_updates_the_example__mergeObjects_EQ_false: function() {
      let cmd = api + "/update-by-example";

      // update one;
      let body = { "collection" : cn, "example" : { "value" : "test" }, "mergeObjects" : false, "newValue" : { "value" : { "foo" : "bar" } } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/one`);
      assertEqual(doc.parsedBody['value'], { "foo": "bar" });

      // update two  ;
      body = { "collection" : cn, "example" : { "value" : { "test" : "foo" } }, "mergeObjects" : true, "newValue" : { "value" : { "bark" : "bart" } } };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/two`);
      assertEqual(doc.parsedBody['value'], { "test": "foo", "bark": "bart" });
    }
  };
}

jsunity.run(by_example_querySuite);
jsunity.run(by_example_query_with_skipSuite);
jsunity.run(remove_by_example_querySuite);
jsunity.run(replace_by_example_querySuite);
jsunity.run(update_by_example_querySuite);
jsunity.run(update_by_example_query__mergeObjectsSuite);
return jsunity.done();
