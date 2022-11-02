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

////////////////////////////////////////////////////////////////////////////////;
// unique constraints during create;
////////////////////////////////////////////////////////////////////////////////;
function creating_skip_list_index_dealing_with_unique_constraints_violationSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_does_not_create_the_index_in_case_of_violation: function() {
      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      let body = { "a" : 1, "b" : 1 };
      let doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // create another document;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // try to create the index;
      let cmd = `/_api/index?collection=${cn}`;
      body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ] };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
    },

    test_does_not_create_the_index_in_case_of_violation__null_attributes: function() {
      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      let body = { "a" : null, "b" : 1 };
      let doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // create another document;
      body = { "a" : null, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // try to create the index;
      let cmd = `/_api/index?collection=${cn}`;
      body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ] };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
    },

    test_does_not_create_the_index_in_case_of_violation__sparse_index: function() {
      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      let body = { "a" : 1, "b" : 1 };
      let doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // create another document;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // try to create the index;
      let cmd = `/_api/index?collection=${cn}`;
      body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ], "sparse" : true };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
    },

    test_creates_the_index_in_case_of_null_attributes__sparse_index: function() {
      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      let body = { "a" : null, "b" : 1 };
      let doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // create another document;
      body = { "a" : null, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      // try to create the index;
      let cmd = `/_api/index?collection=${cn}`;
      body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ], "sparse" : true };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// unique constraints during create;
////////////////////////////////////////////////////////////////////////////////;
function creating_documents_dealing_with_unique_constraintsSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_rolls_back_in_case_of_violation: function() {
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);

      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id1 = doc.parsedBody['_id'];
      assertEqual(typeof id1, 'string');

      let rev1 = doc.parsedBody['_rev'];
      assertEqual(typeof rev1, 'string');

      // check it;
      let cmd2 = `/_api/document/${id1}`;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // create a unique constraint violation;
      body = { "a" : 1, "b" : 2 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check it again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // third try (make sure the rollback has not destroyed anything);
      body = { "a" : 1, "b" : 3 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check it again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);
    },

    test_rolls_back_in_case_of_violation__sparse_index: function() {
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);

      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id1 = doc.parsedBody['_id'];
      assertEqual(typeof id1, 'string');

      let rev1 = doc.parsedBody['_rev'];
      assertEqual(typeof rev1, 'string');

      // check it;
      let cmd2 = `/_api/document/${id1}`;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // create a unique constraint violation;
      body = { "a" : 1, "b" : 2 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check it again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // third try (make sure the rollback has not destroyed anything);
      body = { "a" : 1, "b" : 3 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check it again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// unique constraints during update;
////////////////////////////////////////////////////////////////////////////////;
function updating_documents_dealing_with_unique_constraintsSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_rolls_back_in_case_of_violation_update: function() {
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);

      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id1 = doc.parsedBody['_id'];
      assertEqual(typeof id1, 'string');

      let rev1 = doc.parsedBody['_rev'];
      assertEqual(typeof rev1, 'string');

      // check it;
      let cmd2 = `/_api/document/${id1}`;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // create a second document;
      body = { "a" : 2, "b" : 2 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id2 = doc.parsedBody['_id'];
      assertEqual(typeof id2, 'string');

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');

      // create a unique constraint violation during update;
      body = { "a" : 2, "b" : 3 };
      doc = arango.PUT_RAW(cmd2, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check first document again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      let rev3 = doc.parsedBody['_rev'];
      assertEqual(typeof rev3, 'string');

      // check second document again;
      let cmd3 = `/_api/document/${id2}`;
      doc = arango.GET_RAW(cmd3);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 2);
      assertEqual(doc.parsedBody['b'], 2);
      assertEqual(doc.parsedBody['_id'], id2);
      assertEqual(doc.parsedBody['_rev'], rev2);

      // third try (make sure the rollback has not destroyed anything);
      body = { "a" : 2, "b" : 4 };
      doc = arango.PUT_RAW(cmd2, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check the first document again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);
      assertNotEqual(doc.parsedBody['_rev'], rev2);
    },

    test_rolls_back_in_case_of_violation__sparse_index_update: function() {
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);

      // create a document;
      let cmd1 = `/_api/document?collection=${cn}`;
      body = { "a" : 1, "b" : 1 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id1 = doc.parsedBody['_id'];
      assertEqual(typeof id1, 'string');

      let rev1 = doc.parsedBody['_rev'];
      assertEqual(typeof rev1, 'string');

      // check it;
      let cmd2 = `/_api/document/${id1}`;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      // create a second document;
      body = { "a" : 2, "b" : 2 };
      doc = arango.POST_RAW(cmd1, body);

      assertEqual(doc.code, 201);

      let id2 = doc.parsedBody['_id'];
      assertEqual(typeof id2, 'string');

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');

      // create a unique constraint violation during update;
      body = { "a" : 2, "b" : 3 };
      doc = arango.PUT_RAW(cmd2, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check first document again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);

      let rev3 = doc.parsedBody['_rev'];
      assertEqual(typeof rev3, 'string');

      // check second document again;
      let cmd3 = `/_api/document/${id2}`;
      doc = arango.GET_RAW(cmd3);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 2);
      assertEqual(doc.parsedBody['b'], 2);
      assertEqual(doc.parsedBody['_id'], id2);
      assertEqual(doc.parsedBody['_rev'], rev2);

      // third try (make sure the rollback has not destroyed anything);
      body = { "a" : 2, "b" : 4 };
      doc = arango.PUT_RAW(cmd2, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);

      // check the first document again;
      doc = arango.GET_RAW(cmd2);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['a'], 1);
      assertEqual(doc.parsedBody['b'], 1);
      assertEqual(doc.parsedBody['_id'], id1);
      assertEqual(doc.parsedBody['_rev'], rev1);
      assertNotEqual(doc.parsedBody['_rev'], rev2);
    }
  };
}
jsunity.run(creating_skip_list_index_dealing_with_unique_constraints_violationSuite);
jsunity.run(creating_documents_dealing_with_unique_constraintsSuite);
jsunity.run(updating_documents_dealing_with_unique_constraintsSuite);
return jsunity.done();
