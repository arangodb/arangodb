/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertMatch assertNotUndefined assertNotNull */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const internal = require('internal');
const sleep = internal.sleep;
let api = "/_api/cursor";
let reId = /^\d+$/;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;

function dealing_with_cursorsSuite_error_handlingSuite () {
  return {
    test_returns_an_error_if_body_is_missing: function() {
      let cmd = api;
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_EMPTY.code);
    },

    test_returns_an_error_if_query_attribute_is_missing: function() {
      let cmd = api;
      let body = "{ }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
    },

    test_returns_an_error_if_query_is_null: function() {
      let cmd = api;
      let body = "{ \"query\" : null }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_EMPTY.code);
    },

    test_returns_an_error_if_query_string_is_empty: function() {
      let cmd = api;
      let body = "{ \"query\" : \"\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_EMPTY.code);
    },

    test_returns_an_error_if_query_string_is_just_whitespace: function() {
      let cmd = api;
      let body = "{ \"query\" : \"    \" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_PARSE.code);
    },

    test_returns_an_error_if_collection_is_unknown: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN unknowncollection LIMIT 2 RETURN u.n", "count" : true, "bindVars" : {}, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_returns_an_error_if_cursor_identifier_is_missing: function() {
      let cmd = api;
      let doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_returns_an_error_if_cursor_identifier_is_invalid___PUT: function() {
      let cmd = api + "/123456";
      let doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
    },

    test_returns_an_error_if_cursor_identifier_is_invalid___POST: function() {
      let cmd = api + "/123456";
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
    },

    test_returns_an_error_if_memory_limit_is_violated: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..100000 SORT i RETURN i", "memoryLimit" : 100000 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_SERVER_ERROR.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_RESOURCE_LIMIT.code);
    },

    test_returns_no_errors_but_warnings_if_fail_on_warning_is_not_triggered: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..5 RETURN i / 0", "options" : { "failOnWarning" : false } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 5);
      assertEqual(doc.parsedBody['result'], [ null, null, null, null, null ]);
      assertEqual(doc.parsedBody['extra']['warnings'].length, 5);
      assertEqual(doc.parsedBody['extra']['warnings'][0]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][1]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][2]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][3]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][4]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
    },

    test_returns_no_errors_but_warnings_if_fail_on_warning_is_not_triggered__limiting_number_of_warnings: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..5 RETURN i / 0", "options" : { "failOnWarning" : false, "maxWarningCount" : 3 } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 5);
      assertEqual(doc.parsedBody['result'], [ null, null, null, null, null ]);
      assertEqual(doc.parsedBody['extra']['warnings'].length, 3);
      assertEqual(doc.parsedBody['extra']['warnings'][0]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][1]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
      assertEqual(doc.parsedBody['extra']['warnings'][2]['code'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
    },

    test_returns_an_error_if_fail_on_warning_is_triggered: function() {
      let cmd = api;
      let body = "{ \"query\" : \"FOR i IN 1..5 RETURN i / 0\", \"options\" : { \"failOnWarning\" : true } }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_DIVISION_BY_ZERO.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// create and using cursors, continuation;
////////////////////////////////////////////////////////////////////////////////;

function dealing_with_cursorsSuite_handling_a_cursor_with_continuationSuite () {
  let cn = "users";
  return {
    setUpAll: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let docs = [];
      for (let i=0; i < 2001; i++) {
        docs.push({ "_key" : `test${i}`});
      }
      db[cn].save(docs);
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_creates_a_cursor_and_consumes_data_incrementally___PUT: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} RETURN u`, "count" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1000);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1000);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_creates_a_cursor_and_consumes_data_incrementally___POST: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} RETURN u`, "count" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1000);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1000);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2001);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// create and using cursors;
////////////////////////////////////////////////////////////////////////////////;

