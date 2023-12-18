/*jshint esnext: true */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief Spec for the AQL FOR x IN GRAPH name statement
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

"use strict";

const jsunity = require("jsunity");

const internal = require("internal");
const db = internal.db;
const errors = require("@arangodb").errors;
const gm = require("@arangodb/general-graph");
const vn = "UnitTestVertexCollection";
const en = "UnitTestEdgeCollection";
const isCluster = require("internal").isCluster();
var _ = require("lodash");
var vertex = {};
var edge = {};
var vc;
var ec;

var cleanup = function () {
  db._drop(vn);
  db._drop(en);
  vertex = {};
  edge = {};
};

var createBaseGraph = function () {
  vc = db._create(vn, {numberOfShards: 4});
  ec = db._createEdgeCollection(en, {numberOfShards: 4});
  vertex.A = vc.save({_key: "A"})._id;
  vertex.B = vc.save({_key: "B"})._id;
  vertex.C = vc.save({_key: "C"})._id;
  vertex.D = vc.save({_key: "D"})._id;
  vertex.E = vc.save({_key: "E"})._id;
  vertex.F = vc.save({_key: "F"})._id;
  
  edge.AB = ec.save(vertex.A, vertex.B, {})._id;
  edge.BC = ec.save(vertex.B, vertex.C, {})._id;
  edge.CD = ec.save(vertex.C, vertex.D, {})._id;
  edge.CF = ec.save(vertex.C, vertex.F, {})._id;
  edge.EB = ec.save(vertex.E, vertex.B, {})._id;
  edge.FE = ec.save(vertex.F, vertex.E, {})._id;
};

function multiCollectionGraphSuite () {

  /***********************************************************************
   * Graph under test:
   *
   *  A -> B -> C -> D <-E2- V2:G
   *      /|\  \|/   
   *       E <- F
   *
   *
   *
   ***********************************************************************/
  
  var g;
  const gn = "UnitTestGraph";
  const vn2 = "UnitTestVertexCollection2";
  const en2 = "UnitTestEdgeCollection2";
  let ruleName = "optimize-traversals";
  let paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] }, 
                        skipInaccessibleCollections: true };
  let opts = _.clone(paramEnabled);

  // We always use the same query, the result should be identical.
  let validateResult = function (result) {
    assertEqual(result.length, 1);
    let entry = result[0];
    assertEqual(entry.vertex._id, vertex.C);
    assertEqual(entry.path.vertices.length, 2);
    assertEqual(entry.path.vertices[0]._id, vertex.B);
    assertEqual(entry.path.vertices[1]._id, vertex.C);
    assertEqual(entry.path.edges.length, 1);
    assertEqual(entry.path.edges[0]._id, edge.BC);
  };

  return {

    setUpAll: function() {
      opts.allPlans = true;
      opts.verbosePlans = true;
      cleanup();
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      db._drop(vn2);
      db._drop(en2);
      createBaseGraph();
      g = gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn2, vn)]);
      db[vn2].save({_key: "G"});
      db[en2].save(vn2 + "/G", vn + "/D", {});
    },

    tearDownAll: function() {
      gm._drop(gn);
      db._drop(vn2);
      db._drop(en2);
      cleanup();
    },

    testOtherCollectionAttributeAccessInput: function () {
      var query = `WITH ${vn}
        FOR y IN @@vCol
        FOR x IN OUTBOUND y._id @@eCol
        SORT x._id ASC
        RETURN x._id`;
      var bindVars = {
        "@eCol": en,
        "@vCol": vn
      };
      var result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 6);
      assertEqual(result[0], vertex.B);
      assertEqual(result[1], vertex.B);
      assertEqual(result[2], vertex.C);
      assertEqual(result[3], vertex.D);
      assertEqual(result[4], vertex.E);
      assertEqual(result[5], vertex.F);
    },

    testTraversalAttributeAccessInput: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        FOR y IN OUTBOUND x._id @@eCol
        SORT y._id ASC
        RETURN y._id`;
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testTraversalLetIdInput: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        LET next = x._id
        FOR y IN OUTBOUND next @@eCol
        SORT y._id ASC
        RETURN y._id`;
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testTraversalLetDocInput: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        LET next = x
        FOR y IN OUTBOUND next @@eCol
        SORT y._id ASC
        RETURN y._id`;
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    }

  };
}

function potentialErrorsSuite () {
  var vc, ec;

  return {

    setUpAll: function () {
      cleanup();
      vc = db._create(vn);
      ec = db._createEdgeCollection(en);
      vertex.A = vn + "/unknown";

      vertex.B = vc.save({_key: "B"})._id;
      vertex.C = vc.save({_key: "C"})._id;
      ec.save(vertex.B, vertex.C, {});
    },

    tearDownAll: cleanup,

    testNonIntegerSteps: function () {
      var query = "FOR x IN 2.5 OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNonNumberSteps: function () {
      var query = "FOR x IN 'invalid' OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testMultiDirections: function () {
      var query = "FOR x IN OUTBOUND ANY @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNoCollections: function () {
      var query = "FOR x IN OUTBOUND @startId RETURN x";
      var bindVars = {
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testNoStartVertex: function () {
      var query = "FOR x IN OUTBOUND @@eCol RETURN x";
      var bindVars = {
        "@eCol": en
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testTooManyOutputParameters: function () {
      var query = "FOR x, y, z, f IN OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testTraverseVertexCollection: function () {
      var query = `FOR x IN OUTBOUND @startId @@eCol, @@vCol RETURN x`;
      var bindVars = {
        "@eCol": en,
        "@vCol": vn,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail(query + " should not be allowed");
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
      }
    },

    testStartWithSubquery: function () {
      var query = `FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x`;
      var bindVars = {
        "@eCol": en,
        "@vCol": vn
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testStepsSubquery: function() {
      var query = `WITH ${vn}
        FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart1: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND null @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart2: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND 1 @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
      };
      try {
        db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart3: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND [] @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart4: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND 'foobar' @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart5: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND {foo: 'bar'} @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart6: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND {_id: @startId} @@eCol
        RETURN x._id`;
      var bindVars = {
        "startId": vertex.B,
        "@eCol": en
      };
      var result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart7: function () {
      var query = `FOR x IN OUTBOUND
        (FOR y IN @@vCol FILTER y._id == @startId RETURN y) @@eCol
        RETURN x._id`;
      var bindVars = {
        "startId": vertex.B,
        "@eCol": en,
        "@vCol": vn
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Fix the query, just use the first value
      query = `WITH ${vn}
        FOR x IN OUTBOUND
        (FOR y IN @@vCol FILTER y._id == @startId RETURN y)[0] @@eCol
        RETURN x._id`;
      result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart8: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND
        (FOR y IN @@eCol FILTER y._id == @startId RETURN 'peter') @@eCol
        RETURN x._id`;
      var bindVars = {
        "startId": vertex.A,
        "@eCol": en
      };
      var x = db._query(query, bindVars, {skipInaccessibleCollections: true});
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Actually use the string!
      query = `WITH ${vn}
        FOR x IN OUTBOUND
        (FOR y IN @@eCol FILTER y._id == @startId RETURN 'peter')[0] @@eCol
        RETURN x._id`;
      result = db._query(query, bindVars, {skipInaccessibleCollections: true}).toArray();
      assertEqual(result.length, 0);
    }

  };
}

