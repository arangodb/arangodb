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

let api = "/_api/explain";

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  return {
    test_returns_an_error_if_body_is_missing: function() {
      let cmd = api;
      let doc =  arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_returns_an_error_for_an_invalid_path: function() {
      let cmd = api + "/foo";
      let body = { "query" : "RETURN 1" };
      let doc =  arango.POST_RAW(cmd, body);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_returns_an_error_if_collection_is_unknown: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN unknowncollection LIMIT 2 RETURN u.n" };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_returns_an_error_if_bind_variables_are_missing_completely: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN [1,2] FILTER u.id == @id RETURN 1" };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
    },

    test_returns_an_error_if_bind_variables_are_required_but_empty: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN [1,2] FILTER u.id == @id RETURN 1", "bindVars" : { } };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
    },

    test_returns_an_error_if_bind_variables_are_missing: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN [1,2] FILTER u.id == @id RETURN 1", "bindVars" : { "id2" : 1 } };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
    },

    test_returns_an_error_if_query_contains_a_parse_error: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN " };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_PARSE.code);
    }
  };
}
////////////////////////////////////////////////////////////////////////////////;
// explaining;
////////////////////////////////////////////////////////////////////////////////;

function explaining_queriesSuite () {
  return {
    test_explains_a_simple_query: function() {
      let cmd = api;
      let body = { "query" : "FOR u IN [1,2] RETURN u" };
      let doc =  arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
    }
  };
}
jsunity.run(error_handlingSuite);
jsunity.run(explaining_queriesSuite);
return jsunity.done();
