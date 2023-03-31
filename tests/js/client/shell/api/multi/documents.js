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

let api = "/_api/document";

function dealing_with_documentSuite () {
  let cn = "UnitTestsCollectionDocuments";
  let cid;
  return {

    setUp: function() {
      db._drop(cn);
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // creates documents with invalid types;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_an_invalid_type_1: function() {
      let cmd = api + "?collection=" + cn;
      let body = "[ [] ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody[0]["error"]);
      assertEqual(doc.parsedBody[0]["errorNum"], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    test_creates_a_document_with_an_invalid_type_2: function() {
      let cmd = api + "?collection=" + cn;
      let body = '"test"';
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // updates documents with invalid types;
    ////////////////////////////////////////////////////////////////////////////////;

    test_updates_a_document_with_an_invalid_type_1: function() {
      let cmd = api + `/${cn}/test`;
      let body = "[ ]";
      let doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    test_updates_a_document_with_an_invalid_type_2: function() {
      let cmd = api + `/${cn}/test`;
      let body = '"test"';
      let doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // replaces documents with invalid types;
    ////////////////////////////////////////////////////////////////////////////////;

    test_replaces_a_document_with_an_invalid_type_1: function() {
      let cmd = api + `/${cn}/test`;
      let body = "[ ]";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    test_replaces_a_document_with_an_invalid_type_2: function() {
      let cmd = api + `/${cn}/test`;
      let body = '"test"';
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // updates documents by example with invalid type;
    ////////////////////////////////////////////////////////////////////////////////;

    test_updates_documents_by_example_with_an_invalid_type_1: function() {
      let cmd = "/_api/simple/update-by-example";
      let body = { "collection" : cn, "example" : [ ], "newValue" : { } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_updates_documents_by_example_with_an_invalid_type_2: function() {
      let cmd = "/_api/simple/update-by-example";
      let body = { "collection" : cn, "example" : { }, "newValue" : [ ] };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // replaces documents by example with invalid type;
    ////////////////////////////////////////////////////////////////////////////////;

    test_replaces_documents_by_example_with_an_invalid_type_1: function() {
      let cmd = "/_api/simple/replace-by-example";
      let body = { "collection" : cn, "example" : [ ], "newValue" : { } };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_replaces_documents_by_example_with_an_invalid_type_2: function() {
      let cmd = "/_api/simple/replace-by-example";
      let body = { "collection" : cn, "example" : { }, "newValue" : [ ] };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // removes documents by example with invalid type;
    ////////////////////////////////////////////////////////////////////////////////;

    test_removes_a_document_with_an_invalid_type: function() {
      let cmd = "/_api/simple/remove-by-example";
      let body = { "collection" : cn, "example" : [ ] };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    }

  };
}
jsunity.run(dealing_with_documentSuite);
return jsunity.done();
