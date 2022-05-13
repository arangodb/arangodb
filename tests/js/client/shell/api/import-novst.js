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
const jsunity = require("jsunity");
const api = "/_api/import";
function import__testing_createCollectionSuite () {
  let cn = "UnitTestsImport";
  return {
    setUp: function() {
      db._drop(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_createCollectionEQfalse: function() {
      let cmd = api + `?collection=${cn}&createCollection=false&type=array`;
      let body =  "[ { \"foo\" : true } ]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// import a JSON array of documents ;
////////////////////////////////////////////////////////////////////////////////;
function import_self_contained_documentsSuite () {
  let cn = "UnitTestsImport";
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_JSONA_using_different_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=array`;
      let body =  "[ \n";
      body += "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] },\n";
      body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" },\n";
      body += "{ \"sample\" : \"garbage\" },\n";
      body += "{ }\n";
      body += "]\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 4);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_using_different_documents__typeEQauto: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=auto`;
      let body =  "[ \n";
      body += "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] },\n";
      body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" },\n";
      body += "{ \"sample\" : \"garbage\" },\n";
      body += "{ }\n";
      body += "]\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 4);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_using_whitespace: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=array`;
      let body =  " [\n\n      { \"name\" : \"John\", \"age\" : 29 },      \n     \n \n \r\n \n { \"rubbish\" : \"data goes in here\" }\n\n ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_invalid_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=array`;
      let body =  "[ { \"this doc\" : \"isValid\" },\n{ \"this one\" : is not },\n{ \"again\" : \"this is ok\" },\n\n{ \"but this isn't\" }\n ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_JSONA_empty_body: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=array`;
      let body =  "" ;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_JSONA_no_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=array`;
      let body =  "[\n\n]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_no_collection: function() {
      let cmd = api + "?type=array";
      let body =  "[ \n\n ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_COLLECTION_PARAMETER_MISSING.code);
    },

    test_JSONA_non_existing_collection: function() {
      db._drop(cn);

      let cmd = api + `?collection=${cn}&type=array`;
      let body = "[ { \"sample\" : \"garbage\" } ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////;
// import self-contained documents (one doc per line);
////////////////////////////////////////////////////////////////////////////////;
function import_self_contained_documentsSuite2 () {
  let cn = "UnitTestsImport";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_JSONL_using_different_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=documents`;
      let body =  "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] }\n";
      body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" }\n";
      body += "{ \"sample\" : \"garbage\" }\n";
      body += "{ }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 4);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_using_different_documents__typeEQauto: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=auto`;
      let body =  "{ \"name\" : \"John\", \"age\" : 29, \"gender\" : \"male\", \"likes\" : [ \"socket\", \"bind\", \"accept\" ] }\n";
      body += "{ \"firstName\" : \"Jane\", \"lastName\" : \"Doe\", \"age\" : 35, \"gender\" : \"female\", \"livesIn\" : \"Manila\" }\n";
      body += "{ \"sample\" : \"garbage\" }\n";
      body += "{ }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 4);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_using_whitespace: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=documents`;
      let body =  "\n\n      { \"name\" : \"John\", \"age\" : 29 }      \n     \n \n \r\n \n { \"rubbish\" : \"data goes in here\" }\n\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 7);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_invalid_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=documents`;
      let body =  "{ \"this doc\" : \"isValid\" }\n{ \"this one\" : is not }\n{ \"again\" : \"this is ok\" }\n\n{ \"but this isn't\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 2);
      assertEqual(doc.parsedBody['empty'], 1);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_empty_body: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=documents`;
      let body =  "" ;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_no_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true&type=documents`;
      let body =  "\n\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 2);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONL_no_collection: function() {
      let cmd = api + "?type=documents";
      let body =  "\n\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_COLLECTION_PARAMETER_MISSING.code);
    },

    test_JSONL_non_existing_collection: function() {
      db._drop(cn);

      let cmd = api + `?collection=${cn}&type=documents`;
      let body = "{ \"sample\" : \"garbage\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////;
// import attribute names and data;
////////////////////////////////////////////////////////////////////////////////;
function import_attribute_names_and_dataSuite () {
  let cn = "UnitTestsImport";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_regular: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "[ \"name\", \"age\", \"gender\" ]\n";
      body += "[ \"John\", 29, \"male\" ]\n";
      body += "[ \"Jane\", 35, \"female\" ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_using_whitespace: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "[ \"name\", \"age\", \"gender\" ]\n\n";
      body += "       [  \"John\", 29, \"male\" ]\n\n\r\n";
      body += "[ \"Jane\", 35, \"female\" ]\n\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 4);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_invalid_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "[ \"name\", \"age\", \"gender\" ]\n";
      body += "[ \"John\", 29, \"male\" ]\n";
      body += "[ \"Jane\" ]\n";
      body += "[ ]\n";
      body += "[ 1, 2, 3, 4 ]\n";
      body += "[ \"John\", 29, \"male\" ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 3);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_missing_header: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "\n[ \"name\", \"age\", \"gender\" ]\n";
      body += "[ \"John\", 29, \"male\" ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_wrong_header: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "{ \"name\" : \"John\", \"age\" : 29 }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_empty_body: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "" ;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_no_documents: function() {
      let cmd = api + `?collection=${cn}&createCollection=true`;
      let body =  "[ \"name\" ]\n\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 1);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_no_collection: function() {
      let cmd = api ;
      let body =  "[ \"name\" ]\n";
      body += "[ \"Jane\" ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_COLLECTION_PARAMETER_MISSING.code);
    },

    test_non_existing_collection: function() {
      db._drop(cn);

      let cmd = api + `?collection=${cn}`;
      let body =  "[ \"name\" ]\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// import documents into an edge collection, using a JSON array;
////////////////////////////////////////////////////////////////////////////////;
function import_into_an_edge_collection__using_a_JSON_arraySuite () {
  let vn = "UnitTestsImportVertex";
  let en = "UnitTestsImportEdge";
  return {
    setUp: function() {
      db._drop(vn);
      db._drop(en);

      db._create(vn);
      db._createEdgeCollection(en);

      let docs = [];
      for (let i = 0; i < 50; i++) {
        docs.push({ "_key" : `vertex${i}`, "value" : `${i}` });
      }
      db[vn].save(docs);
    },

    tearDown: function() {
      db._drop(vn);
      db._drop(en);
    },

    test_JSONA_no__from: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array`;
      let body = `[ { \"_to\" : \"${vn}/vertex1\" } ]`;;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_no__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array`;
      let body = `[ { \"_from\" : \"${vn}/vertex1\" } ]`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array`;
      let body = `[ { \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" } ]`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"from\" : \"${vn}/vertex1\", \"to\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_wrong_fromPrefix: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&fromPrefix=a+b`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_from\" : \"vertex1\", \"_to\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_wrong_toPrefix: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&toPrefix=a+b`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_fromPrefix: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&fromPrefix=${vn}`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_from\" : \"vertex1\", \"_to\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"vertex1\", \"_to\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"from\" : \"vertex1\", \"to\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_toPrefix: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&toPrefix=${vn}`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"vertex1\", \"_from\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"to\" : \"vertex1\", \"from\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_fromPrefix_and_toPrefix: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&fromPrefix=${vn}&toPrefix=${vn}`;
      let body = "[\n";
      body += "{ \"a\" : 1, \"_to\" : \"vertex1\", \"_from\" : \"vertex2\" },\n";
      body += "{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"vertex1\", \"_from\" : \"vertex2\" },\n";
      body += "{ \"to\" : \"vertex1\", \"from\" : \"vertex2\" }\n";
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_JSONA_multiple_docs__with_fromPrefix_and_toPrefix__double_prefixing: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=array&fromPrefix=${vn}&toPrefix=${vn}`;
      let body = "[\n";
      body += `{ \"a\" : 1, \"_to\" : \"${vn}/vertex1\", \"_from\" : \"${vn}/vertex2\" },\n`;
      body += `{ \"foo\" : true, \"bar\": \"baz\", \"_to\" : \"${vn}/vertex1\", \"_from\" : \"${vn}vertex2\" },\n`;
      body += `{ \"to\" : \"${vn}/vertex1\", \"from\" : \"${vn}/vertex2\" }\n`;
      body += "]";;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// import documents into an edge collection, using individual documents;
////////////////////////////////////////////////////////////////////////////////;
function import_into_an_edge_collection__using_individual_docsSuite () {
  let vn = "UnitTestsImportVertex";
  let en = "UnitTestsImportEdge";
  return {
    setUp: function() {
      db._drop(vn);
      db._drop(en);

      db._create(vn);
      db._createEdgeCollection(en);

      let docs = [];
      for (let i=0; i < 50; i++) {
        docs.push({ "_key" : `vertex${i}`, "value" : `${i}`});
      }
      db[vn].save(docs);
    },

    tearDown: function() {
      db._drop(vn);
      db._drop(en);
    },

    test_no__from: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=documents`;
      let body = `{ \"_to\" : \"${vn}/vertex1\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_no__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=documents`;
      let body = `{ \"_from\" : \"${vn}/vertex1\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=documents`;
      let body = `{ \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_multiple_documents__with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false&type=documents`;
      let body = `{ \"a\" : 1, \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" }\n`;
      body += `{ \"foo\" : true, \"bar\": \"baz\", \"_from\" : \"${vn}/vertex1\", \"_to\" : \"${vn}/vertex2\" }\n`;
      body += `{ \"from\" : \"${vn}/vertex1\", \"to\" : \"${vn}/vertex2\" }\n`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////;
// import documents into an edge collection, using CSV;
////////////////////////////////////////////////////////////////////////////////;
function import_into_an_edge_collection__using_CSVSuite () {
  let vn = "UnitTestsImportVertex";
  let en = "UnitTestsImportEdge";
  return {
    setUp: function() {
      db._drop(vn);
      db._drop(en);

      db._create(vn);
      db._createEdgeCollection(en);

      let docs = [];
      for (let i=0; i < 50; i++) {
        docs.push({ "_key" : `vertex${i}`, "value" : `${i}`});
      }
      db[vn].save(docs);
    },

    tearDown: function() {
      db._drop(vn);
      db._drop(en);
    },

    test_CSV_no__from: function() {
      let cmd = api + `?collection=${en}&createCollection=false`;
      let body = "[ \"_to\" ]\n";
      body += `[ \"${vn}/vertex1\" ]`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_CSV_no__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false`;
      let body = "[ \"_from\" ]\n";
      body += `[ \"${vn}/vertex1\" ]`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 0);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_CSV_with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false`;
      let body = "[ \"_from\", \"_to\" ]\n";
      body += `[ \"${vn}/vertex1\", \"${vn}/vertex2\" ]`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

    test_CSV_multiple_documents__with__from_and__to: function() {
      let cmd = api + `?collection=${en}&createCollection=false`;
      let body = "[ \"a\", \"_from\", \"_to\", \"foo\" ]\n";
      body += `[ 1, \"${vn}/vertex1\", \"${vn}/vertex2\", \"\" ]\n`;
      body += "\n";
      body += `[ 2, \"${vn}/vertex3\", \"${vn}/vertex4\", \"\" ]\n`;
      body += "[ 2, 1, 2, 3 ]";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 1);
      assertEqual(doc.parsedBody['updated'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
    },

  };
}
  

////////////////////////////////////////////////////////////////////////////////;
// import with update;
////////////////////////////////////////////////////////////////////////////////;
function import_with_updateSuite () {
  let cn = "UnitTestsImport";
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn);

      let cmd = api + `?collection=${cn}&type=documents`;
      let body =  "{ \"_key\" : \"test1\", \"value1\" : 1, \"value2\" : \"test\" }\n";
      body += "{ \"_key\" : \"test2\", \"value1\" : \"abc\", \"value2\" : 3 }\n";
      arango.POST_RAW(cmd, body);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_using_onDuplicateEQerror: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=error`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"_key\" : \"test2\" }\n";
      body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 2);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test');
      assertFalse(doc.parsedBody.hasOwnProperty('value3'));

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertEqual(doc.parsedBody['value1'], 'abc');
      assertEqual(doc.parsedBody['value2'], 3);

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['foo'], 'bar');
    },

    test_using_onDuplicateEQerror__without__key: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=error`;
      let body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 3);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
    },

    test_using_onDuplicateEQerror__mixed__keys: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=error`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 1);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test');
      assertFalse(doc.parsedBody.hasOwnProperty('value3'));
    },

    test_using_onDuplicateEQupdate: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=update`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"_key\" : \"test2\" }\n";
      body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 2);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test-updated');
      assertEqual(doc.parsedBody['value3'], 3);

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertEqual(doc.parsedBody['value1'], 'abc');
      assertEqual(doc.parsedBody['value2'], 3);

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['foo'], 'bar');
    },

    test_using_onDuplicateEQupdate__without__key: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=update`;
      let body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 3);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
    },

    test_using_onDuplicateEQupdate__mixed__keys: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=update`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test-updated');
      assertEqual(doc.parsedBody['value3'], 3);
    },

    test_using_onDuplicateEQreplace: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=replace`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"_key\" : \"test2\" }\n";
      body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 2);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertFalse(doc.parsedBody.hasOwnProperty('value1'));
      assertEqual(doc.parsedBody['value2'], 'test-updated');
      assertEqual(doc.parsedBody['value3'], 3);

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertFalse(doc.parsedBody.hasOwnProperty('value1'));
      assertFalse(doc.parsedBody.hasOwnProperty('value2'));

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['foo'], 'bar');
    },

    test_using_onDuplicateEQreplace__without__key: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=replace`;
      let body =  "{ \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 3);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
    },

    test_using_onDuplicateEQreplace__mixed__keys: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=replace`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test-updated\" }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertFalse(doc.parsedBody.hasOwnProperty('value1'));
      assertEqual(doc.parsedBody['value2'], 'test-updated');
      assertEqual(doc.parsedBody['value3'], 3);
    },

    test_using_onDuplicateEQignore: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=ignore`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test\" }\n";
      body += "{ \"_key\" : \"test2\" }\n";
      body += "{ \"_key\" : \"test3\", \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 2);
      assertEqual(doc.parsedBody['updated'], 0);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test');
      assertFalse(doc.parsedBody.hasOwnProperty('value3'));

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertEqual(doc.parsedBody['value1'], 'abc');
      assertEqual(doc.parsedBody['value2'], 3);

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['foo'], 'bar');
    },

    test_using_onDuplicateEQignore__without__key: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=ignore`;
      let body =  "{ \"value3\" : 3, \"value2\" : \"test\" }\n";
      body += "{ }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 3);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);
    },

    test_using_onDuplicateEQignore__mixed__keys: function() {
      let cmd = api + `?collection=${cn}&type=documents&onDuplicate=ignore`;
      let body =  "{ \"_key\" : \"test1\", \"value3\" : 3, \"value2\" : \"test\" }\n";
      body += "{ \"foo\" : \"bar\" }\n";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 1);
      assertEqual(doc.parsedBody['updated'], 0);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);
      assertEqual(doc.parsedBody['value2'], 'test');
      assertFalse(doc.parsedBody.hasOwnProperty('value3'));
    },

    test_using_onDuplicateEQupdate__values: function() {
      let cmd = api + `?collection=${cn}&onDuplicate=update`;
      let body =  "[\"_key\",\"value\"]\n" ;
      body += "[\"test1\",1]\n";
      body += "[\"test1\",2]\n" ;
      body += "[\"test2\",23]\n" ;
      body += "[\"test2\",42]\n" ;
      body += "[\"test3\",99]\n" ;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 1);
      assertEqual(doc.parsedBody['errors'], 0);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 4);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value'], 2);

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertEqual(doc.parsedBody['value'], 42);

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['value'], 99);
    },

    test_using_onDuplicateEQerror__values: function() {
      let cmd = api + `?collection=${cn}&onDuplicate=error`;
      let body = "[\"_key\",\"value1\"]\n" ;
      body += "[\"test1\",17]\n";
      body += "[\"test2\",23]\n" ;
      body += "[\"test3\",99]\n" ;
      body += "[\"test4\",\"abc\"]\n" ;
      body += "[\"test4\",\"def\"]\n" ;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['created'], 2);
      assertEqual(doc.parsedBody['errors'], 3);
      assertEqual(doc.parsedBody['empty'], 0);
      assertEqual(doc.parsedBody['ignored'], 0);
      assertEqual(doc.parsedBody['updated'], 0);

      doc = arango.GET_RAW(`/_api/document/${cn}/test1`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test1');
      assertEqual(doc.parsedBody['value1'], 1);

      doc = arango.GET_RAW(`/_api/document/${cn}/test2`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test2');
      assertEqual(doc.parsedBody['value1'], "abc");

      doc = arango.GET_RAW(`/_api/document/${cn}/test3`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test3');
      assertEqual(doc.parsedBody['value1'], 99);

      doc = arango.GET_RAW(`/_api/document/${cn}/test4`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_key'], 'test4');
      assertEqual(doc.parsedBody['value1'], "abc");
    },
  };
}

jsunity.run(import__testing_createCollectionSuite);
jsunity.run(import_self_contained_documentsSuite);
jsunity.run(import_self_contained_documentsSuite2);
jsunity.run(import_attribute_names_and_dataSuite);
jsunity.run(import_into_an_edge_collection__using_a_JSON_arraySuite);
jsunity.run(import_into_an_edge_collection__using_individual_docsSuite);
jsunity.run(import_into_an_edge_collection__using_CSVSuite);
jsunity.run(import_with_updateSuite);
return jsunity.done();