function optionsSuite() {
  const gn = "UnitTestGraph";

  return {

    setUp: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDown: function () {
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      cleanup();
    },

    testEdgeUniquenessPath: function () {
      var start = vc.save({_key: "s"})._id;
      var a = vc.save({_key: "a"})._id;
      var b = vc.save({_key: "b"})._id;
      var c = vc.save({_key: "c"})._id;
      var d = vc.save({_key: "d"})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
         FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueEdges: "path"}
         SORT v._key
         RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
      // We expect to get s->a->b->c->a->d
      // and s->a->d
      // But not s->a->b->c->a->b->*
      // And not to continue at a again
      assertEqual(cursor.length, 6);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, b); // We once find b
      assertEqual(cursor[3]._id, c); // And once c
      assertEqual(cursor[4]._id, d); // We once find d on short path
      assertEqual(cursor[5]._id, d); // And find d on long path
    },

    testEdgeUniquenessGlobal: function () {

      var start = vc.save({_key: "s"})._id;
      try {
        var cursor = db._query(
          `WITH ${vn}
           FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueEdges: "global"}
           SORT v._key
           RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_BAD_PARAMETER.code, "We expect a bad parameter");
      }
    },

    testEdgeUniquenessNone: function () {
      var start = vc.save({_key: "s"})._id;
      var a = vc.save({_key: "a"})._id;
      var b = vc.save({_key: "b"})._id;
      var c = vc.save({_key: "c"})._id;
      var d = vc.save({_key: "d"})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
         FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueEdges: "none"}
         SORT v._key
         RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
      // We expect to get s->a->d
      // We expect to get s->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->b->c->a
      assertEqual(cursor.length, 13);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, a); // We somehow return to a again
      assertEqual(cursor[3]._id, a); // We somehow return to a again
      assertEqual(cursor[4]._id, b); // We once find b
      assertEqual(cursor[5]._id, b); // We find b again
      assertEqual(cursor[6]._id, b); // And b again
      assertEqual(cursor[7]._id, c); // And once c
      assertEqual(cursor[8]._id, c); // We find c again
      assertEqual(cursor[9]._id, c); // And c again
      assertEqual(cursor[10]._id, d); // Short Path d
      assertEqual(cursor[11]._id, d); // One Loop d
      assertEqual(cursor[12]._id, d); // Second Loop d
    },

    testVertexUniquenessNone: function () {
      var start = vc.save({_key: "s"})._id;
      var a = vc.save({_key: "a"})._id;
      var b = vc.save({_key: "b"})._id;
      var c = vc.save({_key: "c"})._id;
      var d = vc.save({_key: "d"})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
         FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueVertices: "none"}
         SORT v._key
         RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
      // We expect to get s->a->b->c->a->d
      // and s->a->d
      // But not s->a->b->c->a->b->*
      // And not to continue at a again

      // Default edge Uniqueness is path
      assertEqual(cursor.length, 6);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[2]._id, b); // We once find b
      assertEqual(cursor[3]._id, c); // And once c
      assertEqual(cursor[4]._id, d); // We once find d on short path
      assertEqual(cursor[5]._id, d); // And find d on long path
    },

    testVertexUniquenessGlobalDepthFirst: function () {
      var start = vc.save({_key: "s"})._id;
      try {
        var cursor = db._query(
          `WITH ${vn}
           FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueVertices: "global"}
           SORT v._key
           RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_BAD_PARAMETER.code, "We expect a bad parameter");
      }
    },

    testVertexUniquenessPath: function () {
      var start = vc.save({_key: "s"})._id;
      var a = vc.save({_key: "a"})._id;
      var b = vc.save({_key: "b"})._id;
      var c = vc.save({_key: "c"})._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(a, a, {});
      ec.save(b, c, {});
      ec.save(b, a, {});
      ec.save(c, a, {});
      var cursor = db._query(
        `WITH ${vn}
         FOR v IN 1..10 OUTBOUND "${start}" ${en} OPTIONS {uniqueVertices: "path"}
         SORT v._key
         RETURN v`, {}, {skipInaccessibleCollections: true}).toArray();
      // We expect to get s->a->b->c
      // But not s->a->a*
      // But not s->a->b->a*
      // But not s->a->b->c->a*
      assertEqual(cursor.length, 3);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[1]._id, b); // We find a->b
      assertEqual(cursor[2]._id, c); // We find a->b->c
    },


  };
}

