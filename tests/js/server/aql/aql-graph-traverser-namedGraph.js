/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

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

var _ = require('lodash');
const internal = require('internal');
const db = internal.db;

const gm = require('@arangodb/general-graph');
const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';



const gh = require('@arangodb/graph/helpers');

function namedGraphSuite() {
  /* *********************************************************************
   * Graph under test:
   *
   *  A -> B  ->  C -> D
   *      /|\    \|/
   *       E  <-  F
   *
   ***********************************************************************/
  const gn = 'UnitTestGraph';
  var ruleName = 'optimize-traversals';
  var paramEnabled = {optimizer: {rules: ['-all', '+' + ruleName]}};
  var opts = _.clone(paramEnabled);

  return {

    setUpAll: function () {
      opts.allPlans = true;
      opts.verbosePlans = true;
      gh.cleanup();
      gh.createBaseGraph();
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }
      gm._create(gn, [gm._relation(en, vn, vn)]);
    },

    tearDownAll: function () {
      gm._drop(gn);
      gh.cleanup();
    },

    testGraphNameAccessFromParser: function () {
      let queries = [
        [ 'FOR x IN OUTBOUND @startId GRAPH @graph RETURN x', { graph: gn, startId: gh.vertex.B } ],
        [ 'FOR x IN OUTBOUND @startId GRAPH ' + gn + ' RETURN x', { startId: gh.vertex.B } ],
        [ 'FOR x IN OUTBOUND @startId GRAPH "' + gn + '" RETURN x', { startId: gh.vertex.B } ],
        [ 'FOR x IN OUTBOUND @startId GRAPH `' + gn + '` RETURN x', { startId: gh.vertex.B } ],
        [ 'FOR x IN OUTBOUND @startId GRAPH \'' + gn + '\' RETURN x', { startId: gh.vertex.B } ],
      ];

      queries.forEach(function(query) {
        let nodes = AQL_EXPLAIN(query[0], query[1]).plan.nodes;
        assertEqual("TraversalNode", nodes[1].type); 
        assertEqual("UnitTestGraph", nodes[1].graph);
      });
    },

    testNamedFirstEntryIsVertex: function () {
      var query = 'FOR x IN OUTBOUND @startId GRAPH @graph RETURN x';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedSecondEntryIsEdge: function () {
      var query = 'FOR x, e IN OUTBOUND @startId GRAPH @graph RETURN e';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0]._id, gh.edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedThirdEntryIsPath: function () {
      var query = 'FOR x, e, p IN OUTBOUND @startId GRAPH @graph RETURN p';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry.vertices.length, 2);
      assertEqual(entry.vertices[0]._id, gh.vertex.B);
      assertEqual(entry.vertices[1]._id, gh.vertex.C);
      assertEqual(entry.edges.length, 1);
      assertEqual(entry.edges[0]._id, gh.edge.BC);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedOutboundDirection: function () {
      var query = 'FOR x IN OUTBOUND @startId GRAPH @graph RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, gh.vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedInboundDirection: function () {
      var query = 'FOR x IN INBOUND @startId GRAPH @graph RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.C
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      var entry = result[0];
      assertEqual(entry, gh.vertex.B);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedAnyDirection: function () {
      var query = 'FOR x IN ANY @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);
      var entry = result[0];
      assertEqual(entry, gh.vertex.A);
      entry = result[1];
      assertEqual(entry, gh.vertex.C);
      entry = result[2];
      assertEqual(entry, gh.vertex.E);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedExactNumberSteps: function () {
      var query = 'FOR x IN 2 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedRangeNumberSteps: function () {
      var query = 'FOR x IN 2..3 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 3);

      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.E);
      assertEqual(result[2], gh.vertex.F);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedComputedNumberSteps: function () {
      var query = 'FOR x IN LENGTH([1,2]) OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);

      assertEqual(result[0], gh.vertex.D);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedZeroSteps: function () {
      // We only include the start vertex
      var query = 'FOR x IN 0 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 1);
      assertEqual(result[0], gh.vertex.B);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedZeroStartRangeSteps: function () {
      // We only include the start vertex
      var query = 'FOR x IN 0..1 OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.B
      };
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], gh.vertex.B);
      assertEqual(result[1], gh.vertex.C);
      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedSort: function () {
      var query = 'FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id ASC RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.C
      };

      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], gh.vertex.D);
      assertEqual(result[1], gh.vertex.F);

      // Reverse ordering
      query = 'FOR x IN OUTBOUND @startId GRAPH @graph SORT x._id DESC RETURN x._id';

      result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 2);
      assertEqual(result[0], gh.vertex.F);
      assertEqual(result[1], gh.vertex.D);

      var plans = AQL_EXPLAIN(query, bindVars, opts).plans;
      plans.forEach(function (plan) {
        var jsonResult = AQL_EXECUTEJSON(plan, {optimizer: {rules: ['-all']}}).json;
        assertEqual(jsonResult, result, query);
      });
    },

    testNamedUniqueEdgesOnPath: function () {
      var query = 'FOR x IN 6 OUTBOUND @startId GRAPH @graph RETURN x._id';
      var bindVars = {
        graph: gn,
        startId: gh.vertex.A
      };
      // No result A->B->C->F->E->B (->C) is already used!
      var result = db._query(query, bindVars).toArray();
      assertEqual(result.length, 0);

      query = 'FOR x, e, p IN 2 ANY @startId GRAPH @graph SORT x._id ASC ' +
        'RETURN {v: x._id, edges: p.edges, vertices: p.vertices}';
      result = db._query(query, bindVars).toArray();

      // result: A->B->C
      // result: A->B<-E
      // Invalid result: A->B<-A
      assertEqual(result.length, 2);
      assertEqual(result[0].v, gh.vertex.C);
      assertEqual(result[0].edges.length, 2);
      assertEqual(result[0].edges[0]._id, gh.edge.AB);
      assertEqual(result[0].edges[1]._id, gh.edge.BC);

      assertEqual(result[0].vertices.length, 3);
      assertEqual(result[0].vertices[0]._id, gh.vertex.A);
      assertEqual(result[0].vertices[1]._id, gh.vertex.B);
      assertEqual(result[0].vertices[2]._id, gh.vertex.C);
      assertEqual(result[1].v, gh.vertex.E);
      assertEqual(result[1].edges.length, 2);
      assertEqual(result[1].edges[0]._id, gh.edge.AB);
      assertEqual(result[1].edges[1]._id, gh.edge.EB);

      assertEqual(result[1].vertices.length, 3);
      assertEqual(result[1].vertices[0]._id, gh.vertex.A);
      assertEqual(result[1].vertices[1]._id, gh.vertex.B);
      assertEqual(result[1].vertices[2]._id, gh.vertex.E);

      query = `FOR x IN 1 ANY @startId GRAPH @graph
      FOR y IN 1 ANY x GRAPH @graph
      SORT y._id ASC RETURN y._id`;
      result = db._query(query, bindVars).toArray();

      // result: A->B<-A
      // result: A->B->C
      // result: A->B<-E
      // The second traversal resets the path
      assertEqual(result.length, 3);
      assertEqual(result[0], gh.vertex.A);
      assertEqual(result[1], gh.vertex.C);
      assertEqual(result[2], gh.vertex.E);
    }
  };
}

jsunity.run(namedGraphSuite);
return jsunity.done();
