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
  let cn = "UnitTestsCollectionEdge";
  return {
    setUp: function() {
      db._createEdgeCollection(cn);
    },
    tearDown: function() {
      db._drop(cn);
    },
    
    test_returns_an_error_if_vertex_identifier_is_missing: function() {
      let cmd = `/_api/edges/${cn}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 400);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], 1205);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_vertex_identifier_is_empty: function() {
      let cmd = `/_api/edges/${cn}?vertex=`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 400);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], 1205);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_collection_does_not_exist: function() {
      let cmd = `/_api/edges/${cn}OTHERNAME?vertex=test`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], 1203);
      assertEqual(doc.parsedBody['code'], 404);
      assertEqual(doc.headers['content-type'], contentType);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection name;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_nameSuite () {
  let ce = "UnitTestsCollectionEdge";
  let cv = "UnitTestsCollectionVertex";
  let reFull = new RegExp('^[a-zA-Z0-9_\-]*/[0-9]+$');
  let reEdge = new RegExp('^' + ce + '/');
  let reVertex = new RegExp('^' + cv + '/');
  return {
    setUp: function() {
      let eid = db._createEdgeCollection(ce, { waitForSync: true });
      let vid = db._create(cv, { waitForSync: true });
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
      body = {"_from":id1,"_to":id2};
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
      body = { "e" : 1, "_from" : id1, "_to": id2 };
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

      // check ANY edges;
      cmd = `/_api/edges/${ce}?vertex=${id1}`;
      doc = arango.GET_RAW(cmd);;

      assertEqual(doc.code, 200);
      assertTrue(Array.isArray(doc.parsedBody['edges']));
      assertEqual(doc.parsedBody['edges'].length, 3);

      // check IN edges;
      cmd = `/_api/edges/${ce}?vertex=${id1}&direction=in`;
      doc = arango.GET_RAW(cmd);;

      assertEqual(doc.code, 200);
      assertTrue(Array.isArray(doc.parsedBody['edges']));
      assertEqual(doc.parsedBody['edges'].length, 1);

      // check OUT edges;
      cmd = `/_api/edges/${ce}?vertex=${id1}&direction=out`;
      doc = arango.GET_RAW(cmd);;

      assertEqual(doc.code, 200);
      assertTrue(Array.isArray(doc.parsedBody['edges']));
      assertEqual(doc.parsedBody['edges'].length, 2);
    }
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(known_collection_nameSuite);
return jsunity.done();