function optimizeQuantifierSuite() {
  /********************************
   * Graph under test
   * C <-+             +-> F
   *     |             |
   *     +-B<---A--->E-+
   *     |             |
   * D <-+             +-> G
   *
   *
   * Left side has foo: true , right foo: false
   * Top has bar: true, bottom bar: false.
   * A,B,E has bar: true
   * A has foo: true
   * Edges have foo and bar like their target
   *******************************/


  const gn = "UnitTestGraph";
  let vertices = {};
  let edges = {};

  return {
    setUpAll: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertices.A = vc.save({_key: "A", foo: true, bar: true})._id;
      vertices.B = vc.save({_key: "B", foo: true, bar: true})._id;
      vertices.C = vc.save({_key: "C", foo: true, bar: true})._id;
      vertices.D = vc.save({_key: "D", foo: true, bar: false})._id;
      vertices.E = vc.save({_key: "E", foo: false, bar: true})._id;
      vertices.F = vc.save({_key: "F", foo: false, bar: true})._id;
      vertices.G = vc.save({_key: "G", foo: false, bar: false})._id;

      edges.AB = ec.save({_key: "AB", _from: vertices.A, _to: vertices.B, foo: true, bar: true})._id;
      edges.BC = ec.save({_key: "BC", _from: vertices.B, _to: vertices.C, foo: true, bar: true})._id;
      edges.BD = ec.save({_key: "BD", _from: vertices.B, _to: vertices.D, foo: true, bar: false})._id;
      edges.AE = ec.save({_key: "AE", _from: vertices.A, _to: vertices.E, foo: false, bar: true})._id;
      edges.EF = ec.save({_key: "EF", _from: vertices.E, _to: vertices.F, foo: false, bar: true})._id;
      edges.EG = ec.save({_key: "EG", _from: vertices.E, _to: vertices.G, foo: false, bar: false})._id;

      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDownAll: function () {
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      cleanup();
    },

    testAllVerticesSingle: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo ALL == true
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 5);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
      }
      assertEqual(stats.filtered, 1);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo ALL == false
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query);
      assertEqual(cursor.count(), 0);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      assertEqual(stats.scannedIndex, 1);
      assertEqual(stats.filtered, 1);
    },

    testAllEdgesSingle: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == true
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 4);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // TODO Check for Optimization
        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
      }
      assertTrue(stats.filtered <= 2);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == false
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 4);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
      }
      assertTrue(stats.filtered <= 2);
    },

    testNoneVerticesSingle: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo NONE == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 5);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
      }
      assertEqual(stats.filtered, 1);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo NONE == true
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query);
      assertEqual(cursor.count(), 0);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      assertEqual(stats.scannedIndex, 1);
      assertEqual(stats.filtered, 1);
    },

    testNoneEdgesSingle: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo NONE == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 4);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
      }
      assertEqual(stats.filtered, 1);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo NONE == true
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 4);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // TODO Check for Optimization
        assertTrue(stats.scannedIndex <= 17);
      }
      assertEqual(stats.filtered, 1);
    },

    testAllVerticesMultiple: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo ALL == true
        FILTER p.vertices[*].bar ALL == true
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 5);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
      }
      assertEqual(stats.filtered, 2);
    },

    testAllEdgesMultiple: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == true
        FILTER p.edges[*].bar ALL == true
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 3);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
      }
      assertTrue(stats.filtered <= 3);
    },

    testAllNoneVerticesMultiple: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo ALL == true
        FILTER p.vertices[*].bar NONE == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 5);
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
      }
      assertEqual(stats.filtered, 2);
    },

    testAllNoneEdgesMultiple: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == true
        FILTER p.edges[*].bar NONE == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 3);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
      }
      assertTrue(stats.filtered <= 3);
    },

    testAllVerticesDepth: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.vertices[*].foo ALL == true
        FILTER p.vertices[2].bar == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 5);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
      }
      assertTrue(stats.filtered <= 4);
    },

    testAllEdgesAndDepth: function () {
      let query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == true
        FILTER p.edges[1].bar == false
        SORT v._key
        RETURN v._id
      `;
      let cursor = db._query(query, {}, {skipInaccessibleCollections: true});
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        assertTrue(stats.scannedIndex <= 3);
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
      }
      assertTrue(stats.filtered <= 4);
    }
  };
};

function exampleGraphsSuite () {
  let ex = require('@arangodb/graph-examples/example-graph');

  return {
    setUpAll: () => {
      ex.dropGraph('traversalGraph');
      ex.loadGraph('traversalGraph');
    },

    tearDownAll: () => {
      ex.dropGraph('traversalGraph');
    },

    testMinDepthFilterNEQ: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label != 'left_blub'
                 RETURN v._key`;
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());

      q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER 'left_blub' != p.edges[1].label
             RETURN v._key`;
      res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
    },

    testMinDepthFilterEq: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label == null
                 RETURN v._key`;
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 1);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B'].sort());

      q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER null == p.edges[1].label
             RETURN v._key`;
      res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 1);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B'].sort());
 
    },

    testMinDepthFilterIn: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label IN [null, 'left_blarg', 'foo', 'bar', 'foxx']
                 RETURN v._key`;
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
    },

    testMinDepthFilterLess: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label < 'left_blub'
                 RETURN v._key`;
      let res = db._query(q);
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());

      q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER 'left_blub' > p.edges[1].label
             RETURN v._key`;
      res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
 
    },

    testMinDepthFilterNIN: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label NOT IN ['left_blub', 'foo', 'bar', 'foxx']
                 RETURN v._key`;
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
    },

    testMinDepthFilterComplexNode: () => {
      let q = `LET condition = { value: 'left_blub' }
               FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label != condition.value
                 RETURN v._key`;
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());

      q = `LET condition = { value: 'left_blub' }
           FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER condition.value != p.edges[1].label
             RETURN v._key`;
      res = db._query(q);
      assertEqual(res.count(), 3);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
 
    },

    testMinDepthFilterReference: () => {
      let q = `FOR snippet IN ['right']
        LET test = CONCAT(snippet, '_blob')
          FOR v, e, p IN 1..2 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
            FILTER p.edges[1].label != test 
            RETURN v._key`;
      
      let res = db._query(q, {}, {skipInaccessibleCollections: true});
      assertEqual(res.count(), 5);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ["B", "C", "E", "G", "J"].sort());
    }

  };
};

jsunity.run(multiCollectionGraphSuite);
jsunity.run(potentialErrorsSuite);
jsunity.run(optionsSuite);
jsunity.run(exampleGraphsSuite);

return jsunity.done();
