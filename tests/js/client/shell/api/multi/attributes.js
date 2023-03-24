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

let api = "/_api/document";

function dealing_with_attribute_nameSuite () {
  let cn = "UnitTestsCollectionAttributes";
  return {

    setUp: function() {
      db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // creates a document with an empty attribute ;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_an_empty_attribute_name: function() {
      let cmd = api + "?collection=" + cn;
      let body = { "" : "a", "foo" : "b" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      let id = doc.parsedBody['_id'];

      cmd = api + "/" + id;
      doc = arango.GET_RAW(cmd);

      assertTrue(doc.parsedBody.hasOwnProperty(('')));
      assertTrue(doc.parsedBody.hasOwnProperty(('foo')));
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // queries a document with an empty attribute ;
    ////////////////////////////////////////////////////////////////////////////////;

    test_queries_a_document_with_an_empty_attribute_name: function() {
      let cmd = api + "?collection=" + cn;
      let body = { "" : "a", "foo" : "b" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      cmd = "/_api/simple/all";
      body = { "collection" : cn };
      doc = arango.PUT_RAW(cmd, body);

      let documents = doc.parsedBody['result'];

      assertEqual(documents.length, 1);
      assertTrue(documents[0].hasOwnProperty(('')));
      assertTrue(documents[0].hasOwnProperty(('foo')));
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // creates a document with reserved attribute names;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_reserved_attribute_names: function() {
      let cmd = api + "?collection=" + cn;
      let body = { "_rev" : "99", "foo" : "002", "_id" : "meow", "_from" : "a", "_to" : "b", "_test" : "c", "meow" : "d" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let id = doc.parsedBody['_id'];

      cmd = api + "/" + id;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.parsedBody['_id'], id);
      assertNotEqual(doc.parsedBody['_rev'], '99');
      assertFalse(doc.parsedBody.hasOwnProperty('_from'));
      assertFalse(doc.parsedBody.hasOwnProperty('_to'));
      assertTrue(doc.parsedBody.hasOwnProperty(('_test')));
      assertEqual(doc.parsedBody['_test'], 'c');
      assertTrue(doc.parsedBody.hasOwnProperty(('meow')));
      assertEqual(doc.parsedBody['meow'], 'd');
      assertEqual(doc.parsedBody['foo'], '002');
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // nested attribute names;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_nested_attribute_names: function() {
      let cmd = api + "?collection=" + cn;
      let body = { "a" : "1", "b" : { "b" : "2" , "a" : "3", "": "4", "_key": "moetoer", "_from": "5", "_lol" : false, "c" : 6 } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      let id = doc.parsedBody['_id'];

      cmd = api + "/" + id;
      doc = arango.GET_RAW(cmd);

      assertTrue(doc.parsedBody.hasOwnProperty(('a')));
      assertEqual(doc.parsedBody['a'], '1');
      assertTrue(doc.parsedBody.hasOwnProperty(('b')));

      assertTrue(doc.parsedBody['b'].hasOwnProperty(('')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('_from')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('_key')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('_lol')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('b')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('a')));
      assertTrue(doc.parsedBody['b'].hasOwnProperty(('c')));
      assertEqual(doc.parsedBody['b'], { "": "4", "b" : "2", "a" : "3", "_key" : "moetoer", "_from" : "5", "_lol" : false, "c" : 6 });
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // duplicate attribute names;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_duplicate_attribute_names: function() {
      let cmd = api + "?collection=" + cn;
      let body = '{ "a" : "1", "b" : "2", "a" : "3" }';
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // nested duplicate attribute names;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creates_a_document_with_nested_duplicate_attribute_names: function() {
      let cmd = api + "?collection=" + cn;
      let body = '{ "a" : "1", "b" : { "b" : "2" , "c" : "3", "b": "4" } }';
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
    }
  };
}
jsunity.run(dealing_with_attribute_nameSuite);
return jsunity.done();
