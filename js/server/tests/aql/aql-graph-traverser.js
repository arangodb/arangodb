/*jshint esnext: true */
/*global assertEqual, assertTrue, fail, AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

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
var mmfilesEngine = (db._engine().name === "mmfiles");
var rocksDBEngine = (db._engine().name === "rocksdb");

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

function limitSuite () {
  const gn = "UnitTestGraph";
  var c;

  return {

    setUpAll: function() {
      db._drop(gn + "v");
      db._drop(gn + "e");
      db._drop(gn + "e2");

      var i;

      var c = db._create(gn + "v");
      for (i = 0; i < 10000; ++i) {
        c.insert({_key: "test" + i });
      }

      c = db._createEdgeCollection(gn + "e");
      for (i = 0; i < 10000; ++i) {
        c.insert({ _from: gn + "v/test" + i, _to: gn + "v/test" + i });
      }

      c = db._createEdgeCollection(gn + "e2");
      c.insert({ _from: gn + "v/test1", _to: gn + "v/test0" });
      c.insert({ _from: gn + "v/test2", _to: gn + "v/test0" });
      c.insert({ _from: gn + "v/test2", _to: gn + "v/test1" });
    },

    tearDownAll: function () {
      db._drop(gn + "v");
      db._drop(gn + "e");
      db._drop(gn + "e2");
    },

    testLimits: function() {
      var queries = [
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 0, 10000 RETURN e", 10000 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 0, 1000 RETURN e", 1000 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 0, 100 RETURN e", 100 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 0, 10 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10, 10 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10, 100 RETURN e", 100 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10, 1000 RETURN e", 1000 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10, 10000 RETURN e", 9990 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 1000, 1 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 1000, 10 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 1000, 100 RETURN e", 100 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 1000, 1000 RETURN e", 1000 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 1000, 10000 RETURN e", 9000 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 1 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 9 RETURN e", 9 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 10 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 11 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 100 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9990, 1000 RETURN e", 10 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9999, 1 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9999, 10 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9999, 100 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 9999, 1000 RETURN e", 1 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10000, 0 RETURN e", 0 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10000, 1 RETURN e", 0 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10000, 10 RETURN e", 0 ], 
        [ "FOR v IN " + gn + "v FOR e IN 1..1 OUTBOUND v._id " + gn + "e LIMIT 10000, 1000 RETURN e", 0 ] 
      ];

      queries.forEach(function(query) {
        assertEqual(query[1], AQL_EXECUTE(query[0]).json.length);
      });
    },
    
    testLimitsMultiEdges: function() {
      var queries = [
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test0'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 RETURN e", 0 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test0'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 0, 1 RETURN e", 0 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test0'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 1, 1 RETURN e", 0 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test1'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test1'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 0, 1 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test1'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 1, 1 RETURN e", 0 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test1'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 2, 1 RETURN e", 0 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 RETURN e", 2 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 0, 1 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 0, 1 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 0, 2 RETURN e", 2 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 1, 1 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 1, 2 RETURN e", 1 ], 
        [ "WITH " + gn + "v FOR v IN ['" + gn + "v/test2'] FOR e IN 1..1 OUTBOUND v " + gn + "e2 LIMIT 2, 1 RETURN e", 0 ]
      ]; 
      
      queries.forEach(function(query) {
        assertEqual(query[1], AQL_EXECUTE(query[0]).json.length, query);
      });
    }

  };
}

function nestedSuite () {
  const gn = "UnitTestGraph";
  var objects, tags, tagged;

  return {

    setUpAll: function() {
      tags = db._create(gn + "tags");
      objects = db._create(gn + "objects");
      tagged = db._createEdgeCollection(gn + "tagged");

      [ "airplane", "bicycle", "train", "car", "boat" ].forEach(function(_key) {
        objects.insert({ _key });
      });

      [ "public", "private", "fast", "slow", "land", "air", "water" ].forEach(function(_key) {
        tags.insert({ _key });
      });

      [
        [ "air", "airplane" ],
        [ "land", "car" ],
        [ "land", "bicycle" ],
        [ "land", "train" ],
        [ "water", "boat" ],
        [ "fast", "airplane" ],
        [ "fast", "car" ],
        [ "slow", "bicycle" ],
        [ "fast", "train" ],
        [ "slow", "boat" ],
        [ "public", "airplane" ],
        [ "private", "car" ],
        [ "private", "bicycle" ],
        [ "public", "train" ],
        [ "public", "boat" ]
      ].forEach(function(edge) {
        tagged.insert({ _from: tags.name() + "/" + edge[0], _to: objects.name() + "/" + edge[1] });
      });
    },

    tearDownAll: function () {
      db._drop(gn + "tags");
      db._drop(gn + "objects");
      db._drop(gn + "tagged");
    },

    testNested: function() {
      var query = "with " + objects.name() + ", " + tags.name() + " for vehicle in any @start1 @@tagged for type in any @start2 @@tagged filter vehicle._id == type._id return vehicle._key";
      
      var result = AQL_EXECUTE(query, { start1: tags.name() + "/land", start2: tags.name() + "/public", "@tagged": tagged.name() }).json;
      assertEqual([ "train" ], result);
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/air", start2: tags.name() + "/fast", "@tagged": tagged.name() }).json;
      assertEqual([ "airplane" ], result);
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/air", start2: tags.name() + "/slow", "@tagged": tagged.name() }).json;
      assertEqual([ ], result);
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/land", start2: tags.name() + "/fast", "@tagged": tagged.name() }).json;
      assertEqual([ "car", "train" ], result.sort());
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/land", start2: tags.name() + "/private", "@tagged": tagged.name() }).json;
      assertEqual([ "bicycle", "car" ], result.sort());
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/public", start2: tags.name() + "/slow", "@tagged": tagged.name() }).json;
      assertEqual([ "boat" ], result);
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/public", start2: tags.name() + "/fast", "@tagged": tagged.name() }).json;
      assertEqual([ "airplane", "train" ], result.sort());
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/public", start2: tags.name() + "/foo", "@tagged": tagged.name() }).json;
      assertEqual([ ], result);
      
      result = AQL_EXECUTE(query, { start1: tags.name() + "/foo", start2: tags.name() + "/fast", "@tagged": tagged.name() }).json;
      assertEqual([ ], result);
    }
  };
}

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
  var ruleName = "optimize-traversals";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

  return {

    setUpAll: function() {
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

    tearDownAll: function () {
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

    testZeroSteps: function () {
      // We only include the start vertex
      var query = "FOR x IN 0 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], vertex.B);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testZeroStartRangeSteps: function () {
      // We only include the start vertex
      var query = "FOR x IN 0..1 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id";
      var bindVars = {
        graph: gn,
        startId: vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.B);
      assertEqual(result[1], vertex.C);
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
  var ruleName = "optimize-traversals";
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
      var query = `WITH ${vn}
        FOR x, e, p IN OUTBOUND "${vertex.B}" ${en}
        SORT x._key
        RETURN {vertex: x, path: p}`;
      var result = db._query(query).toArray();
      validateResult(result);
      var plans = AQL_EXPLAIN(query, { }, opts).plans;
      plans.forEach(function(plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testStartBindParameter: function () {
      var query = `WITH ${vn}
        FOR x, e, p IN OUTBOUND @startId ${en}
        SORT x._key
        RETURN {vertex: x, path: p}`;
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
      var query = `WITH ${vn}
        FOR x, e, p IN OUTBOUND "${vertex.B}" @@eCol
        SORT x._key
        RETURN {vertex: x, path: p}`;
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
      var query = `WITH ${vn}
        FOR x, e, p IN @steps OUTBOUND "${vertex.B}" ${en} 
        SORT x._key
        RETURN {vertex: x, path: p}`;
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
      var query = `WITH ${vn}
        FOR x, e, p IN @lsteps..@rsteps OUTBOUND "${vertex.B}" ${en}
        SORT x._key
        RETURN {vertex: x, path: p}`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._key
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x, e IN OUTBOUND @startId @@eCol
        SORT x._key
        RETURN e`;
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
      var query = `WITH ${vn}
        FOR x, e, p IN OUTBOUND @startId @@eCol
        SORT x._key
        RETURN p`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._key
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN INBOUND @startId @@eCol
        SORT x._key
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN ANY @startId @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN 2 OUTBOUND @startId @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN 2..3 OUTBOUND @startId @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN LENGTH([1,2]) OUTBOUND @startId @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._id ASC
        RETURN x._id`;
      var bindVars = {
        "@eCol": en,
        startId: vertex.C
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], vertex.D);
      assertEqual(result[1], vertex.F);

      // Reverse ordering
      query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._id DESC
        RETURN x._id`;

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
      var query = `WITH ${vn}
        FOR y IN @@vCol
        FOR x IN OUTBOUND y @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR y IN @@vCol
        FOR x IN OUTBOUND y._id @@eCol
        SORT x._id ASC
        RETURN x._id`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        FOR y IN OUTBOUND x._id @@eCol
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
  var ruleName = "optimize-traversals";
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var opts = _.clone(paramEnabled);

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

    tearDownAll: function() {
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
      var query = `FOR x IN OUTBOUND @startId @@eCol, @@vCol RETURN x`;
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
      var query = `FOR x IN OUTBOUND (FOR y IN @@vCol SORT y._id LIMIT 3 RETURN y) @@eCol SORT x._id RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN (FOR y IN 1..1 RETURN y) OUTBOUND @startId @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND null @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND 1 @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND [] @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND 'foobar' @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND {foo: 'bar'} @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND {_id: @startId} @@eCol
        RETURN x._id`;
      var bindVars = {
        "startId": vertex.B,
        "@eCol": en
      };
      var result = db._query(query, bindVars).toArray();
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
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Fix the query, just use the first value
      query = `WITH ${vn}
        FOR x IN OUTBOUND
        (FOR y IN @@vCol FILTER y._id == @startId RETURN y)[0] @@eCol
        RETURN x._id`;
      result = db._query(query, bindVars).toArray();
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
      var x = db._query(query, bindVars);
      var result = x.toArray();
      var extra = x.getExtra();
      assertEqual(result, []);
      assertEqual(extra.warnings.length, 1);
      // Actually use the string!
      query = `WITH ${vn}
        FOR x IN OUTBOUND
        (FOR y IN @@eCol FILTER y._id == @startId RETURN 'peter')[0] @@eCol
        RETURN x._id`;
      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 0);
    }

  };
}

function complexInternaSuite () {
  var ruleName = "optimize-traversals";
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
      var query = `WITH ${vn2}
        FOR x IN OUTBOUND @startId @@eCol
        RETURN x`;
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
      var query = `WITH ${vn}
        LET s = 1
        FOR x IN s OUTBOUND @startId @@eCol
        RETURN x`;
      var bindVars = {
        "@eCol": en,
        "startId": vertex.A
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, vertex.B);
    },

    testMultipleBlocksResult: function () {
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        SORT x._key RETURN x`;
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
      var query = `WITH ${vn}
        FOR x, e, p IN 1..2 OUTBOUND @startId @@eCol
        LIMIT 4, 100
        RETURN p.vertices[1]._key`;
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
      var query = `WITH ${vn}
        FOR x IN OUTBOUND @startId @@eCol
        RETURN x._key`;
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
    },

    testTailRecursion: function () {
      // This test is to make sure their is no
      // inifinite callstack in getSome() API
      let query = `
        WITH ${vn}
        FOR id IN 0..100000
        FOR v IN OUTBOUND CONCAT('${vn}/foobar', id) ${en}
        RETURN v
      `;

      let res = db._query(query);
      assertEqual(res.count(), 0);
      // With inifinit callstack in getSome this
      // test will segfault
    },

    testEdgeOptimizeAboveMinDepth: function () {

      // The query should return depth 1
      // Because edges[1] == null => null != edge.BC => ok
      // And not depth 2/3 because edges[1]._id == edge.BC => not okay
      let query = `
        WITH ${vn}
        FOR v, e, p IN 1..3 OUTBOUND '${vn}/A' ${en}
          FILTER p.edges[1]._id != '${edge.BC}'
          RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [vertex.B]);
    },

    testVertexOptimizeAboveMinDepth: function () {

      // The query should return depth 1
      // Because vertices[2] == null => null != vertex.C => ok
      // And not depth 2/3 because vertices[3]._id == vertex.C => not okay
      let query = `
        WITH ${vn}
        FOR v, e, p IN 1..3 OUTBOUND '${vn}/A' ${en}
          FILTER p.vertices[2]._id != '${vertex.C}'
          RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [vertex.B]);
    },

    testPathOptimizeAboveMinDepth: function () {

      // The query should return depth 1
      // Because vertices[2] == null => null != vertex.C => ok
      // And not depth 2/3 because vertices[3]._id == vertex.C => not okay
      let query = `
        WITH ${vn}
        FOR v, e, p IN 1..3 OUTBOUND '${vn}/A' ${en}
          FILTER p.edges[1]._id != '${edge.BC}'
          FILTER p.vertices[2]._id != '${vertex.C}'
          RETURN v._id`;

      let res = db._query(query);
      assertEqual(res.count(), 1);
      assertEqual(res.toArray(), [vertex.B]);
    },



  };

}

function optimizeInSuite () {

  var ruleName = "optimize-traversals";
  var startId = vn + "/optIn";

  return {

    setUpAll: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vc.save({_key: startId.split("/")[1]});
      
      for (var i = 0; i < 100; ++i) {
        var tmp = vc.save({_key: "tmp" + i, value: i});
        ec.save(startId, tmp._id, {_key: "tmp" + i, value: i});
        for (var j = 0; j < 100; ++j) {
          var innerTmp = vc.save({_key: "innertmp" + i + "_" + j});
          ec.save(tmp._id, innerTmp._id, {});
        }
      }
    },

    tearDownAll: cleanup,

    testSingleOptimize: function () {
      var vertexQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.vertices[1]._key IN @keys
        RETURN v._key`;
      var edgeQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.edges[0]._key IN @keys
        RETURN v._key`;
      var bindVars = {
        "@eCol": en,
        "startId": startId,
        "keys": ["tmp0", "tmp1", "tmp2", "tmp3", "tmp4", "tmp5", "tmp6", "tmp7", "tmp8", "tmp9"]
      };
      
      var result = db._query(vertexQuery, bindVars);
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars);
      extra = result.getExtra();
      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        assertEqual(extra.stats.filtered, 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      // if the rule is disabled we expect to do way more filtering
      var noOpt  = { optimizer: { rules: [ "-all" ] } };
      result = db._query(vertexQuery, bindVars, {}, noOpt);

      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);
    },

    testCombinedAndOptimize: function () {
      var vertexQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.vertices[1]._key IN @keys
        AND p.vertices[1].value IN @values
        RETURN v._key`;
      var edgeQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.edges[0]._key IN @keys
        AND p.edges[0].value IN @values
        RETURN v._key`;
      var mixedQuery1 = `WITH ${vn} 
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.edges[0]._key IN @keys
        AND p.vertices[1].value IN @values
        RETURN v._key`;
      var mixedQuery2 = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER p.vertices[1]._key IN @keys
        AND p.edges[0].value IN @values
        RETURN v._key`;
      var bindVars = {
        "@eCol": en,
        "startId": startId,
        "keys": ["tmp0", "tmp1", "tmp2", "tmp3", "tmp4", "tmp5", "tmp6", "tmp7", "tmp8", "tmp9"],
        "values": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
      };
      
      var result = db._query(vertexQuery, bindVars);
      var extra = result.getExtra();

      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars);
      extra = result.getExtra();

      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        assertEqual(extra.stats.filtered, 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery1, bindVars);
      extra = result.getExtra();

      if (isCluster) {
        // The cluster uses a different index no filtering on _key
        assertEqual(extra.stats.filtered, 0);
      } else {
        // We have only 10 valid elements in the array.
        assertEqual(extra.stats.filtered, 0);
      }
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery2, bindVars);
      extra = result.getExtra();
      // We have only 10 valid elements in the array.
      assertEqual(extra.stats.filtered, 90);
      assertEqual(result.count(), 1000);

      // if the rule is disabled we expect to do way more filtering
      var noOpt  = { optimizer: { rules: [ "-all" ] } };
      result = db._query(vertexQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(edgeQuery, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery1, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);

      result = db._query(mixedQuery2, bindVars, {}, noOpt);
      extra = result.getExtra();
      // For each vertex not in the list we filter once for every connected edge
      assertEqual(extra.stats.filtered, 90 * 100);
      assertEqual(result.count(), 1000);
    },

    testCombinedNoOptimize: function () {
      var vertexQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER @obj IN p.vertices
        RETURN [v, e, p]`;
      var edgeQuery = `WITH ${vn}
        FOR v, e, p IN 2 OUTBOUND @startId @@eCol
        FILTER @obj IN p.edges
        RETURN [v, e, p]`;
      var bindVars = {
        "@eCol": en,
        "startId": startId,
        "obj": {"_key": "tmp0", "value": 0}
      };

      var noOpt  = { optimizer: { rules: [ "-all" ] } };
      var opt  = { optimizer: { rules: [ "-all" , "+" + ruleName ] } };

      var optPlans = AQL_EXPLAIN(vertexQuery, bindVars, opt).plan;
      var noOptPlans = AQL_EXPLAIN(vertexQuery, bindVars, noOpt).plan;
      assertEqual(optPlans.rules, []);
      // This query cannot be optimized by traversal rule
      assertEqual(optPlans, noOptPlans);

      optPlans = AQL_EXPLAIN(edgeQuery, bindVars, opt).plan;
      noOptPlans = AQL_EXPLAIN(edgeQuery, bindVars, noOpt).plan;
      assertEqual(optPlans.rules, []);
      // This query cannot be optimized by traversal rule
      assertEqual(optPlans, noOptPlans);
    },
    

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
    setUpAll: function() {
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

    tearDownAll: cleanup,

    testVertexEarlyPruneHighDepth: function () {
      var query = `WITH ${vn}
        FOR v, e, p IN 100 OUTBOUND @start @@eCol
        FILTER p.vertices[1]._key == 'wrong'
        RETURN v`;
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

      if(mmfilesEngine){
        assertEqual(stats.scannedIndex, 2);
      } else {
        assertEqual(stats.scannedIndex, 1);
      }

      assertEqual(stats.filtered, 1);
    },

    testStartVertexEarlyPruneHighDepth: function () {
      var query = `WITH ${vn}
        FOR v, e, p IN 100 OUTBOUND @start @@eCol
        FILTER p.vertices[0]._key == 'wrong'
        RETURN v`;
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
      var query = `WITH ${vn}
      FOR v, e, p IN 100 OUTBOUND @start @@eCol
      FILTER p.edges[0]._key == 'wrong'
      RETURN v`;
      var bindVars = {
        "@eCol": en,
        "start": vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // The lookup will be using the primary Index.
      // It will find 0 elements.
      assertEqual(stats.scannedIndex, 0);
      assertEqual(stats.filtered, 0);
    },

    testVertexLevel0: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      }
      else {
        // 2 Edge Lookups (A)
        // 2 Primary (B, D) for Filtering
        // 2 Edge Lookups (B)
        // All edges are cached
        // 1 Primary Lookups A -> B (B cached)
        // 1 Primary Lookups A -> B -> C (A, B cached)
        // 1 Primary Lookups A -> B -> F (A, B cached)
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);
        
        // Without traverser-read-cache
        assertTrue(stats.scannedIndex <= 17);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 17);
        } else {
          assertEqual(stats.scannedIndex, 13);
        }
        */
      }
      // 1 Filter On D
      assertEqual(stats.filtered, 1);
    },

    testVertexLevel2: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 13);
        } else {
          assertTrue(stats.scannedIndex <= 7);
        }
      }
      else {
        // 2 Edge Lookups (A)
        // 4 Edge Lookups (2 B) (2 D)
        // 4 Primary Lookups for Eval (C, E, G, F)
        // 2 Primary Lookups A -> B (A, B)
        // 1 Primary Lookups A -> D (D)
        // 0 Primary Lookups A -> B -> C
        // 0 Primary Lookups A -> B -> F
        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 13);

        // With traverser-read-cache
        assertTrue(stats.scannedIndex <= 24);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 24);
        } else {
          assertEqual(stats.scannedIndex, 18);
        }
        */
      }
      // 2 Filter (B, C) too short
      // 2 Filter (E, G)
      assertTrue(stats.filtered <= 4);
    },

    testVertexLevelsCombined: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 2 Edge Lookups (0 B) (2 D)
        // 2 Primary Lookups (E, G)
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      }
      else {
        // 2 Edge Lookups (A)
        // 2 Primary Lookups for Eval (B, D)
        // 2 Edge Lookups (0 B) (2 D)
        // 2 Primary Lookups for Eval (E, G)
        // 1 Primary Lookups A -> D
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        assertTrue(stats.scannedIndex <= 11);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 11);
        } else {
          assertEqual(stats.scannedIndex, 7);
        }
        */
      }
      // 2 Filter (B, D) too short
      // 2 Filter (E, G)
      assertTrue(stats.filtered <= 4);
    },

    testEdgeLevel0: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
        // 2 Edge
        // 1 Primary (B)
        // 2 Edge
        // 2 Primary (C,F)
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 8);
        } else {
          assertTrue(stats.scannedIndex <= 4);
        }
      }
      else {
        // 2 Edge Lookups (A)
        // 2 Edge Lookups (B)
        // 2 Primary Lookups A -> B
        // 1 Primary Lookups A -> B -> C
        // 1 Primary Lookups A -> B -> F
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        assertTrue(stats.scannedIndex <= 15);
        /*
        if (mmfilesEngine){
          assertEqual(stats.scannedIndex, 15);
        } else {
          assertEqual(stats.scannedIndex, 11);
        }
        */
      }
      // 1 Filter (A->D)
      assertEqual(stats.filtered, 1);
    },

    testEdgeLevel1: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
        // 2 Edge Lookups (A)
        // 4 Edge Lookups (2 B) (2 D)
        // 2 Primary Lookups (C, F)
        // It may be that B or D are fetched accidentially
        // they may be inserted in the vertexToFetch list, which
        // lazy loads all vertices in it.
        if (stats.scannedIndex !== 8) {
          if (mmfilesEngine) {
            assertTrue(stats.scannedIndex <= 11);
          } else {
            assertTrue(stats.scannedIndex <= 5);
          }
        }
      }
      else {
        // 2 Edge Lookups (A)
        // 4 Edge Lookups (2 B) (2 D)
        // 2 Primary Lookups A -> B
        // 1 Primary Lookups A -> D
        // 1 Primary Lookups A -> B -> C
        // 1 Primary Lookups A -> B -> F
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 11);
        
        // Without traverser-read-cache
        assertTrue(stats.scannedIndex <= 20);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 20);
        } else {
          assertEqual(stats.scannedIndex, 14);
        }
        */
      }
      // 2 Filter On (B, D) too short 
      // 2 Filter On (D->E, D->G)
      assertTrue(stats.filtered <= 4);
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
        var query = `WITH ${vn}
        FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
          if (mmfilesEngine) {
            assertTrue(stats.scannedIndex <= 9);
          } else {
            // FIXME this used to be 5 ??
            assertTrue(stats.scannedIndex <= 5, stats.scannedIndex);
            //assertEqual(stats.scannedIndex <= 5);
          }
        }
        else {
          // Cluster uses a lookup cache.
          // Pointless in single-server mode
          // Makes Primary Lookups for data
          
          // 2 Edge Lookups (A)
          // 2 Primary (B, D) for Filtering
          // 2 Edge Lookups (B)
          // 1 Primary Lookups A -> B
          // 1 Primary Lookups A -> B -> C
          // 1 Primary Lookups A -> B -> F
          // With traverser-read-cache
          // assertEqual(stats.scannedIndex, 9);
          
          // Without traverser-read-cache
          assertTrue(stats.scannedIndex <= 17);
          /*
          if(mmfilesEngine){
            assertEqual(stats.scannedIndex, 17);
          } else {
            assertEqual(stats.scannedIndex, 13);
          }
          */
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
        var query = `WITH ${vn}
        FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
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
          if (mmfilesEngine) {
            assertTrue(stats.scannedIndex <= 9);
          } else {
            assertTrue(stats.scannedIndex <= 5);
          }
        }
        else {
          // Cluster uses a lookup cache.
          // Pointless in single-server mode
          // Makes Primary Lookups for data
          
          // 2 Edge Lookups (A)
          // 2 Primary (B, D) for Filtering
          // 2 Edge Lookups (B)
          // 1 Primary Lookups A -> B
          // 1 Primary Lookups A -> B -> C
          // 1 Primary Lookups A -> B -> F
          // With traverser-read-cache
          // assertEqual(stats.scannedIndex, 9);
          
          // Without traverser-read-cache
          assertTrue(stats.scannedIndex <= 17);
          /*
          if(mmfilesEngine){
            assertEqual(stats.scannedIndex, 17);
          } else {
            assertEqual(stats.scannedIndex, 13);
          }
          */
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

    setUpAll: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      vertex.A = vc.save({_key: "A"})._id;
      vertex.B = vc.save({_key: "B"})._id;

      ec.save(vertex.A, vn + "/missing", {});
      ec.save(vn + "/missing", vertex.B, {});
    },

    tearDownAll: cleanup,

    testRequestMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN OUTBOUND @startId @@eCol RETURN x._id`;
      var bindVars = {
        "@eCol": en,
        startId: vertex.A
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [null]);
    },

    testStartAtMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN OUTBOUND @startId @@eCol RETURN x._id`;
      var bindVars = {
        "@eCol": en,
        startId: vn + "/missing"
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result, [vertex.B]);
    },

    testHopOverMissingVertex: function () {
      var query = `WITH ${vn} FOR x IN 2 OUTBOUND @startId @@eCol RETURN x._id`;
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
        var query = `WITH ${vn} FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
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
        var query = `WITH ${vn} FOR x, e, p IN 2 OUTBOUND @startId @@eCol ${filter[i]} RETURN x._id`;
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

    setUpAll: function () {
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

    tearDownAll: function () {
      cleanup();
      db._drop(en2);
    },

    testOverrideOneDirection: function () {
      var queries = [
        { q1 :`WITH ${vn} FOR x IN ANY @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN ANY @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.E] },
        { q1 :`WITH ${vn} FOR x IN ANY @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN ANY @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.C, vertex.D] },
        { q1 :`WITH ${vn} FOR x IN ANY @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN ANY @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D, vertex.E] },
        { q1 :`WITH ${vn} FOR x IN ANY @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN ANY @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.D, vertex.E] },
        { q1 :`WITH ${vn} FOR x IN OUTBOUND @start INBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN OUTBOUND @start INBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D] },
        { q1 :`WITH ${vn} FOR x IN OUTBOUND @start @@ec1, INBOUND @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN OUTBOUND @start ${en}, INBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.B, vertex.E] },
        { q1 :`WITH ${vn} FOR x IN INBOUND @start @@ec1, OUTBOUND @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN INBOUND @start ${en}, OUTBOUND ${en2} SORT x._key RETURN x._id`,
          res: [vertex.C, vertex.D] },
        { q1 :`WITH ${vn} FOR x IN INBOUND @start OUTBOUND @@ec1, @@ec2 SORT x._key RETURN x._id`,
          q2 :`WITH ${vn} FOR x IN INBOUND @start OUTBOUND ${en}, ${en2} SORT x._key RETURN x._id`,
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
    },

    testDuplicationCollections: function () {
      var queries = [
        [ `WITH ${vn} FOR x IN ANY @start @@ec, INBOUND @@ec RETURN x`, false ],
        [ `WITH ${vn} FOR x IN ANY @start @@ec, OUTBOUND @@ec RETURN x`, false ],
        [ `WITH ${vn} FOR x IN ANY @start @@ec, ANY @@ec RETURN x`, true ],
        [ `WITH ${vn} FOR x IN INBOUND @start @@ec, INBOUND @@ec RETURN x`, true ],
        [ `WITH ${vn} FOR x IN INBOUND @start @@ec, OUTBOUND @@ec RETURN x`, false ],
        [ `WITH ${vn} FOR x IN INBOUND @start @@ec, ANY @@ec RETURN x`, false ],
        [ `WITH ${vn} FOR x IN OUTBOUND @start @@ec, INBOUND @@ec RETURN x`, false ],
        [ `WITH ${vn} FOR x IN OUTBOUND @start @@ec, OUTBOUND @@ec RETURN x`, true ],
        [ `WITH ${vn} FOR x IN OUTBOUND @start @@ec, ANY @@ec RETURN x`, false ]
      ];

      var bindVars = {
        "@ec": en,
        start: vertex.A
      };
      queries.forEach(function (query) {
        if (query[1]) {
          // should work
          db._query(query[0], bindVars).toArray();
        } else {
          // should fail
          try {
            db._query(query[0], bindVars).toArray();
            fail();
          } catch (e) {
            assertEqual(e.errorNum, errors.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
          }
        }
      });
    }

  };
}

function subQuerySuite () {
  const gn = "UnitTestGraph";

  return {

    /**
     * Graph under Test:
     *
     * A -> B -> [B1, B2, B3, B4, B5]
     *   \> C -> [C1, C2, C3, C4, C5]
     *   \> D -> [D1, D2, D3, D4, D5]
     *
     */

    setUpAll: function () {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});

      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);

      vertex.A = vc.save({_key: "A"})._id;
      vertex.B = vc.save({_key: "B"})._id;
      vertex.C = vc.save({_key: "C"})._id;
      vertex.D = vc.save({_key: "D"})._id;

      vertex.B1 = vc.save({_key: "B1", value: 1})._id;
      vertex.B2 = vc.save({_key: "B2", value: 2})._id;
      vertex.B3 = vc.save({_key: "B3", value: 3})._id;
      vertex.B4 = vc.save({_key: "B4", value: 4})._id;
      vertex.B5 = vc.save({_key: "B5", value: 5})._id;

      vertex.C1 = vc.save({_key: "C1", value: 1})._id;
      vertex.C2 = vc.save({_key: "C2", value: 2})._id;
      vertex.C3 = vc.save({_key: "C3", value: 3})._id;
      vertex.C4 = vc.save({_key: "C4", value: 4})._id;
      vertex.C5 = vc.save({_key: "C5", value: 5})._id;

      vertex.D1 = vc.save({_key: "D1", value: 1})._id;
      vertex.D2 = vc.save({_key: "D2", value: 2})._id;
      vertex.D3 = vc.save({_key: "D3", value: 3})._id;
      vertex.D4 = vc.save({_key: "D4", value: 4})._id;
      vertex.D5 = vc.save({_key: "D5", value: 5})._id;

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
      cleanup();
    },


    // The test is that the traversal in subquery has more then LIMIT many
    // results. In case of a bug the cursor of the traversal is reused for the second
    // iteration as well and does not reset.
    testSubQueryFixedStart: function () {
      var q = `WITH ${vn}
               FOR v IN OUTBOUND "${vertex.A}" ${en}
               SORT v._key
               LET sub = (
                 FOR t IN OUTBOUND "${vertex.B}" ${en}
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
               FOR v IN OUTBOUND "${vertex.A}" ${en}
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
    },



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
         RETURN v`).toArray();
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
           RETURN v`).toArray();
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
         RETURN v`).toArray();
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
         RETURN v`).toArray();
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
           RETURN v`).toArray();
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
         RETURN v`).toArray();
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 22);
        } else {
          assertEqual(stats.scannedIndex, 18);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 8);
        } else {
          assertTrue(stats.scannedIndex <= 4);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // TODO Check for Optimization
        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 17);
        } else {
          assertEqual(stats.scannedIndex, 13);
        }
        */
      }
      assertTrue(stats.filtered <= 2);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo ALL == false
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 8);
        } else {
          assertTrue(stats.scannedIndex <= 4);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 17);
        } else {
          assertEqual(stats.scannedIndex, 13);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 23);
        assertTrue(stats.scannedIndex <= 22);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 22);
        } else {
          assertEqual(stats.scannedIndex, 18);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C, vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 8);
        } else {
          assertTrue(stats.scannedIndex <= 4);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 18);
        assertTrue(stats.scannedIndex <= 17);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 17);
        } else {
          assertEqual(stats.scannedIndex, 13);
        }
        */
      }
      assertEqual(stats.filtered, 1);

      query = `
        FOR v, e, p IN 0..2 OUTBOUND "${vertices.A}" GRAPH "${gn}"
        FILTER p.edges[*].foo NONE == true
        SORT v._key
        RETURN v._id
      `;
      cursor = db._query(query);
      assertEqual(cursor.count(), 4);
      result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.E, vertices.F, vertices.G]);

      stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 8);
        } else {
          assertTrue(stats.scannedIndex <= 4);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 8);

        // Without traverser-read-cache
        // TODO Check for Optimization
        assertTrue(stats.scannedIndex <= 17);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 17);
        } else {
          assertEqual(stats.scannedIndex, 13);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 18);
        } else {
          assertEqual(stats.scannedIndex, 14);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 7);
        } else {
          assertTrue(stats.scannedIndex <= 3);
        }
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 13);
        } else {
          assertEqual(stats.scannedIndex, 9);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      } else {
        // With traverser-read-cache
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 18);
        } else {
          assertEqual(stats.scannedIndex, 14);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 3);
      let result = cursor.toArray();
      assertEqual(result, [vertices.A, vertices.B, vertices.C]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 7);
        } else {
          assertTrue(stats.scannedIndex <= 3);
        }
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 13);
        } else {
          assertEqual(stats.scannedIndex, 9);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 9);
        } else {
          assertTrue(stats.scannedIndex <= 5);
        }
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 9);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 17);
        assertTrue(stats.scannedIndex <= 18);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 18);
        } else {
          assertEqual(stats.scannedIndex, 14);
        }
        */
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
      let cursor = db._query(query);
      assertEqual(cursor.count(), 1);
      let result = cursor.toArray();
      assertEqual(result, [vertices.D]);

      let stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        if (mmfilesEngine) {
          assertTrue(stats.scannedIndex <= 7);
        } else {
          assertTrue(stats.scannedIndex <= 3);
        }
      } else {
        // With activated traverser-read-cache:
        // assertEqual(stats.scannedIndex, 7);

        // Without traverser-read-cache
        // TODO Check for Optimization
        // assertEqual(stats.scannedIndex, 12);
        assertTrue(stats.scannedIndex <= 13);
        /*
        if(mmfilesEngine){
          assertEqual(stats.scannedIndex, 13);
        } else {
          assertEqual(stats.scannedIndex, 9);
        }
        */
      }
      assertTrue(stats.filtered <= 4);
    }
  };
};

