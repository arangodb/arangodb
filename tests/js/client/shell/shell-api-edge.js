/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertMatch */

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
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  return {
    test_returns_an_error_if_collection_does_not_exist: function() {
      let cn = "UnitTestsCollectionEdge";
      db._drop(cn);
      let cmd = `/_api/document?collection=${cn}`;
      let body = {"_from":"test","_to":"test"};
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection name;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_nameSuite1 () {
  let ce = "UnitTestsCollectionEdge";
  let cv = "UnitTestsCollectionVertex";
  let reFull = new RegExp('^[a-zA-Z0-9_-]+/[0-9]+$');
  let reEdge = new RegExp('^' + ce + '/');
  let reVertex = new RegExp('^' + cv + '/');
  let vid;
  let eid;
  return {
    setUp: function() {
      eid = db._createEdgeCollection(ce, { waitForSync: true });
      vid = db._create(cv, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(ce);
      db._drop(cv);
    },

    test_creating_an_edge: function() {
      let cmd = `/_api/document?collection=${cv}`;

      // create first vertex;
      let body = { "a" : 1 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reVertex, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id1 = doc.parsedBody['_id'];

      // create second vertex;
      body = { "a" : 2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reVertex, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id2 = doc.parsedBody['_id'];

      // create edge;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : id1 , "_to" : id2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id3 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id3}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_id'], id3);
      assertEqual(doc.parsedBody['_from'], id1);
      assertEqual(doc.parsedBody['_to'], id2);
      assertEqual(doc.headers['content-type'], contentType);

      // create another edge;
      cmd = `/_api/document?collection=${ce}`;
      body = { "e" : 1, "_from" : id1, "_to" : id2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id4 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id4}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_id'], id4);
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.parsedBody['_from'], id1);
      assertMatch(reFull, doc.parsedBody['_from']);
      assertMatch(reVertex, doc.parsedBody['_from']);
      assertEqual(doc.parsedBody['_to'], id2);
      assertMatch(reFull, doc.parsedBody['_to']);
      assertMatch(reVertex, doc.parsedBody['_to']);
      assertEqual(doc.parsedBody['e'], 1);
      assertEqual(doc.headers['content-type'], contentType);

      // create third edge;
      cmd = `/_api/document?collection=${ce}`;
      body = { "e" : 2, "_from" : id2, "_to" : id1 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id5 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id5}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_id'], id5);
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.parsedBody['_from'], id2);
      assertMatch(reFull, doc.parsedBody['_from']);
      assertMatch(reVertex, doc.parsedBody['_from']);
      assertEqual(doc.parsedBody['_to'], id1);
      assertMatch(reFull, doc.parsedBody['_to']);
      assertMatch(reVertex, doc.parsedBody['_to']);
      assertEqual(doc.parsedBody['e'], 2);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_using_collection_names_in_from_and_to: function() {
      let cmd = `/_api/document?collection=${cv}`;

      // create a vertex;
      let body = { "a" : 1 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reVertex, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id = doc.parsedBody['_id'];
      // replace collection id with collection name;
      let translated = id.replace(/^[0-9]+/, 'UnitTestsCollectionVertex/');

      // create edge, using collection id;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : id, "_to" : id };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      // create edge, using collection names;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : translated, "_to" : translated };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      // create edge, using mix of collection names and ids;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : translated, "_to" : id };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      // turn parameters around;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : id, "_to" : translated };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_using_collection_ids_in_collection__from_and_to: function() {
      let cmd = `/_api/document?collection=${vid._id}`;

      // create a vertex;
      let body = { "a" : 1 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reVertex, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);

      let id = doc.parsedBody['_id'];
      // replace collection name with collection id;
      let translated = id.replace(/UnitTestsCollectionVertex/, vid._id);

      // create edge, using collection id;
      cmd = `/_api/document?collection=${eid._id}`;
      body = { "_from" : translated, "_to" : translated };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertMatch(reFull, doc.parsedBody['_id']);
      assertMatch(reEdge, doc.parsedBody['_id']);
      assertEqual(doc.headers['content-type'], contentType);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // create an edge using keys;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creating_an_edge_using_keys: function() {
      let cmd = `/_api/document?collection=${cv}`;

      // create first vertex;
      let body = { "name" : "Fred", "_key" : "fred" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.parsedBody['_id'], `${cv}/fred`);
      assertEqual(typeof doc.parsedBody['_key'], 'string');
      assertEqual(doc.parsedBody['_key'], "fred");
      assertEqual(doc.headers['content-type'], contentType);

      let id1 = doc.parsedBody['_id'];

      // create second vertex;
      body = { "name" : "John", "_key" : "john" };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.parsedBody['_id'], `${cv}/john`);
      assertEqual(typeof doc.parsedBody['_key'], 'string');
      assertEqual(doc.parsedBody['_key'], "john");
      assertEqual(doc.headers['content-type'], contentType);

      let id2 = doc.parsedBody['_id'];

      // create edge;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : id1, "_to" : id2, "what" : "fred->john", "_key" : "edge1" };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.parsedBody['_id'], `${ce}/edge1`);
      assertEqual(typeof doc.parsedBody['_key'], 'string');
      assertEqual(doc.parsedBody['_key'], "edge1");
      assertEqual(doc.headers['content-type'], contentType);

      let id3 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id3}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.parsedBody['_id'], id3);
      assertEqual(typeof doc.parsedBody['_key'], 'string');
      assertEqual(doc.parsedBody['_key'], "edge1");
      assertEqual(doc.parsedBody['_from'], id1);
      assertEqual(doc.parsedBody['_to'], id2);
      assertEqual(doc.parsedBody['what'], "fred->john");
      assertEqual(doc.headers['content-type'], contentType);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // create using invalid keys;
    ////////////////////////////////////////////////////////////////////////////////;

    test_creating_an_edge_using_invalid_keys: function() {
      let cmd = `/_api/document?collection=${cv}`;

      // create first vertex;
      let body = { "name" : "Fred", "_key" : "fred" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let id1 = doc.parsedBody['_id'];

      // create second vertex;
      body = { "name" : "John", "_key" : "john" };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let id2 = doc.parsedBody['_id'];

      // create edge, 1st try;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : `${cv}/rak/ov`, "_to" : `${cv}/pj/otr` };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      // create edge, 2nd try;
      cmd = `/_api/document?collection=${ce}`;
      body = { "_from" : `${cv}/rakov`, "_to" : `${cv}/pjotr`, "_key" : "the fox" };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection name, waitForSync URL param;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_nameSuite2 () {
  let ce = "UnitTestsCollectionEdge";
  let cv = "UnitTestsCollectionVertex";
  return {
    setUp: function() {
      db._createEdgeCollection(ce);
      db._create(cv, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(ce);
      db._drop(cv);
    },

    test_creating_an_edge__waitForSync_URL_paramEQfalse: function() {
      // create first vertex;
      let cmd = `/_api/document?collection=${cv}`;
      let body = { "a" : 1 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id1 = doc.parsedBody['_id'];

      // create second vertex;
      body = { "a" : 2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id2 = doc.parsedBody['_id'];

      // create edge;
      cmd = `/_api/document?collection=${ce}&waitForSync=false`;
      body = { "_from" : id1, "_to" : id2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 202);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id3 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id3}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_id'], id3);
      assertEqual(doc.parsedBody['_from'], id1);
      assertEqual(doc.parsedBody['_to'], id2);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_creating_an_edge__waitForSync_URL_paramEQtrue: function() {
      // create first vertex;
      let cmd = `/_api/document?collection=${cv}`;
      let body = { "a" : 1 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id1 = doc.parsedBody['_id'];

      // create second vertex;
      body = { "a" : 2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id2 = doc.parsedBody['_id'];

      // create edge;
      cmd = `/_api/document?collection=${ce}&waitForSync=true`;
      body = { "_from" : id1, "_to" : id2 };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(typeof doc.parsedBody['_id'], 'string');
      assertEqual(doc.headers['content-type'], contentType);

      let id3 = doc.parsedBody['_id'];

      // check edge;
      cmd = `/_api/document/${id3}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['_id'], id3);
      assertEqual(doc.parsedBody['_from'], id1);
      assertEqual(doc.parsedBody['_to'], id2);
      assertEqual(doc.headers['content-type'], contentType);
    }
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(known_collection_nameSuite1);
jsunity.run(known_collection_nameSuite2);
return jsunity.done();
