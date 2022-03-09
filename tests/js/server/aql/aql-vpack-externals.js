/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for VelocyPack Externals
////////////////////////////////////////////////////////////////////////////////

function aqlVPackExternalsTestSuite () {

  const collName = "UnitTestsVPackExternals";
  const edgeColl = "UnitTestsVPackEdges";

  return {

    setUpAll: function () {
      db._drop(collName);
      db._drop(edgeColl);
      let coll = db._create(collName);
      let docs = [];
      for (let i = 1000; i < 5000; ++i) {
        docs.push({_key: "test" + i, value: "test" + i});
      }
      coll.insert(docs);

      let ecoll = db._createEdgeCollection(edgeColl);
      docs = [];
      for(let i = 1001; i < 3000; ++i) {
        docs.push({_from: collName + "/test1000", _to: collName + "/test" + i});
      }
      ecoll.insert(docs);
    },

    tearDownAll: function () {
      db._drop(collName);
      db._drop(edgeColl);
    },
    
    testCustom: function () {
      const query = `FOR x IN ${collName} FILTER x IN [${JSON.stringify(db[collName].any())}] RETURN x`;
      const cursor = db._query(query);
      assertTrue(cursor.hasNext());
    },
    
    testCustomSubquery: function () {
      const query = `FOR x IN ${collName} FILTER x IN (FOR doc IN ${collName} LIMIT 1 RETURN doc) RETURN x`;
      const cursor = db._query(query);
      assertTrue(cursor.hasNext());
    },
    
    testPlainExternal: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN x`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n._key, "test" + i);
      }
    },

    testExternalInArray: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [x, x, x]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[0]._key, "test" + i);
        assertEqual(n[1]._key, "test" + i);
        assertEqual(n[2]._key, "test" + i);
      }
    },

    testExternalInMixedArray: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [5, x, x._key]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[1]._key, "test" + i);
      }
    },

    testExternalInObject: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN {doc: x}`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n.doc._key, "test" + i);
      }
    },

    testExternalNested: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN [5, {doc: [x]}]`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n[1].doc[0]._key, "test" + i);
      }
    },

    testExternalInMerge: function () {
      const query = `FOR x IN ${collName} SORT x._key RETURN MERGE({value2: 5}, x)`;
      const cursor = db._query(query);
      for (let i = 1000; i < 5000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n._key, "test" + i);
        assertEqual(n.value2, 5);
      }
    },

    testExternalInNeighbors: function () {
      const query = `WITH ${collName} FOR n IN OUTBOUND "${collName}/test1000" ${edgeColl} OPTIONS {bfs: true, uniqueVertices: "global"} SORT n._key RETURN n`;
      const cursor = db._query(query);
      for (let i = 1001; i < 3000; ++i) {
        assertTrue(cursor.hasNext());
        let n = cursor.next();
        assertEqual(n._key, "test" + i);
      }
    },

    testExternalInTraversalMerge: function () {
      const query = `WITH ${collName} LET s = (FOR n IN OUTBOUND "${collName}/test1000" ${edgeColl} RETURN n) RETURN MERGE(s)`;
      const cursor = db._query(query);
      const doc = cursor.next();
      assertTrue(doc.hasOwnProperty('_key'));
      assertTrue(doc.hasOwnProperty('_rev'));
      assertTrue(doc.hasOwnProperty('_id'));
      assertFalse(cursor.hasNext());
    }
  };
}


function aqlVPackExternalsModifyTestSuite () {

  const collName = "UnitTestsVPackExternals";
  const edgeColl = "UnitTestsVPackEdges";

  return {

    setUp: function () {
      db._drop(collName);
      db._drop(edgeColl);
      let coll = db._create(collName);
      let docs = [];
      for (let i = 1000; i < 5000; ++i) {
        docs.push({_key: "test" + i, value: "test" + i});
      }
      coll.insert(docs);

      let ecoll = db._createEdgeCollection(edgeColl);
      docs = [];
      for(let i = 1001; i < 3000; ++i) {
        docs.push({_from: collName + "/test1000", _to: collName + "/test" + i});
      }
      ecoll.insert(docs);
    },

    tearDown: function () {
      db._drop(collName);
      db._drop(edgeColl);
    },

    testCustomSubquery2: function () {
      db[collName].insert({ value: db[collName].any() });
      const query = `FOR x IN ${collName} FILTER x.value IN (FOR doc IN ${collName} RETURN doc) RETURN x`;
      const cursor = db._query(query);
      assertTrue(cursor.hasNext());
    },

    testExternalAttributeAccess: function () {
      let coll = db._collection(collName);
      let ecoll = db._collection(edgeColl);
      coll.truncate({ compact: false });
      ecoll.truncate({ compact: false });
      coll.insert({ _key: "a", w: 1});
      coll.insert({ _key: "b", w: 2});
      coll.insert({ _key: "c", w: 3});
      ecoll.insert({ _key: "a", _from: coll.name() + "/a", _to: coll.name() + "/b", w: 1});
      ecoll.insert({ _key: "b", _from: coll.name() + "/b", _to: coll.name() + "/c", w: 2});

      const query = `WITH ${collName} FOR x,y,p IN 1..10 OUTBOUND '${collName}/a' ${edgeColl} SORT x._key, y._key RETURN p.vertices[*].w`;
      const cursor = db._query(query);
     
      assertEqual([ 1, 2 ], cursor.next());
      assertEqual([ 1, 2, 3 ], cursor.next());
    },

    testIdAccessInMinMax: function () {
      let coll = db._collection(collName);
      let ecoll = db._collection(edgeColl);
      coll.truncate({ compact: false });
      ecoll.truncate({ compact: false });
      
      coll.insert({ _key: "a", w: 1});
      coll.insert({ _key: "b", w: 2});
      coll.insert({ _key: "c", w: 3});
      ecoll.insert({ _from: coll.name() + "/a", _to: coll.name() + "/b", w: 1});
      ecoll.insert({ _from: coll.name() + "/b", _to: coll.name() + "/c", w: 2});
      ecoll.insert({ _from: coll.name() + "/a", _to: coll.name() + "/a", w: 3});

      const query = `WITH ${collName} FOR x IN ANY '${collName}/a' ${edgeColl} COLLECT ct = x.w >= 1 INTO g RETURN MAX(g)`;
      const cursor = db._query(query);
      var doc = cursor.next();
      delete doc.x._rev;
      assertEqual({ "x" : { "_key" : "b", "_id" : collName + "/b", "w" : 2 } }, doc);
    }, 

    testExternalAttributeAccess2: function () {
      let coll = db._collection(collName);
      let ecoll = db._collection(edgeColl);
      coll.truncate({ compact: false });
      ecoll.truncate({ compact: false });

      for (var i = 0; i < 100; ++i) {
        coll.insert({ _key: "test" + i, username: "test" + i });
        ecoll.insert({ _from: collName + "/test" + i, _to: collName + "/test" + (i + 1), _key: "test" + i });
      }

      const query = `LET us = (FOR u1 IN ${collName} FILTER u1.username == "test1" FOR u2 IN ${collName} FILTER u2.username == "test2" RETURN { u1, u2 }) FOR u IN us FOR msg IN ${edgeColl} FILTER msg._from == u.u1._id && msg._to == u.u2._id RETURN msg._id`; 
      const result = db._query(query).toArray();
      assertEqual(edgeColl + "/test1", result[0]);
    }
  };
}
  jsunity.run(aqlVPackExternalsTestSuite);
  jsunity.run(aqlVPackExternalsModifyTestSuite);
return jsunity.done();
