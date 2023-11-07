/* jshint esnext: true */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Michael Hackstein
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;


const internal = require('internal');
const db = require('internal').db;

const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';

const gh = require('@arangodb/graph/helpers');

function subQuerySuite() {
  const gn = 'UnitTestGraph';
  var vc, ec;
  var vertex = {};
  var edge = {};
  return {

    /*
     * Graph under Test:
     *
     * A -> B -> [B1, B2, B3, B4, B5]
     *   \> C -> [C1, C2, C3, C4, C5]
     *   \> D -> [D1, D2, D3, D4, D5]
     *
     */
    setUpAll: function () {
      gh.cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);

      vertex.A = vc.save({_key: 'A'})._id;
      vertex.B = vc.save({_key: 'B'})._id;
      vertex.C = vc.save({_key: 'C'})._id;
      vertex.D = vc.save({_key: 'D'})._id;

      vertex.B1 = vc.save({_key: 'B1', value: 1})._id;
      vertex.B2 = vc.save({_key: 'B2', value: 2})._id;
      vertex.B3 = vc.save({_key: 'B3', value: 3})._id;
      vertex.B4 = vc.save({_key: 'B4', value: 4})._id;
      vertex.B5 = vc.save({_key: 'B5', value: 5})._id;

      vertex.C1 = vc.save({_key: 'C1', value: 1})._id;
      vertex.C2 = vc.save({_key: 'C2', value: 2})._id;
      vertex.C3 = vc.save({_key: 'C3', value: 3})._id;
      vertex.C4 = vc.save({_key: 'C4', value: 4})._id;
      vertex.C5 = vc.save({_key: 'C5', value: 5})._id;

      vertex.D1 = vc.save({_key: 'D1', value: 1})._id;
      vertex.D2 = vc.save({_key: 'D2', value: 2})._id;
      vertex.D3 = vc.save({_key: 'D3', value: 3})._id;
      vertex.D4 = vc.save({_key: 'D4', value: 4})._id;
      vertex.D5 = vc.save({_key: 'D5', value: 5})._id;

      ec.save(vertex.A, vertex.B, {});
      ec.save(vertex.A, vertex.C, {});
      ec.save(vertex.A, vertex.D, {});

      ec.save(vertex.B, vertex.B1, {});
      ec.save(vertex.B, vertex.B2, {});
      ec.save(vertex.B, vertex.B3, {});
      ec.save(vertex.B, vertex.B4, {});
      ec.save(vertex.B, vertex.B5, {});

      ec.save(vertex.C, vertex.C1, {});
      ec.save(vertex.C, vertex.C2, {});
      ec.save(vertex.C, vertex.C3, {});
      ec.save(vertex.C, vertex.C4, {});
      ec.save(vertex.C, vertex.C5, {});

      ec.save(vertex.D, vertex.D1, {});
      ec.save(vertex.D, vertex.D2, {});
      ec.save(vertex.D, vertex.D3, {});
      ec.save(vertex.D, vertex.D4, {});
      ec.save(vertex.D, vertex.D5, {});
    },

    tearDownAll: function () {
      try {
        gm._drop(gn);
      } catch (e) {
        // Just in case the test leaves an invalid state.
      }
      gh.cleanup();
    },

    testUniqueVertexGetterMemoryIssue: function () {
      // test case: UniqueVertexGetter keeps track of which vertices
      // were already visited. the vertex id tracked pointed to memory
      // which was allocated only temporarily and that became invalid
      // once TraverserCache::clear() got called.
      let query = `WITH ${vn} FOR i IN 1..3 FOR v, e, p IN 1..3 OUTBOUND '${vertex.A}' ${en} OPTIONS { uniqueVertices: 'global', bfs: true } SORT v._key RETURN v._key`;
      let result = db._query(query).toArray();
      assertEqual([ 
        "B", "B", "B", 
        "B1", "B1", "B1", 
        "B2", "B2", "B2", 
        "B3", "B3", "B3", 
        "B4", "B4", "B4", 
        "B5", "B5", "B5", 
        "C", "C", "C", 
        "C1", "C1", "C1", 
        "C2", "C2", "C2", 
        "C3", "C3", "C3", 
        "C4", "C4", "C4", 
        "C5", "C5", "C5", 
        "D", "D", "D", 
        "D1", "D1", "D1", 
        "D2", "D2", "D2", 
        "D3", "D3", "D3", 
        "D4", "D4", "D4", 
        "D5", "D5", "D5" 
      ], result);
      
      // while we are here, try a different query as well
      query = `WITH ${vn} FOR i IN 1..3 LET sub = (FOR v, e, p IN 1..3 OUTBOUND '${vertex.A}' ${en} OPTIONS { uniqueVertices: 'global', bfs: true } SORT v._key RETURN v._key) RETURN sub`;
      result = db._query(query).toArray();
      assertEqual([ 
        [ "B", "B1", "B2", "B3", "B4", "B5", "C", "C1", "C2", "C3", "C4", "C5", "D", "D1", "D2", "D3", "D4", "D5" ], 
        [ "B", "B1", "B2", "B3", "B4", "B5", "C", "C1", "C2", "C3", "C4", "C5", "D", "D1", "D2", "D3", "D4", "D5" ], 
        [ "B", "B1", "B2", "B3", "B4", "B5", "C", "C1", "C2", "C3", "C4", "C5", "D", "D1", "D2", "D3", "D4", "D5" ] 
      ], result);
    },

    // The test is that the traversal in subquery has more then LIMIT many
    // results. In case of a bug the cursor of the traversal is reused for the second
    // iteration as well and does not reset.
    testSubQueryFixedStart: function () {
      var q = `WITH ${vn}
      FOR v IN OUTBOUND '${vertex.A}' ${en}
      SORT v._key
      LET sub = (
        FOR t IN OUTBOUND '${vertex.B}' ${en}
        SORT t.value
        LIMIT 3
        RETURN t
      )
      RETURN sub`;
      var actual = db._query(q).toArray();
      assertEqual(actual.length, 3); // On the top level we find 3 results
      for (var i = 0; i < actual.length; ++i) {
        var current = actual[i];
        assertEqual(current.length, 3); // Check for limit
        // All should be connected to B!
        assertEqual(current[0]._id, vertex.B1);
        assertEqual(current[1]._id, vertex.B2);
        assertEqual(current[2]._id, vertex.B3);
      }
    },

    // The test is that the traversal in subquery has more then LIMIT many
    // results. In case of a bug the cursor of the traversal is reused for the second
    // iteration as well and does not reset.
    testSubQueryDynamicStart: function () {
      var q = `WITH ${vn}
      FOR v IN OUTBOUND '${vertex.A}' ${en}
      SORT v._key
      LET sub = (
        FOR t IN OUTBOUND v ${en}
        SORT t.value
        LIMIT 3
        RETURN t
      )
      RETURN sub`;
      var actual = db._query(q).toArray();
      assertEqual(actual.length, 3); // On the top level we find 3 results
      for (var i = 0; i < actual.length; ++i) {
        assertEqual(actual[i].length, 3); // Check for limit
      }
      var current = actual[0];
      // All should be connected to B!
      assertEqual(current[0]._id, vertex.B1);
      assertEqual(current[1]._id, vertex.B2);
      assertEqual(current[2]._id, vertex.B3);

      current = actual[1];
      // All should be connected to C!
      assertEqual(current[0]._id, vertex.C1);
      assertEqual(current[1]._id, vertex.C2);
      assertEqual(current[2]._id, vertex.C3);

      current = actual[2];
      // All should be connected to D!
      assertEqual(current[0]._id, vertex.D1);
      assertEqual(current[1]._id, vertex.D2);
      assertEqual(current[2]._id, vertex.D3);
    }
  };
}

jsunity.run(subQuerySuite);
return jsunity.done();
