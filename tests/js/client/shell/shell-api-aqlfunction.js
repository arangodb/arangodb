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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");

let api = "/_api/aqlfunction";


////////////////////////////////////////////////////////////////////////////////;
// error handling ;
////////////////////////////////////////////////////////////////////////////////;
function error_handlinSuite () {
  return {
    test_add_function__without_name: function() {
      let body = "{ \"code\" : \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code);
    },

    test_add_function__invalid_name_1: function() {
      let body = "{ \"name\" : \"\", \"code\" : \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code);
    },

    test_add_function__invalid_name_2: function() {
      let body = "{ \"name\" : \"_aql::foobar\", \"code\" : \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code);
    },

    test_add_function__invalid_name_3: function() {
      let body = "{ \"name\" : \"foobar\", \"code\" : \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code);
    },

    test_add_function__no_code: function() {
      let body = "{ \"name\" : \"myfunc::mytest\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code);
    },

    test_add_function__invalid_code: function() {
      let body = "{ \"name\" : \"myfunc::mytest\", \"code\" : \"function ()\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code);
    },

    test_deleting_non_existing_function: function() {
      let doc = arango.DELETE_RAW(api + "/mytest%3A%3Amynonfunc");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// adding and deleting functions;
////////////////////////////////////////////////////////////////////////////////;
function adding_and_deleting_functionSuite () {
  return {
    setUp: function() {
      arango.DELETE("/_api/aqlfunction/UnitTests%3A%3Amytest");
    },

    tearDown: function() {
      arango.DELETE("/_api/aqlfunction/UnitTests%3A%3Amytest");
    },

    test_add_function__valid_code: function() {
      let body = "{ \"name\" : \"UnitTests::mytest\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);
    },

    test_add_function__update: function() {
      let body = "{ \"name\" : \"UnitTests::mytest\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_add_function__delete: function() {
      let body = "{ \"name\" : \"UnitTests::mytest\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Amytest");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Amytest");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code);
    },

    test_add_function__delete_multiple: function() {
      let body = "{ \"name\" : \"UnitTests::mytest::one\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      body = "{ \"name\" : \"UnitTests::mytest::two\", \"code\": \"function () { return 1; }\" }";
      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      body = "{ \"name\" : \"UnitTests::foo\", \"code\": \"function () { return 1; }\" }";
      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Amytest?group=true");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Amytest%3A%3Aone");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Amytest%3A%3Atwo");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      doc = arango.DELETE_RAW(api + "/UnitTests%3A%3Afoo");
      assertEqual(doc.code, 200);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// retrieving the list of functions;
////////////////////////////////////////////////////////////////////////////////;
function retrieving_functionSuite () {
  return {
    setUp: function() {
      arango.DELETE("/_api/aqlfunction/UnitTests?group=true");
    },

    tearDown: function() {
      arango.DELETE("/_api/aqlfunction/UnitTests?group=true");
    },

    test_add_function_and_retrieve_the_list: function() {
      let body = "{ \"name\" : \"UnitTests::mytest\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);

      doc = arango.GET_RAW(api + "?prefix=UnitTests");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['result'][0]['name'], "UnitTests::mytest");
      assertEqual(doc.parsedBody['result'][0]['code'], "function () { return 1; }");
    },

    test_add_functions_and_retrieve_the_list: function() {
      let body = "{ \"name\" : \"UnitTests::mytest1\", \"code\": \"function () { return 1; }\" }";
      let doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 201);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      doc = arango.GET_RAW(api + "?prefix=UnitTests");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['result'][0]['name'], "UnitTests::mytest1");
      assertEqual(doc.parsedBody['result'][0]['code'], "function () { return 1; }");

      body = "{ \"name\" : \"UnitTests::mytest1\", \"code\": \"( function () { return   3 * 5; } ) \" }";
      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['isNewlyCreated']);

      doc = arango.GET_RAW(api + "?prefix=UnitTests");
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['result'].length, 1);
      assertEqual(doc.parsedBody['result'][0]['name'], "UnitTests::mytest1");
      assertEqual(doc.parsedBody['result'][0]['code'], "( function () { return   3 * 5; } )");
    }
  };
}

jsunity.run(error_handlinSuite);
jsunity.run(adding_and_deleting_functionSuite);
jsunity.run(retrieving_functionSuite);
return jsunity.done();
