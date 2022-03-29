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


let api = "/_api/transaction";

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  return {
    test_returns_an_error_if_a_wrong_method_type_is_used: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : " for " };
      let doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_METHOD_NOT_ALLOWED.code);
    },

    test_returns_an_error_if_no_body_is_posted: function() {
      let cmd = api;
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_no_collections_attribute_is_present: function() {
      let cmd = api;
      let body = { "foo" : "bar" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_the_collections_attribute_has_a_wrong_type: function() {
      let cmd = api;
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_collections_sub_attribute_is_wrong: function() {
      let cmd = api;
      let body = { "collections" : { "write": false } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_no_action_is_specified: function() {
      let cmd = api;
      let body = { "collections" : { } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_action_attribute_has_a_wrong_type: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : false };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_action_attribute_contains_broken_code_1: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : " for " };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_action_attribute_contains_broken_code_2: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : "return 1;" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_action_attribute_contains_broken_code_3: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : "function() {" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_transactions_are_nested_1: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { return TRANSACTION({ collections: { }, action: function() { return 1; } }); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_NESTED.code);
    },

    test_returns_an_error_if_transactions_are_nested_2: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { return TRANSACTION({ collections: { }, action: "function () { return 1; }" }); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_NESTED.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// using "wrong" collections;
////////////////////////////////////////////////////////////////////////////////;
function using_wrong_collectionsSuite () {
  let cn1 = "UnitTestsTransactions1";
  let cn2 = "UnitTestsTransactions2";
  return {
    setUp: function() {
      db._create(cn1);
      db._create(cn2);
    },

    tearDown: function() {
      db._drop(cn1);
      db._drop(cn2);
    },

    test_returns_an_error_if_referring_to_a_non_existing_collection: function() {
      let cmd = api;
      let body = { "collections" : { "write": "_meow" }, "action" : "function () { return 1; }" };
      let doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_returns_an_error_when_using_a_non_declared_collection: function() {
      let cmd = api;
      let body = { "collections" : { "write": cn1 }, "action" : `function () { require("internal").db.${cn2}.save({ }); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code);
    },

    test_returns_an_error_when_using_a_collection_in_invalid_mode_1: function() {
      let cmd = api;
      let body = { "collections" : { "read": cn1 }, "action" : `function () { require("internal").db.${cn1}.save({ }); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code);
    },

    test_returns_an_error_when_using_a_collection_in_invalid_mode_2: function() {
      let cmd = api;
      let body = { "collections" : { "read": [ cn1, cn2 ] }, "action" : `function () { require("internal").db.${cn1}.save({ }); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_UNREGISTERED_COLLECTION.code);
    },

    test_returns_an_error_when_using_a_disallowed_operation: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { require("internal").db._create("abc");} ` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_DISALLOWED_OPERATION.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// params;
////////////////////////////////////////////////////////////////////////////////;
function using_parametersSuite () {
  return {
    test_checking_return_parameters: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return [ params[1], params[4] ]; }`, "params" : [ 1, 2, 3, 4, 5 ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ 2, 5 ]);
    },

    test_checking_return_parameters__other_argument_name: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (args) { return [ args[1], args[4] ]; }`, "params" : [ 1, 2, 3, 4, 5 ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ 2, 5 ]);
    },

    test_checking_return_parameters__object_1: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return params['foo']; }`, "params" : { "foo" : "bar" } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], "bar");
    },

    test_checking_return_parameters__object_2: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return params['meow']; }`, "params" : { "foo" : "bar" } };
      let doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], null);
    },

    test_checking_return_parameters__undefined_1: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return params['meow']; }`, "params" : null };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TYPE_ERROR.code);
    },

    test_checking_return_parameters__undefined_2: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return params['meow']; }`, "params" : { } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], null);
    },

    test_checking_return_parameters__undefined_3: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function (params) { return params; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], null);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// non-collection transactions;
////////////////////////////////////////////////////////////////////////////////;
function non_collection_transactionsSuite () {
  return {
    test_returning_a_simple_type: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { return 42; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], 42);
    },

    test_returning_a_compound_type: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { return [ true, { a: 42, b: [ null, true ], c: "foo" } ]; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ true, { "a": 42, "b": [ null, true ], "c": "foo" } ]);
    },

    test_returning_an_exception: function() {
      let cmd = api;
      let body = { "collections" : { }, "action" : `function () { throw "doh!"; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_SERVER_ERROR.code);

      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_INTERNAL.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// single-collection transactions;
////////////////////////////////////////////////////////////////////////////////;
function single_collection_transactionsSuite () {
  let cn = "UnitTestsTransactions";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_read_only__using_write__single: function() {
      let body = { };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);

      assertEqual(db[cn].count(), 3);

      let cmd = api;
      body = { "collections" : { "write": cn }, "action" : `function () { var c = require("internal").db.UnitTestsTransactions; return c.count(); }` };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], 3);

      assertEqual(db[cn].count(), 3);
    },

    test_read_only__using_read__single: function() {
      let body = { };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn}`, body);

      assertEqual(db[cn].count(), 3);

      let cmd = api;
      body = { "collections" : { "read": cn }, "action" : `function () { var c = require("internal").db.UnitTestsTransactions; return c.count(); }` };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], 3);

      assertEqual(db[cn].count(), 3);
    },

    test_committing__single: function() {
      let cmd = api;
      let body = { "collections" : { "write": cn }, "action" : `function () { var c = require("internal").db.UnitTestsTransactions; c.save({ }); c.save({ }); return c.count(); }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], 2);

      assertEqual(db[cn].count(), 2);
    },

    test_aborting__single: function() {
      let cmd = api;
      let body = { "collections" : { "write": cn }, "action" : `function () { var c = require("internal").db.UnitTestsTransactions; c.save({ }); c.save({ }); throw "doh!"; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_INTERNAL.code);

      assertEqual(db[cn].count(), 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// multi-collection transactions;
////////////////////////////////////////////////////////////////////////////////;
function multi_collection_transactionsSuite () {
  let cn1 = "UnitTestsTransactions1";
  let cn2 = "UnitTestsTransactions2";
  return {
    setUp: function() {
      db._create(cn1);
      db._create(cn2);
    },

    tearDown: function() {
      db._drop(cn1);
      db._drop(cn2);
    },

    test_read_only__using_write__multi: function() {
      let body = { };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn2}`, body);

      assertEqual(db[cn1].count(), 3);
      assertEqual(db[cn2].count(), 1);

      let cmd = api;
      body = { "collections" : { "write": [ cn1, cn2 ] }, "action" : `function () { var c1 = require("internal").db.${cn1}; var c2 = require("internal").db.${cn2}; return [ c1.count(), c2.count() ]; }` };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ 3, 1 ]);

      assertEqual(db[cn1].count(), 3);
      assertEqual(db[cn2].count(), 1);
    },

    test_read_only__using_read__multi: function() {
      let body = { };
      let doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn1}`, body);
      doc = arango.POST_RAW(`/_api/document?collection=${cn2}`, body);

      assertEqual(db[cn1].count(), 3);
      assertEqual(db[cn2].count(), 1);

      let cmd = api;
      body = { "collections" : { "read": [ cn1, cn2 ] }, "action" : `function () { var c1 = require("internal").db.${cn1}; var c2 = require("internal").db.${cn2}; return [ c1.count(), c2.count() ]; }` };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ 3, 1 ]);

      assertEqual(db[cn1].count(), 3);
      assertEqual(db[cn2].count(), 1);
    },

    test_committing__multi: function() {
      let cmd = api;
      let body = { "collections" : { "write": [ cn1, cn2 ] }, "action" : `function () { var c1 = require("internal").db.${cn1}; var c2 = require("internal").db.${cn2}; c1.save({ }); c1.save({ }); c2.save({ }); return [ c1.count(), c2.count() ]; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['result'], [ 2, 1 ]);

      assertEqual(db[cn1].count(), 2);
      assertEqual(db[cn2].count(), 1);
    },

    test_aborting__multi: function() {
      let cmd = api;
      let body = { "collections" : { "write": [ cn1, cn2 ] }, "action" : `function () { var c1 = require("internal").db.${cn1}; var c2 = require("internal").db.${cn2}; c1.save({ }); c1.save({ }); c2.save({ }); throw "doh!"; }` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_TRANSACTION_INTERNAL.code);

      assertEqual(db[cn1].count(), 0);
      assertEqual(db[cn2].count(), 0);
    }
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(using_wrong_collectionsSuite);
jsunity.run(using_parametersSuite);
jsunity.run(non_collection_transactionsSuite);
jsunity.run(single_collection_transactionsSuite);
jsunity.run(multi_collection_transactionsSuite);
return jsunity.done();