function optimizeNonVertexCentricIndexesSuite () {

  let explain = function (query, params) {
    return AQL_EXPLAIN(query, params, { optimizer: { rules: [ "+all" ] } });
  };

  let vertices = {};
  let edges = {};

  return {
    setUpAll: () => {
      cleanup();
      vc = db._create(vn, {numberOfShards: 4});
      ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertices.A = vc.save({_key: "A"})._id;
      vertices.B = vc.save({_key: "B"})._id;
      vertices.C = vc.save({_key: "C"})._id;
      vertices.D = vc.save({_key: "D"})._id;
      vertices.E = vc.save({_key: "E"})._id;
      vertices.F = vc.save({_key: "F"})._id;
      vertices.G = vc.save({_key: "G"})._id;

      vertices.FOO = vc.save({_key: "FOO"})._id;
      vertices.BAR = vc.save({_key: "BAR"})._id;

      edges.AB = ec.save({_key: "AB", _from: vertices.A, _to: vertices.B, foo: "A", bar: true})._id;
      edges.BC = ec.save({_key: "BC", _from: vertices.B, _to: vertices.C, foo: "B", bar: true})._id;
      edges.BD = ec.save({_key: "BD", _from: vertices.B, _to: vertices.D, foo: "C", bar: false})._id;
      edges.AE = ec.save({_key: "AE", _from: vertices.A, _to: vertices.E, foo: "D", bar: true})._id;
      edges.EF = ec.save({_key: "EF", _from: vertices.E, _to: vertices.F, foo: "E", bar: true})._id;
      edges.EG = ec.save({_key: "EG", _from: vertices.E, _to: vertices.G, foo: "F", bar: false})._id;


      // Adding these edges to make the estimate for the edge-index extremly bad
      let badEdges = [];
      for (let j = 0; j < 1000; ++j) {
        badEdges.push({_from: vertices.FOO, _to: vertices.BAR, foo: "foo" + j, bar: j});
      }
      ec.save(badEdges);
    },

    tearDownAll: cleanup,

    tearDown: () => {
      // After each test get rid of all superflous indexes.
      var idxs = db[en].getIndexes();
      for (let i = 2; i < idxs.length; ++i) {
        db[en].dropIndex(idxs[i].id);
      }
    },

    testUniqueHashIndex : () => {
      var idx = db[en].ensureIndex({type: "hash", fields: ["foo"], unique: true, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[0].foo == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.levels["0"];
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testUniqueSkiplistIndex : () => {
      var idx = db[en].ensureIndex({type: "skiplist", fields: ["foo"], unique: true, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[0].foo == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.levels["0"];
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testAllUniqueHashIndex : () => {
      var idx = db[en].ensureIndex({type: "hash", fields: ["foo"], unique: true, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[*].foo ALL == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.base;
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
    },

    testAllUniqueSkiplistIndex : () => {
      var idx = db[en].ensureIndex({type: "skiplist", fields: ["foo"], unique: true, sparse: false});
      // This index is assumed to be better than edge-index, but does not contain _from/_to
      let q = `FOR v,e,p IN OUTBOUND "${vertices.A}" ${en}
               FILTER p.edges[*].foo ALL == "A"
               RETURN v._id`;

      let exp = explain(q, {}).plan.nodes.filter(node => {return node.type === "TraversalNode";});
      assertEqual(1, exp.length);
      // Check if we did use the hash index on level 0
      let indexes  = exp[0].indexes;
      let found = indexes.base;
      assertEqual(1, found.length);
      found = found[0];
      assertEqual(idx.type, found.type);
      assertEqual(idx.fields, found.fields);

      let result = db._query(q).toArray();
      assertEqual(result[0], vertices.B);
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
      let res = db._query(q);
      assertEqual(res.count(), 3);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());

      q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER 'left_blub' != p.edges[1].label
             RETURN v._key`;
      res = db._query(q);
      assertEqual(res.count(), 3);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
    },

    testMinDepthFilterEq: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label == null
                 RETURN v._key`;
      let res = db._query(q);
      assertEqual(res.count(), 1);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ['B'].sort());

      q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
             FILTER p.vertices[1]._key != 'G'
             FILTER null == p.edges[1].label
             RETURN v._key`;
      res = db._query(q);
      assertEqual(res.count(), 1);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B'].sort());
 
    },

    testMinDepthFilterIn: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label IN [null, 'left_blarg', 'foo', 'bar', 'foxx']
                 RETURN v._key`;
      let res = db._query(q);
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
      res = db._query(q);
      assertEqual(res.count(), 3);
      resArr = res.toArray().sort();
      assertEqual(resArr, ['B', 'C', 'D'].sort());
 
    },

    testMinDepthFilterNIN: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND 'circles/A' GRAPH 'traversalGraph'
                 FILTER p.vertices[1]._key != 'G'
                 FILTER p.edges[1].label NOT IN ['left_blub', 'foo', 'bar', 'foxx']
                 RETURN v._key`;
      let res = db._query(q);
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
      let res = db._query(q);
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
      
      let res = db._query(q);
      assertEqual(res.count(), 5);
      let resArr = res.toArray().sort();
      assertEqual(resArr, ["B", "C", "E", "G", "J"].sort());
    }

  };
};

jsunity.run(limitSuite);
jsunity.run(nestedSuite);
jsunity.run(namedGraphSuite);
jsunity.run(multiCollectionGraphSuite);
jsunity.run(multiEdgeCollectionGraphSuite);
jsunity.run(potentialErrorsSuite);
jsunity.run(complexInternaSuite);
jsunity.run(optimizeInSuite);
jsunity.run(complexFilteringSuite);
jsunity.run(brokenGraphSuite);
jsunity.run(multiEdgeDirectionSuite);
jsunity.run(subQuerySuite);
jsunity.run(optionsSuite);
jsunity.run(optimizeQuantifierSuite);
jsunity.run(exampleGraphsSuite);
if (!isCluster) {
  jsunity.run(optimizeNonVertexCentricIndexesSuite);
}

return jsunity.done();