function dealing_with_cursorsSuite_handling_a_cursorSuite () {
  let cn = "users";
  return {
    setUpAll: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let docs = [];
      for (let i=0; i < 10; i++) {
        docs.push({ "n" : `${i}`});
      }
      db[cn].save(docs);
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_creates_a_cursor_single_run: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 2 RETURN u.n`, "count" : true, "bindVars" : {}, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_cursor_single_run__without_count: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 2 RETURN u.n`, "count" : false, "bindVars" : {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], undefined);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_cursor_single_run__large_batch_size: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 2 RETURN u.n`, "count" : true, "batchSize" : 5 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 2);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_usable_cursor_and_consumes_data_incrementally___PUT: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_creates_a_usable_cursor_and_consumes_data_incrementally___POST: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_creates_a_cursor_and_deletes_it_in_the_middle___PUT: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
    },

    test_creates_a_cursor_and_deletes_it_in_the_middle___POST: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      cmd = api + `/${id}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
    },

    test_deleting_a_cursor: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
    },

    test_deleting_a_deleted_cursor: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);

      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['id'], undefined);
    },

    test_deleting_an_invalid_cursor: function() {
      let cmd = api + "/999999"; // we assume this cursor id is invalid;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['id'], undefined);
    },

    test_creates_a_streaming_cursor_with_a_low_TTL: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..10 LET x = SLEEP(5) RETURN i", "batchSize" : 1, "ttl" : 2, "options": { "stream": true } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];
      cmd = api + `/${id}`;

      for (let i = 0; i < 2; i++) {
        doc = arango.POST_RAW(cmd, "");
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(typeof doc.parsedBody['id'], "string");
        assertMatch(reId, doc.parsedBody['id']);
        assertEqual(doc.parsedBody['id'], id);
        assertTrue(doc.parsedBody['hasMore']);
        assertEqual(doc.parsedBody['result'].length, 1);
        assertFalse(doc.parsedBody['cached']);
      }

      doc = arango.DELETE_RAW(cmd);
    },

    test_creates_a_non_streaming_cursor_with_a_low_TTL: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..10 LET x = SLEEP(5) RETURN i", "batchSize" : 1, "ttl" : 2 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      let id = doc.parsedBody['id'];
      cmd = api + `/${id}`;
      
      for (let i = 0; i < 2; i ++) {
        doc = arango.POST_RAW(cmd, "");
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertFalse(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['code'], 200);
        assertEqual(typeof doc.parsedBody['id'], "string");
        assertMatch(reId, doc.parsedBody['id']);
        assertEqual(doc.parsedBody['id'], id);
        assertTrue(doc.parsedBody['hasMore']);
        assertEqual(doc.parsedBody['result'].length, 1);
        assertFalse(doc.parsedBody['cached']);
      }

      doc = arango.DELETE_RAW(cmd);
    },

    test_creates_a_cursor_that_will_expire___PUT: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 1, "ttl" : 5 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);
      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);

      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      // after this, the cursor might expire eventually;
      // the problem is that we cannot exactly determine the point in time;
      // when it really vanishes, as this depends on thread scheduling, state     ;
      // of the cleanup thread etc.;

      sleep(8); // this should delete the cursor on the server
      doc = arango.PUT_RAW(cmd, "");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_creates_a_cursor_that_will_expire___POST: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 1, "ttl" : 5 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);
      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);

      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      // after this, the cursor might expire eventually;
      // the problem is that we cannot exactly determine the point in time;
      // when it really vanishes, as this depends on thread scheduling, state     ;
      // of the cleanup thread etc.;

      sleep(8); // this should delete the cursor on the server;
      doc = arango.POST_RAW(cmd, "");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_CURSOR_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_creates_a_cursor_that_will_not_expire___PUT: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 1, "ttl" : 60 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);
      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);

      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(5); // this should not delete the cursor on the server;
      doc = arango.PUT_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_cursor_that_will_not_expire___POST: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} LIMIT 5 RETURN u.n`, "count" : true, "batchSize" : 1, "ttl" : 60 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);
      let id = doc.parsedBody['id'];

      cmd = api + `/${id}`;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], "string");
      assertMatch(reId, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(1);

      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);

      sleep(5);// # this should not delete the cursor on the server;
      doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], id);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['count'], 5);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_query_that_executes_a_v8_expression_during_query_optimization: function() {
      let cmd = api;
      let body = { "query" : "RETURN CONCAT('foo', 'bar', 'baz')" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_query_that_executes_a_v8_expression_during_query_execution: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} RETURN PASSTHRU(KEEP(u, '_key'))` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 10);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_query_that_executes_a_dynamic_index_expression_during_query_execution: function() {
      let cmd = api;
      let body = { "query" : `FOR i IN ${cn} FOR j IN ${cn} FILTER i._key == j._key RETURN i._key` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 10);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_query_that_executes_a_dynamic_V8_index_expression_during_query_execution: function() {
      let cmd = api;
      let body = { "query" : `FOR i IN ${cn} FOR j IN ${cn} FILTER j._key == PASSTHRU(i._key) RETURN i._key` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 10);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_cursor_with_different_bind_values: function() {
      let cmd = api;
      let body = { "query" : "RETURN @values", "bindVars" : { "values" : [ null, false, true, -1, 2.5, 3e4, "", " ", "true", "foo bar baz", [ 1, 2, 3, "bar" ], { "foo" : "bar", "" : "baz", " bar-baz " : "foo-bar" } ] } };
      let doc = arango.POST_RAW(cmd, body);

      let values = [ [ null, false, true, -1, 2.5, 3e4, "", " ", "true", "foo bar baz", [ 1, 2, 3, "bar" ], { "foo": "bar", "": "baz", " bar-baz ": "foo-bar" } ] ];
      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'], values);
      assertFalse(doc.parsedBody['cached']);
    },

    test_creates_a_query_that_survives_memory_limit_constraints: function() {
      let cmd = api;
      let body = { "query" : "FOR i IN 1..10000 SORT i RETURN i", "memoryLimit" : 10000000, "batchSize": 10 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertNotNull(doc.parsedBody['id']);
      assertTrue(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 10);
      assertFalse(doc.parsedBody['cached']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking a query;
////////////////////////////////////////////////////////////////////////////////;

function dealing_with_cursorsSuite_checking_a_querySuite () {
  let cn = "users";
  return {
    setUpAll: function() {
      db._drop(cn);
      let cid = db._create(cn);
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_valid_query: function() {
      let cmd = "/_api/query";
      let body = { "query" : `FOR u IN ${cn} FILTER u.name == @name LIMIT 2 RETURN u.n` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['bindVars'], ["name"]);
    },

    test_invalid_query: function() {
      let cmd = "/_api/query";
      let body = { "query" : `FOR u IN ${cn} FILTER u.name = @name LIMIT 2 RETURN u.n` };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_PARSE.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// floating point values;
////////////////////////////////////////////////////////////////////////////////;

function dealing_with_cursorsSuite_fetching_floating_point_valuesSuite () {
  let cn = "users";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);
      cid = db._create(cn);

      db[cn].save([
        { "_key" : "big", "value" : 4e+262 },
        { "_key" : "neg", "value" : -4e262 },
        { "_key" : "pos", "value" : 4e262 },
        { "_key" : "small", "value" : 4e-262 }
      ]);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_fetching_via_cursor: function() {
      let cmd = api;
      let body = { "query" : `FOR u IN ${cn} SORT u._key RETURN u.value`};
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertEqual(doc.parsedBody['id'], undefined);
      let result = doc.parsedBody['result'];
      assertEqual(result.length, 4);
      assertEqual(result[0], 4e262);
      assertEqual(result[1], -4e262);
      assertEqual(result[2], 4e262);
      assertEqual(result[3], 4e-262);

      doc = arango.GET_RAW(`/_api/document/${cn}/big`);
      assertEqual(doc.parsedBody['value'], 4e262);

      doc = arango.GET_RAW(`/_api/document/${cn}/neg`);
      assertEqual(doc.parsedBody['value'], -4e262);

      doc = arango.GET_RAW(`/_api/document/${cn}/pos`);
      assertEqual(doc.parsedBody['value'], 4e262);

      doc = arango.GET_RAW(`/_api/document/${cn}/small`);
      assertEqual(doc.parsedBody['value'], 4e-262);
    }
  };
}

jsunity.run(dealing_with_cursorsSuite_error_handlingSuite);
jsunity.run(dealing_with_cursorsSuite_handling_a_cursor_with_continuationSuite);
jsunity.run(dealing_with_cursorsSuite_handling_a_cursorSuite);
jsunity.run(dealing_with_cursorsSuite_checking_a_querySuite);
jsunity.run(dealing_with_cursorsSuite_fetching_floating_point_valuesSuite);
return jsunity.done();
