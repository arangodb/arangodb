/*jshint esnext: true */
/*global assertEqual, fail, AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

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
const isCluster = require("@arangodb/cluster").isCluster();
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

function namedGraphSuite () {

  /***********************************************************************
   * Graph under test:
   *
   *  A -> B  ->  C -> D
   *      /|\    \|/
   *       E  <-  F
   *
   *
   *
   *
   *
   *
   *
   *
   *
   *
   *
   *
   *
   ***********************************************************************/

  var g;
  const gn = "UnitTestGraph";
  var ruleName = "merge-traversal-filter";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

  return {

    setUp: function() {
      opts.allPlans = true;
      opts.verbosePlans = true;
      cleanup();
      createBaseGraph();
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      g = gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDown: function () {
      gm._drop(gn);
      cleanup();
    },

    testFirstEntryIsVertex: function () {
      var query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSecondEntryIsEdge: function () {
      var query = "FOR x, e IN OUTBOUND @startId GRAPH @graph RETURN e";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testThirdEntryIsPath: function () {
      var query = "FOR x, e, p IN OUTBOUND @startId GRAPH @graph RETURN p";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry.vertices.length, 2);
      assertEqual(entry.vertices[0]._id, vertex.B);
      assertEqual(entry.vertices[1]._id, vertex.C);
      assertEqual(entry.edges.length, 1);
      assertEqual(entry.edges[0]._id, edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testOutboundDirection: function () {
      var query = "FOR x IN OUTBOUND @startId GRAPH @graph RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testInboundDirection: function () {
      var query = "FOR x IN INBOUND @startId GRAPH @graph RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.C
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, vertex.B);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testAnyDirection: function () {
      var query = "FOR x IN ANY @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);
      var entry = result[0];
      assertEqual(entry, vertex.A);
      entry = result[1];
      assertEqual(entry, vertex.C);
      entry = result[2];
      assertEqual(entry, vertex.E);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testExactNumberSteps: function () {
      var query = "FOR x IN 2 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testRangeNumberSteps: function () {
      var query = "FOR x IN 2..3 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);

      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.E);
      assertEqual(result[2], vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testComputedNumberSteps: function () {
      var query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], vertex.D);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSort: function () {
      var query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.C
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.F);

      // Reverse ordering
      query = "FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id DESC RETURN x._id";

      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.F);
      assertEqual(result[1], vertex.D);

      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testUniqueEdgesOnPath : function () {
      var query = "FOR x IN 6 OUTBOUND @startId GRAPH @graph RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.A
      };
      // No result A->B->C->F->E->B (->C) is already used!
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 0);

      query = "FOR x, e, p IN 2 ANY @startId GRAPH @graph SORT x._id ASC " + 
              "RETURN {v: x._id, edges: p.edges, vertices: p.vertices}";
      result = db._query(query, bindVars).toArray();

      // result: A->B->C
      // result: A->B<-E
      // Invalid result: A->B<-A
      assertEqual(result.length, 2);
      assertEqual(result[0].v, vertex.C);
      assertEqual(result[0].edges.length, 2);
      assertEqual(result[0].edges[0]._id, edge.AB);
      assertEqual(result[0].edges[1]._id, edge.BC);

      assertEqual(result[0].vertices.length, 3);
      assertEqual(result[0].vertices[0]._id, vertex.A);
      assertEqual(result[0].vertices[1]._id, vertex.B);
      assertEqual(result[0].vertices[2]._id, vertex.C);
      assertEqual(result[1].v, vertex.E);
      assertEqual(result[1].edges.length, 2);
      assertEqual(result[1].edges[0]._id, edge.AB);
      assertEqual(result[1].edges[1]._id, edge.EB);

      assertEqual(result[1].vertices.length, 3);
      assertEqual(result[1].vertices[0]._id, vertex.A);
      assertEqual(result[1].vertices[1]._id, vertex.B);
      assertEqual(result[1].vertices[2]._id, vertex.E);

      query = `FOR x IN 1 ANY @startId GRAPH @graph
               FOR y IN 1 ANY x GRAPH @graph
               SORT y._id ASC RETURN y._id`;
      result = db._query(query, bindVars).toArray();

      // result: A->B<-A
      // result: A->B->C
      // result: A->B<-E
      // The second traversal resets the path
      assertEqual(result.length, 3);
      assertEqual(result[0], vertex.A);
      assertEqual(result[1], vertex.C);
      assertEqual(result[2], vertex.E);
    }
  };
}

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
  var ruleName = "merge-traversal-filter";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

  // We always use the same query, the result should be identical.
  var validateResult = function (result) {
    assertEqual(result.length, 1);
    var entry = result[0];
    assertEqual(entry.vertex._id, vertex.C);
    assertEqual(entry.path.vertices.length, 2);
    assertEqual(entry.path.vertices[0]._id, vertex.B);
    assertEqual(entry.path.vertices[1]._id, vertex.C);
    assertEqual(entry.path.edges.length, 1);
    assertEqual(entry.path.edges[0]._id, edge.BC);
  };

  return {

    setUp: function() {
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

    tearDown: function() {
      gm._drop(gn);
      db._drop(vn2);
      db._drop(en2);
      cleanup();
    },

    testNoBindParameterDoubleFor: function () {
      /* this test is intended to trigger the clone functionality. */
      var query = "FOR t IN " + vn +
        " FOR s IN " + vn2 + 
        " FOR x, e, p IN OUTBOUND t " + en + " SORT x._key RETURN {vertex: x, path: p}";
      var result = db._query(query).toArray();
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNoBindParameterSingleFor: function () {
      var query = "FOR s IN " + vn + " FOR x, e, p IN OUTBOUND s " + en + " SORT x._key RETURN x";
      var result = db._query(query).toArray();
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNoBindParameterSingleForFilter: function () {
      var query = "FOR s IN " + vn + " FOR x, e, p IN OUTBOUND s " +
        en + " FILTER p.vertices[1]._key == s._key SORT x._key RETURN x";
      var result = db._query(query).toArray();
      assertEqual(result.length, 0);
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult.length, 0);
      });
    },

    testNoBindParameterV8Function: function () {
      var query = "FOR s IN " + vn + " FOR x, e, p IN OUTBOUND s " +
        en + " FILTER p.vertices[1]._key == NOOPT(V8(RAND())) SORT x._key RETURN x";
      var result = db._query(query).toArray();
      assertEqual(result.length, 0);
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult.length, 0);
      });
    },


    testNoBindParameter: function () {
      var query = "FOR x, e, p IN OUTBOUND '" + vertex.B + "' " + en + " SORT x._key RETURN {vertex: x, path: p}";
      var result = db._query(query).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testStartBindParameter: function () {
      var query = "FOR x, e, p IN OUTBOUND @startId " + en + " SORT x._key RETURN {vertex: x, path: p}";
      var bindVars = {
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testEdgeCollectionBindParameter: function () {
      var query = "FOR x, e, p IN OUTBOUND '" + vertex.B + "' @@eCol SORT x._key RETURN {vertex: x, path: p}";
      var bindVars = {
        "@eCol": en
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testStepsBindParameter: function () {
      var query = "FOR x, e, p IN @steps OUTBOUND '" + vertex.B + "' " + en + 
                  " SORT x._key RETURN {vertex: x, path: p}";
      var bindVars = {
        steps: 1
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testStepsRangeBindParameter: function () {
      var query = "FOR x, e, p IN @lsteps..@rsteps OUTBOUND '" + vertex.B
        + "' " + en + " SORT x._key RETURN {vertex: x, path: p}";
      var bindVars = {
        lsteps: 1,
        rsteps: 1
      };
      var result = db._query(query, bindVars).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testFirstEntryIsVertex: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol SORT x._key RETURN x";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSecondEntryIsEdge: function () {
      var query = "FOR x, e IN OUTBOUND @startId @@eCol SORT x._key RETURN e";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testThirdEntryIsPath: function () {
      var query = "FOR x, e, p IN OUTBOUND @startId @@eCol SORT x._key RETURN p";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry.vertices.length, 2);
      assertEqual(entry.vertices[0]._id, vertex.B);
      assertEqual(entry.vertices[1]._id, vertex.C);
      assertEqual(entry.edges.length, 1);
      assertEqual(entry.edges[0]._id, edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testOutboundDirection: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol SORT x._key RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testInboundDirection: function () {
      var query = "FOR x IN INBOUND @startId @@eCol SORT x._key RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.C
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, vertex.B);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testAnyDirection: function () {
      var query = "FOR x IN ANY @startId @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);
      var entry = result[0];
      assertEqual(entry, vertex.A);
      entry = result[1];
      assertEqual(entry, vertex.C);
      entry = result[2];
      assertEqual(entry, vertex.E);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testExactNumberSteps: function () {
      var query = "FOR x IN 2 OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testRangeNumberSteps: function () {
      var query = "FOR x IN 2..3 OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);

      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.E);
      assertEqual(result[2], vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testComputedNumberSteps: function () {
      var query = "FOR x IN LENGTH([1,2]) OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], vertex.D);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSort: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.C
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.F);

      // Reverse ordering
      query = "FOR x IN OUTBOUND @startId @@eCol SORT x._id DESC RETURN x._id";

      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.F);
      assertEqual(result[1], vertex.D);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSingleDocumentInput: function () {
      var query = "FOR y IN @@vCol FILTER y._id == @startId "
        + "FOR x IN OUTBOUND y @@eCol SORT x._key RETURN x";
      var bindVars = {
        startId: vertex.B,
        "@eCol": en,
        "@vCol": vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testListDocumentInput: function () {
      var query = "FOR y IN @@vCol "
        + "FOR x IN OUTBOUND y @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        "@vCol": vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 6);
      assertEqual(result[0], vertex.B);
      assertEqual(result[1], vertex.B);
      assertEqual(result[2], vertex.C);
      assertEqual(result[3], vertex.D);
      assertEqual(result[4], vertex.E);
      assertEqual(result[5], vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testOtherCollectionAttributeAccessInput: function () {
      var query = "FOR y IN @@vCol "
        + "FOR x IN OUTBOUND y._id @@eCol SORT x._id ASC RETURN x._id";
      var bindVars = {
        "@eCol": en,
        "@vCol": vn
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 6);
      assertEqual(result[0], vertex.B);
      assertEqual(result[1], vertex.B);
      assertEqual(result[2], vertex.C);
      assertEqual(result[3], vertex.D);
      assertEqual(result[4], vertex.E);
      assertEqual(result[5], vertex.F);
    },

    testTraversalAttributeAccessInput: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol "
                  + "FOR y IN OUTBOUND x._id @@eCol SORT y._id ASC RETURN y._id";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testTraversalLetIdInput: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol "
                  + "LET next = x._id "
                  + "FOR y IN OUTBOUND next @@eCol SORT y._id ASC RETURN y._id";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testTraversalLetDocInput: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol "
                  + "LET next = x "
                  + "FOR y IN OUTBOUND next @@eCol SORT y._id ASC RETURN y._id";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    }

  };
}




function multiEdgeCollectionGraphSuite () {

  /***********************************************************************
   * Graph under test:
   *
   *         B<----+       <- B & C via edge collection A
   *               |
   *         D<----A----<C
   *               |
   *               +----<E <- D & E via edge colltion B
   *
   ***********************************************************************/

  var g;
  const gn = "UnitTestGraph";
  const en2 = "UnitTestEdgeCollection2";
  var ruleName = "merge-traversal-filter";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

  return {

    setUp: function() {
      opts.allPlans = true;
      opts.verbosePlans = true;
      cleanup();
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      vc  = db._create(vn, {numberOfShards: 4});
      ec  = db._createEdgeCollection(en,  {numberOfShards: 4});
      var ec2 = db._createEdgeCollection(en2, {numberOfShards: 4});

      g = gm._create(gn, [gm._relation(en, vn, vn), gm._relation(en2, vn, vn)]);

      vertex.A = vc.save({_key: "A"})._id;
      vertex.B = vc.save({_key: "B"})._id;
      vertex.C = vc.save({_key: "C"})._id;
      vertex.D = vc.save({_key: "D"})._id;
      vertex.E = vc.save({_key: "E"})._id;

      edge.AB = ec.save(vertex.A, vertex.B, {})._id;
      edge.CA = ec.save(vertex.C, vertex.A, {})._id;
      edge.AD = ec2.save(vertex.A, vertex.D, {})._id;
      edge.EA = ec2.save(vertex.E, vertex.A, {})._id;
    },

    tearDown: function() {
      gm._drop(gn);
      db._drop(vn);
      db._drop(en);
      db._drop(en2);
      cleanup();
    },

    testTwoVertexCollectionsInOutbound: function () {
      /* this test is intended to trigger the clone functionality. */
      var expectResult = ['B', 'C', 'D', 'E'];
      var query = "FOR x IN ANY @startId GRAPH @graph SORT x._id RETURN x._key";
      var bindVars = {
        graph: gn,
        startId: vertex.A
      };

      var result = db._query(query, bindVars).toArray();

      assertEqual(result, expectResult, query);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    }

  };
}

function potentialErrorsSuite () {
  var vc, ec;

  return {

    setUp: function () {
      cleanup();
      vc = db._create(vn);
      ec = db._createEdgeCollection(en);
      vertex.A = vn + "/unknown";

      vertex.B = vc.save({_key: "B"})._id;
      vertex.C = vc.save({_key: "C"})._id;
      ec.save(vertex.B, vertex.C, {});
    },

    tearDown: cleanup,

    testNonIntegerSteps: function () {
      var query = "FOR x IN 2.5 OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
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
        db._query(query, bindVars).toArray();
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
        db._query(query, bindVars).toArray();
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
        db._query(query, bindVars).toArray();
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
        db._query(query, bindVars).toArray();
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
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testTraverseVertexCollection: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol, @@vCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "@vCol": vn,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail(query + " should not be allowed");
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
      }
    },

    testStartWithSubquery: function () {
      var query = "FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x";
      var bindVars = {
        "@eCol": en,
        "@vCol": vn
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testStepsSubquery: function() {
      var query = "FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart1: function () {
      var query = "FOR x IN OUTBOUND null @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart2: function () {
      var query = "FOR x IN OUTBOUND 1 @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
      };
      try {
        db._query(query, bindVars).toArray();
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testCrazyStart3: function () {
      var query = "FOR x IN OUTBOUND [] @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart4: function () {
      var query = "FOR x IN OUTBOUND 'foobar' @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
    },

    testCrazyStart5: function () {
      var query = "FOR x IN OUTBOUND {foo: 'bar'} @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 0);
    },

    testCrazyStart6: function () {
      var query = "FOR x IN OUTBOUND {_id: @startId} @@eCol RETURN x._id";
      var bindVars = {
        "startId": vertex.B,
        "@eCol": en
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart7: function () {
      var query = "FOR x IN OUTBOUND (FOR y IN @@vCol FILTER y._id == @startId RETURN y) @@eCol RETURN x._id";
      var bindVars = {
        "startId": vertex.B,
        "@eCol": en,
        "@vCol": vn
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Fix the query, just use the first value
      query = "FOR x IN OUTBOUND (FOR y IN @@vCol FILTER y._id == @startId RETURN y)[0] @@eCol RETURN x._id";
      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.C);
    },

    testCrazyStart8: function () {
      var query = "FOR x IN OUTBOUND (FOR y IN @@eCol FILTER y._id == @startId RETURN 'peter') @@eCol RETURN x._id";
      var bindVars = {
        "startId": vertex.A,
        "@eCol": en
      };
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Actually use the string!
      query = "FOR x IN OUTBOUND (FOR y IN @@eCol FILTER y._id == @startId RETURN 'peter')[0] @@eCol RETURN x._id";
      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 0);
    }

  };
}

function complexInternaSuite () {
  var ruleName = "merge-traversal-filter";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

  return {

    setUp: function () {
      opts.allPlans = true;
      opts.verbosePlans = true;
      cleanup();
      createBaseGraph();
    },

    tearDown: cleanup,

    testUnknownVertexCollection: function () {
      const vn2 = "UnitTestVertexCollectionOther";
      db._drop(vn2);
      const vc2 = db._create(vn2);
      vc.save({_key: "1"});
      vc2.save({_key: "1"});
      ec.save(vn + "/1", vn2 + "/1", {});
      var query = "FOR x IN OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vn + "/1"
      };
      // NOTE: vn2 is not explicitly named in AQL
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vn2 + "/1");
      db._drop(vn2);
    },

    testStepsFromLet: function () {
      var query = "LET s = 1 FOR x IN s OUTBOUND @startId @@eCol RETURN x";
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vertex.B);
    },

    testMultipleBlocksResult: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol SORT x._key RETURN x";
      var amount = 10000;
      var startId = vn + "/test";
      var bindVars = {
        "@eCol": en,
        "startId": startId
      };
      vc.save({_key: startId.split("/")[1]});
      
      // Insert amount many edges and vertices into the collections.
      for (var i = 0; i < amount; ++i) {
        var tmp = vc.save({_key: "" + i})._id;
        ec.save(startId, tmp, {});
      }

      // Check that we can get all of them out again.
      var result = db._query(query, bindVars).toArray();
      // Internally: The Query selects elements in chunks, check that nothing is lost.
      assertEqual(result.length, amount);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testSkipSome: function () {
      var query = "FOR x, e, p IN 1..2 OUTBOUND @startId @@eCol LIMIT 4, 100 RETURN p.vertices[1]._key";
      var startId = vn + "/test";
      var bindVars = {
        "@eCol": en,
        "startId": startId
      };
      vc.save({_key: startId.split("/")[1]});
      
      // Insert amount many edges and vertices into the collections.
      for (var i = 0; i < 3; ++i) {
        var tmp = vc.save({_key: "" + i})._id;
        ec.save(startId, tmp, {});
        for (var k = 0; k < 3; ++k) {
          var tmp2 = vc.save({_key: "" + i + "_" + k})._id;
          ec.save(tmp, tmp2, {});
        }
      }

      // Check that we can get all of them out again.
      var result = db._query(query, bindVars).toArray();
      // Internally: The Query selects elements in chunks, check that nothing is lost.
      assertEqual(result.length, 8);

      // Each of the 3 parts of this graph contains of 4 nodes, one connected to the start.
      // And 3 connected to the first one. As we do a depth first traversal we expect to skip
      // exactly one sub-tree. Therefor we expect exactly two sub-trees to be traversed.
      var seen = {};
      for (var r of result) {
        if (!seen.hasOwnProperty(r)) {
          seen[r] = true;
        }
      }
      assertEqual(Object.keys(seen).length, 2);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testManyResults: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._key";
      var startId = vn + "/many";
      var bindVars = {
        "@eCol": en,
        "startId": startId
      };
      vc.save({_key: startId.split("/")[1]});
      var amount = 10000;
      for (var i = 0; i < amount; ++i) {
        var _id = vc.save({});
        ec.save(startId, _id, {});
      }
      var result = db._query(query, bindVars);
      var found = 0;
      // Count has to be correct
      assertEqual(result.count(), amount);
      while (result.hasNext()) {
        result.next();
        ++found;
      }
      // All elements must be enumerated
      assertEqual(found, amount);
    }

  };

}

function complexFilteringSuite () {

  /***********************************************************************
   * Graph under test:
   *
   * C <- B <- A -> D -> E
   * F <--|         |--> G
   *
   *
   *
   *
   *
   * Tri1 --> Tri2
   *  ^        |
   *  |--Tri3<-|
   *
   *
   ***********************************************************************/

  return {
    setUp: function() {
      cleanup();
      var vc = db._create(vn, {numberOfShards: 4});
      var ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertex.A = vc.save({_key: "A", left: false, right: false})._id;
      vertex.B = vc.save({_key: "B", left: true, right: false, value: 25})._id;
      vertex.C = vc.save({_key: "C", left: true, right: false})._id;
      vertex.D = vc.save({_key: "D", left: false, right: true, value: 75})._id;
      vertex.E = vc.save({_key: "E", left: false, right: true})._id;
      vertex.F = vc.save({_key: "F", left: true, right: false})._id;
      vertex.G = vc.save({_key: "G", left: false, right: true})._id;

      edge.AB = ec.save(vertex.A, vertex.B, {left: true, right: false})._id;
      edge.BC = ec.save(vertex.B, vertex.C, {left: true, right: false})._id;
      edge.AD = ec.save(vertex.A, vertex.D, {left: false, right: true})._id;
      edge.DE = ec.save(vertex.D, vertex.E, {left: false, right: true})._id;
      edge.BF = ec.save(vertex.B, vertex.F, {left: true, right: false})._id;
      edge.DG = ec.save(vertex.D, vertex.G, {left: false, right: true})._id;

      vertex.Tri1 = vc.save({_key: "Tri1", isLoop: true})._id;
      vertex.Tri2 = vc.save({_key: "Tri2", isLoop: true})._id;
      vertex.Tri3 = vc.save({_key: "Tri3", isLoop: true})._id;

      edge.Tri12 = ec.save(vertex.Tri1, vertex.Tri2, {isLoop: true})._id;
      edge.Tri23 = ec.save(vertex.Tri2, vertex.Tri3, {isLoop: true})._id;
      edge.Tri31 = ec.save(vertex.Tri3, vertex.Tri1, {isLoop: true, lateLoop: true})._id;
    },

    tearDown: cleanup,

    testVertexEarlyPruneHighDepth: function () {
      var query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.vertices[1]._key == 'wrong' RETURN v";
      var bindVars = {
        "@eCol": en,
        "start": vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // 1 Primary (Tri1)
      // 1 Edge (Tri1->Tri2)
      // 1 Primary (Tri2)
      if (isCluster) {
        assertEqual(stats.scannedIndex, 3);
      }
      else {
        assertEqual(stats.scannedIndex, 2);
      }
      assertEqual(stats.filtered, 1);
    },

    testStartVertexEarlyPruneHighDepth: function () {
      var query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.vertices[0]._key == 'wrong' RETURN v";
      var bindVars = {
        "@eCol": en,
        "start": vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // 1 Primary (Tri1)
      assertEqual(stats.scannedIndex, 1);
      assertEqual(stats.filtered, 1);
    },

    testEdgesEarlyPruneHighDepth: function () {
      var query = "FOR v, e, p IN 100 OUTBOUND @start @@eCol FILTER p.edges[0]._key == 'wrong' RETURN v";
      var bindVars = {
        "@eCol": en,
        "start": vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // 1 Primary (Tri1)
      // 1 Edge (Tri1->Tri2)
      if (isCluster) {
        assertEqual(stats.scannedIndex, 2);
      }
      else {
        // SingleServer can go shortcut here
        assertEqual(stats.scannedIndex, 1);
      }
      assertEqual(stats.filtered, 1);
    },

    testVertexLevel0: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.vertices[0].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // 1 Primary (A)
      // 0 Edge
      assertEqual(stats.scannedIndex, 1);
      // 1 Filter (A)
      assertEqual(stats.filtered, 1);
    },

    testVertexLevel1: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.vertices[1].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 3);
      assertEqual(cursor.toArray(), ["B", "C", "F"]);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 2 Edge Lookups (2 B) (0 D)
        // 2 Primary Lookups (C, F)
        assertEqual(stats.scannedIndex, 9);
      }
      else {
        // Cluster uses a lookup cache.
        // Pointless in single-server mode
        // Makes Primary Lookups for data
        
        // 2 Edge Lookups (A)
        // 2 Primary (B, D) for Filtering
        // 2 Edge Lookups (B)
        // 3 Primary Lookups A -> B
        // 4 Primary Lookups A -> B -> C
        // 4 Primary Lookups A -> B -> F
        assertEqual(stats.scannedIndex, 17);
      }
      // 1 Filter On D
      assertEqual(stats.filtered, 1);
    },

    testVertexLevel2: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.vertices[2].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      // We expect to find C, F
      // B and D will be post filtered
      assertEqual(cursor.count(), 2);
      assertEqual(cursor.toArray(), ["C", "F"]);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Primary lookup B,D
        // 4 Primary Lookups (C, F, E, G)
        assertEqual(stats.scannedIndex, 13);
      }
      else {
        // 2 Edge Lookups (A)
        // 4 Edge Lookups (2 B) (2 D)
        // 4 Primary Lookups for Eval (C, E, G, F)
        // 3 Primary Lookups A -> B
        // 3 Primary Lookups A -> D
        // 4 Primary Lookups A -> B -> C
        // 4 Primary Lookups A -> B -> F
        assertEqual(stats.scannedIndex, 24);
      }
      // 2 Filter (E, G)
      // 2 Filter A->B, A->D path too short
      assertEqual(stats.filtered, 4);
    },

    testVertexLevelsCombined: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.vertices[1].right == true
      FILTER p.vertices[2].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      // Everything should be filtered, no results
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 2 Edge Lookups (0 B) (2 D)
        // 2 Primary Lookups (E, G)
        assertEqual(stats.scannedIndex, 9);
      }
      else {
        // 2 Edge Lookups (A)
        // 2 Primary Lookups for Eval (B, D)
        // 2 Edge Lookups (0 B) (2 D)
        // 2 Primary Lookups for Eval (E, G)
        // 3 Primary Lookups A -> D
        assertEqual(stats.scannedIndex, 11);
      }
      // 1 Filter (B)
      // 2 Filter (E, G)
      // Filter A->D too short
      assertEqual(stats.filtered, 4);
    },

    testEdgeLevel0: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.edges[0].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 3);
      assertEqual(cursor.toArray(), ["B", "C", "F"]);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary (A)
        // 2 Edge
        // 1 Primary (B)
        // 2 Edge
        // 2 Primary (C,F)
        assertEqual(stats.scannedIndex, 8);
      }
      else {
        // 2 Edge Lookups (A)
        // 2 Edge Lookups (B)
        // 3 Primary Lookups A -> B
        // 4 Primary Lookups A -> B -> C
        // 4 Primary Lookups A -> B -> F
        assertEqual(stats.scannedIndex, 15);
      }
      // 1 Filter (A->D)
      assertEqual(stats.filtered, 1);
    },

    testEdgeLevel1: function () {
      var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.edges[1].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        "@ecol": en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 2);
      assertEqual(cursor.toArray(), ["C", "F"]);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 4 Edge Lookups (2 B) (2 D)
        // 2 Primary Lookups (C, F)
        assertEqual(stats.scannedIndex, 11);
      }
      else {
        // 2 Edge Lookups (A)
        // 4 Edge Lookups (2 B) (2 D)
        // 3 Primary Lookups A -> B
        // 3 Primary Lookups A -> D
        // 4 Primary Lookups A -> B -> C
        // 4 Primary Lookups A -> B -> F
        assertEqual(stats.scannedIndex, 20);
      }
      // 2 Filter On (D->E, D->G)
      // Filter on A->D, A->B because path is too short is not counted. No Document!
      assertEqual(stats.filtered, 4);
    },

    testVertexLevel1Less: function () {
      var filters = [
        "FILTER p.vertices[1].value <= 50",
        "FILTER p.vertices[1].value <= 25",
        "FILTER 25 >= p.vertices[1].value",
        "FILTER 50 >= p.vertices[1].value",
        "FILTER p.vertices[1].value < 50",
        "FILTER p.vertices[1].value < 75",
        "FILTER 75 > p.vertices[1].value",
        "FILTER 50 > p.vertices[1].value"
      ];
      for (var f of filters) {
        var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
        ${f}
        SORT v._key
        RETURN v._key`;
        var bindVars = {
          "@ecol": en,
          start: vertex.A
        };
        var cursor = db._query(query, bindVars);
        assertEqual(cursor.count(), 3, query);
        assertEqual(cursor.toArray(), ["B", "C", "F"]);
        var stats = cursor.getExtra().stats;
        assertEqual(stats.scannedFull, 0);
        if (isCluster) {
          // 1 Primary lookup A
          // 2 Edge Lookups (A)
          // 2 Primary lookup B,D
          // 2 Edge Lookups (2 B) (0 D)
          // 2 Primary Lookups (C, F)
          assertEqual(stats.scannedIndex, 9);
        }
        else {
          // Cluster uses a lookup cache.
          // Pointless in single-server mode
          // Makes Primary Lookups for data
          
          // 2 Edge Lookups (A)
          // 2 Primary (B, D) for Filtering
          // 2 Edge Lookups (B)
          // 3 Primary Lookups A -> B
          // 4 Primary Lookups A -> B -> C
          // 4 Primary Lookups A -> B -> F
          assertEqual(stats.scannedIndex, 17);
        }
        // 1 Filter On D
        assertEqual(stats.filtered, 1);
      }
    },

    testVertexLevel1Greater: function () {
      var filters = [
        "FILTER p.vertices[1].value > 50",
        "FILTER p.vertices[1].value > 25",
        "FILTER 25 < p.vertices[1].value",
        "FILTER 50 < p.vertices[1].value",
        "FILTER p.vertices[1].value > 50",
        "FILTER p.vertices[1].value >= 75",
        "FILTER 75 <= p.vertices[1].value",
        "FILTER 50 < p.vertices[1].value"
      ];
      for (var f of filters) {
        var query = `FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
        ${f}
        SORT v._key
        RETURN v._key`;
        var bindVars = {
          "@ecol": en,
          start: vertex.A
        };
        var cursor = db._query(query, bindVars);
        assertEqual(cursor.count(), 3, query);
        assertEqual(cursor.toArray(), ["D", "E", "G"]);
        var stats = cursor.getExtra().stats;
        assertEqual(stats.scannedFull, 0);
        if (isCluster) {
          // 1 Primary lookup A
          // 2 Edge Lookups (A)
          // 2 Primary lookup B,D
          // 2 Edge Lookups (2 B) (0 D)
          // 2 Primary Lookups (C, F)
          assertEqual(stats.scannedIndex, 9);
        }
        else {
          // Cluster uses a lookup cache.
          // Pointless in single-server mode
          // Makes Primary Lookups for data
          
          // 2 Edge Lookups (A)
          // 2 Primary (B, D) for Filtering
          // 2 Edge Lookups (B)
          // 3 Primary Lookups A -> B
          // 4 Primary Lookups A -> B -> C
          // 4 Primary Lookups A -> B -> F
          assertEqual(stats.scannedIndex, 17);
        }
        // 1 Filter On D
        assertEqual(stats.filtered, 1);
      }
    }

  };

}

function brokenGraphSuite () {

  var paramDisabled  = { optimizer: { rules: [ "-all" ] } };

  return {

    setUp: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      vertex.A = vc.save({_key: "A"})._id;
      vertex.B = vc.save({_key: "B"})._id;

      ec.save(vertex.A, vn + "/missing", {});
      ec.save(vn + "/missing", vertex.B, {});
    },

    tearDown: cleanup,

    testRequestMissingVertex: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [null]);
    },

    testStartAtMissingVertex: function () {
      var query = "FOR x IN OUTBOUND @startId @@eCol RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vn + "/missing"
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [vertex.B]);
    },

    testHopOverMissingVertex: function () {
      var query = "FOR x IN 2 OUTBOUND @startId @@eCol RETURN x._id";
      var bindVars = {
        "@eCol": en,
        startId: vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [vertex.B]);
    },

    testFilterOnMissingVertexNotTrue: function () {
      var filter = [
        "FILTER p.vertices[1].attribute == 'missing'",
        "FILTER p.vertices[1].attribute > 12",
        "FILTER p.vertices[1] != null"
      ];
      for (var i = 0; i < filter.length; ++i) {
        var query = `FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
        var bindVars = {
          "@eCol": en,
          startId: vertex.A
        };
        var result = AQL_EXECUTE(query, bindVars).json;
        assertEqual(result.length, 0, "With opt: ", query);
        result = AQL_EXECUTE(query, bindVars, paramDisabled).json;
        assertEqual(result.length, 0, "Without opt: ", query);
      }
    },

    testFilterOnMissingVertexTrue: function () {
      var filter = [
        "FILTER p.vertices[1].attribute != 'missing'",
        "FILTER p.vertices[1].attribute < 12",
        "FILTER p.vertices[1] == null"
      ];
      for (var i = 0; i < filter.length; ++i) {
        var query = `FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
        var bindVars = {
          "@eCol": en,
          startId: vertex.A
        };
        var result = AQL_EXECUTE(query, bindVars).json;
        assertEqual(result.length, 1, "With opt: ", query);
        assertEqual(result, [ vertex.B ], "With opt: ", query);
        result = AQL_EXECUTE(query, bindVars, paramDisabled).json;
        assertEqual(result.length, 1, "Without opt: ", query);
        assertEqual(result, [ vertex.B ], "Without opt: ", query);
      }
    },

    testQueryWithEmptyGraph: function () {
      var query = `FOR x IN OUTBOUND 'start/123' GRAPH @graph RETURN x`;
      var emptyGN = "UnitTestEmptyGraph";
      try {
        gm._drop(emptyGN);
      } catch (e) {
      }
      gm._create(emptyGN);
      var bindVars = {
        graph: emptyGN
      };
      try {
        db._query(query, bindVars);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_GRAPH_EMPTY.code);
      }
    }
  };
}

function multiEdgeDirectionSuite () {
  const en2 = "UnitTestEdgeCollection2";
  var ec2;

  return {

    setUp: function () {
      cleanup();
      db._drop(en2);

      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      ec2 = db._createEdgeCollection(en2, {numberOfShards: 4});
     
      vertex.A = vc.save({_key: "A"})._id;
      vertex.B = vc.save({_key: "B"})._id;
      vertex.C = vc.save({_key: "C"})._id;
      vertex.D = vc.save({_key: "D"})._id;
      vertex.E = vc.save({_key: "E"})._id;

      vertex.F = vc.save({_key: "F"})._id;

      // F is always 2 hops away and only reachable with alternating
      // collections and directions

      ec.save(vertex.A, vertex.B, {});
      ec.save(vertex.C, vertex.A, {});
      ec2.save(vertex.A, vertex.D, {});
      ec2.save(vertex.E, vertex.A, {});

      ec2.save(vertex.F, vertex.B, {});
      ec2.save(vertex.C, vertex.F, {});

      ec.save(vertex.F, vertex.D, {});
      ec.save(vertex.E, vertex.F, {});
    },

    tearDown: function () {
      cleanup();
      db._drop(en2);
    },

    testOverrideOneDirection: function () {
      var queries = [
        { q1 :"FOR x IN ANY @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN ANY @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.E] },
        { q1 :"FOR x IN ANY @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN ANY @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.D] },
        { q1 :"FOR x IN ANY @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN ANY @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D, vertex.E] },
        { q1 :"FOR x IN ANY @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN ANY @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.D, vertex.E] },
        { q1 :"FOR x IN OUTBOUND @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN OUTBOUND @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D] },
        { q1 :"FOR x IN OUTBOUND @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN OUTBOUND @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.E] },
        { q1 :"FOR x IN INBOUND @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN INBOUND @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D] },
        { q1 :"FOR x IN INBOUND @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id",
          q2 :`FOR x IN INBOUND @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.E] },
      ];

      var bindVars = {
        "@ec1": en,
        "@ec2": en2,
        start: vertex.A
      };
      var bindVars2 = {
        start: vertex.A
      };
      queries.forEach(function (item) {
        var result = db._query(item.q1, bindVars).toArray();
        assertEqual(result, item.res);
        result = db._query(item.q2, bindVars2).toArray();
        assertEqual(result, item.res);
      });
    }


  };
}

jsunity.run(namedGraphSuite);
jsunity.run(multiCollectionGraphSuite);
jsunity.run(multiEdgeCollectionGraphSuite);
jsunity.run(potentialErrorsSuite);
jsunity.run(complexInternaSuite);
jsunity.run(complexFilteringSuite);
jsunity.run(brokenGraphSuite);
jsunity.run(multiEdgeDirectionSuite);

return jsunity.done();
