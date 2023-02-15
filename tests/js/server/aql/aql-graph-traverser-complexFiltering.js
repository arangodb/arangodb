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
const errors = require('@arangodb').errors;

const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';
const isCluster = require('@arangodb/cluster').isCluster();

const gh = require('@arangodb/graph/helpers');

function complexFilteringSuite() {
  var vertex = {};
  var edge = {};
  /* *********************************************************************
   * Graph under test:
   *
   * C <- B <- A -> D -> E
   * F <--|         |--> G
   *
   *
   * Tri1 --> Tri2
   *  ^        |
   *  |--Tri3<-|
   *
   *
   ***********************************************************************/
  return {
    setUpAll: function () {
      gh.cleanup();
      var vc = db._create(vn, {numberOfShards: 4});
      var ec = db._createEdgeCollection(en, {numberOfShards: 4});
      vertex.A = vc.save({_key: 'A', left: false, right: false})._id;
      vertex.B = vc.save({_key: 'B', left: true, right: false, value: 25})._id;
      vertex.C = vc.save({_key: 'C', left: true, right: false})._id;
      vertex.D = vc.save({_key: 'D', left: false, right: true, value: 75})._id;
      vertex.E = vc.save({_key: 'E', left: false, right: true})._id;
      vertex.F = vc.save({_key: 'F', left: true, right: false})._id;
      vertex.G = vc.save({_key: 'G', left: false, right: true})._id;

      edge.AB = ec.save(vertex.A, vertex.B, {left: true, right: false})._id;
      edge.BC = ec.save(vertex.B, vertex.C, {left: true, right: false})._id;
      edge.AD = ec.save(vertex.A, vertex.D, {left: false, right: true})._id;
      edge.DE = ec.save(vertex.D, vertex.E, {left: false, right: true})._id;
      edge.BF = ec.save(vertex.B, vertex.F, {left: true, right: false})._id;
      edge.DG = ec.save(vertex.D, vertex.G, {left: false, right: true})._id;

      vertex.Tri1 = vc.save({_key: 'Tri1', isLoop: true})._id;
      vertex.Tri2 = vc.save({_key: 'Tri2', isLoop: true})._id;
      vertex.Tri3 = vc.save({_key: 'Tri3', isLoop: true})._id;

      edge.Tri12 = ec.save(vertex.Tri1, vertex.Tri2, {isLoop: true})._id;
      edge.Tri23 = ec.save(vertex.Tri2, vertex.Tri3, {isLoop: true})._id;
      edge.Tri31 = ec.save(vertex.Tri3, vertex.Tri1, {isLoop: true, lateLoop: true})._id;
    },

    tearDownAll: gh.cleanup,

    testPruneWithSubquery: function () {
      let query = `FOR v,e,p IN 1..100 OUTBOUND @start @ecol PRUNE 2 <= LENGTH(FOR w IN p.vertices FILTER w._id == v._id RETURN 1) RETURN p`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.Tri1
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    // Regression test for https://github.com/arangodb/arangodb/issues/12372
    // While subqueries in PRUNE should have already been forbidden, there was
    // a place in the grammar where the subquery wasn't correctly flagged.
    testPruneWithSubquery2: function () {
      // The additional parentheses in LENGTH are important for this test!
      let query = `FOR v,e,p IN 1..100 OUTBOUND @start @ecol PRUNE 2 <= LENGTH((FOR w IN p.vertices FILTER w._id == v._id RETURN 1)) RETURN p`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.Tri1
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    testStorePruneConditionVertexInVariable: function () {
      let condition = "v.value == 75";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} SORT pruneCondition RETURN pruneCondition`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} SORT ${condition} RETURN ${condition}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertTrue(_.isEqual(query1result, query2result));
    },

    testStorePruneConditionVertexInVariableUnusedExplain: function () {
      let condition = "v.value == 75";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} RETURN 1`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 4);
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; 
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
          return;
        }
      });
      assertFalse(foundExtraLet);
    },
   
    testStorePruneConditionVertexInVariableFilter: function () {
      let condition = "v.value == 75";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER pruneCondition RETURN {value: v.value, key: v._key}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} FILTER ${condition} RETURN {value: v.value, key: v._key}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 1);
      assertEqual(query2result.length, 1);
      assertTrue(_.isEqual(query1result, query2result));
      assertEqual(query1result[query1result.length - 1].value, 75);
      assertEqual(query2result[query2result.length - 1].value, 75);
      assertEqual(query1result[query1result.length - 1].key, "D");
      assertEqual(query2result[query2result.length - 1].key, "D");
    },
   
    testStorePruneConditionVertexInVariableFilterWithOptions: function () {
      let condition = "v.value == 75";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} OPTIONS {bfs: true} FILTER pruneCondition RETURN {value: v.value, key: v._key}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} OPTIONS {bfs: true} FILTER ${condition} RETURN {value: v.value, key: v._key}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 1);
      assertEqual(query2result.length, 1);
      assertTrue(_.isEqual(query1result, query2result));
      assertEqual(query1result[query1result.length - 1].value, 75);
      assertEqual(query2result[query2result.length - 1].value, 75);
      assertEqual(query1result[query1result.length - 1].key, "D");
      assertEqual(query2result[query2result.length - 1].key, "D");
    },
 
    testStorePruneConditionVertexInVariableFilterLogicalOr: function () {
      const condition = "v.value == 75 || v.value == 25";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER pruneCondition SORT v._key RETURN {value: v.value, key: v._key}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} FILTER ${condition} SORT v._key RETURN {value: v.value, key: v._key}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 2);
      assertEqual(query2result.length, 2);
      assertTrue(_.isEqual(query1result, query2result));
      assertEqual(query1result[0].value, 25);
      assertEqual(query1result[0].key, "B");
      assertEqual(query1result[1].value, 75);
      assertEqual(query1result[1].key, "D");
    },

    testReferToPruneVariableInPruneCondition: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v.value == 75 && pruneCondition RETURN 1`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testReassignVariableInPruneConditionLet: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v.value == 75 LET pruneCondition = v._key == 'B' RETURN 1`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_VARIABLE_REDECLARED.code);
      }
    },

    testReassignVariableInPruneCondition: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE p = v.value == 75 RETURN 1`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_VARIABLE_REDECLARED.code);
      }
    },

    testReassignOtherVariableInPruneCondition: function () {
      let query = `WITH ${vn} FOR other IN 1..3 FOR v, e, p IN 1..10 OUTBOUND @start @@eCol  PRUNE other = true RETURN 1`;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_VARIABLE_REDECLARED.code);
      }
    },


   testReassignVariableInPruneConditionInAnotherTraversal: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v.value == 75 FOR v2, e2, p2 IN 1..10 OUTBOUND v @@ecol PRUNE pruneCondition = v2.value == 25 RETURN 1 `;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_VARIABLE_REDECLARED.code);
      }
    },

    testAssignVariableInPruneConditionWithSubquery: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = (FOR other IN 1..2 RETURN other) RETURN 1  `;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },

    
    testUseVariableInPruneConditionInTraversal: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v._key == 'G' FILTER pruneCondition FOR v2, e2, p2 IN 1..10 INBOUND v @@eCol 
      FILTER  pruneCondition SORT v2._key RETURN {outerVertex: v._key, innerVertex: v2._key}  `;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 2);
      assertEqual(queryResult[0].outerVertex, 'G');
      assertEqual(queryResult[1].outerVertex, 'G');
      assertEqual(queryResult[0].innerVertex, 'A');
      assertEqual(queryResult[1].innerVertex, 'D');
    },

    testUseVariableInPruneConditionInSubquery: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v._key == 'G' LET subquery = (FOR v2 IN  p.vertices FILTER pruneCondition RETURN v2._key) SORT v._key RETURN {subquery: subquery, vertex: v._key}  `;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 6); //B, C, D, E, F, G
      queryResult.forEach((result, index) => {
        if(index < 5) {
          assertTrue(result["subquery"].length === 0);
        }
        else {
          assertEqual(result["subquery"].length, 3);
          assertEqual(result["subquery"][0],'A');
          assertEqual(result["subquery"][1],'D');
          assertEqual(result["subquery"][2],'G');
        }
      });
    },

    testUseVariableInPruneConditionAsArrayElement: function () {
      let query = `WITH ${vn} LET newArray = [] FOR v,e,p IN 1..2 OUTBOUND @start @@eCol  PRUNE pruneCondition = v._key == 'G' FILTER e.right == true SORT v._key RETURN  PUSH (newArray, pruneCondition) `;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 3);
      assertFalse(queryResult[0][0]); //D
      assertFalse(queryResult[1][0]); //E
      assertTrue(queryResult[2][0]);  //G
    },

   testUseVariableInPruneConditionInFilterNot: function () {
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = v._key == 'G' FILTER NOT pruneCondition SORT v._key RETURN {vertex: v._key} `;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 5);
      assertEqual(queryResult[0].vertex, 'B');
      assertEqual(queryResult[1].vertex, 'C');
      assertEqual(queryResult[2].vertex, 'D');
      assertEqual(queryResult[3].vertex, 'E');
      assertEqual(queryResult[4].vertex, 'F');
   },

    testUseVariableInPruneConditionInPathFilter: function () {
      const condition = "v._key == 'F'";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER e.left == true SORT v._key RETURN {subquery: (FOR v2, e2, p2 IN 1..10 INBOUND v @@eCol FILTER p2.edges[*].left ALL == pruneCondition RETURN e2), vertex: v._key, pruneCondition: pruneCondition}`; 
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let queryResult = db._query(query, bindVars).toArray();
      assertEqual(queryResult.length, 3);
      assertEqual(queryResult[0].vertex, 'B');
      assertTrue(queryResult[0].subquery.length === 0);
      assertEqual(queryResult[1].vertex, 'C');
      assertTrue(queryResult[1].subquery.length === 0);
      assertEqual(queryResult[2].vertex, 'F');
      assertTrue(queryResult[2].subquery.length === 2);
      assertEqual(queryResult[2]["subquery"][0]._from, `${vn}/B`);
      assertEqual(queryResult[2]["subquery"][0]._to, `${vn}/F`);
      assertEqual(queryResult[2]["subquery"][1]._from, `${vn}/A`);
      assertEqual(queryResult[2]["subquery"][1]._to, `${vn}/B`);
    },

     testUseVariableInPruneConditionWithCollect: function () {
      const condition = "v._key == 'F'";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER NOT pruneCondition COLLECT tmp = v._key RETURN {variable: pruneCondition} `;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testUseVariableInPruneConditionWithCollectAfterChange: function () {
      const condition = "v._key == 'F'";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER NOT pruneCondition COLLECT tmp = v._key LET v = {vertex: 'A'} RETURN {variable: pruneCondition} `;
      try {
        let bindVars = {
          '@eCol': en,
          'start': vertex.A
        };
        db._query(query, bindVars);
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testStorePruneConditionEdgeInVariable: function () {
      const condition = "e.left == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} SORT v._key RETURN {key: v._key, from: e._from, to: e._to}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} SORT v._key RETURN {key: v._key, from: e._from, to: e._to}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 4);
      assertEqual(query2result.length, 4);
      assertTrue(_.isEqual(query1result, query2result));
    },

    testStorePruneConditionEdgeInVariableFilter: function () {
      const condition = "e.right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER ${condition} RETURN {from: e._from, to: e._to}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} FILTER ${condition} RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 1);
      assertEqual(query2result.length, 1);
      assertTrue(_.isEqual(query1result, query2result));
      assertEqual(query1result[0].from, `${vn}\/A`);
      assertEqual(query1result[0].to, `${vn}\/D`);
    },

    testStorePruneConditionPathInVariable: function () {
      const condition = "p.edges[0].right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} SORT v._key RETURN {key: v._key}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} SORT v._key RETURN {key: v._key}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 4);
      assertEqual(query2result.length, 4);
      assertTrue(_.isEqual(query1result, query2result));
    },

    testStorePruneConditionPathInVariableFilter: function () {
      const condition = "e.right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol  PRUNE pruneCondition = ${condition} FILTER ${condition} RETURN {key: v._key}`;
      let query2 = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @@eCol PRUNE ${condition} FILTER ${condition} RETURN {key: v._key}`;
      let bindVars = {
        '@eCol': en,
        'start': vertex.A
      };
      let query1result = db._query(query, bindVars).toArray();
      let query2result = db._query(query2, bindVars).toArray();
      assertEqual(query1result.length, 1);
      assertEqual(query2result.length, 1);
      assertTrue(_.isEqual(query1result, query2result));
      assertEqual(query1result[0].key, 'D');
    },

    testStorePruneConditionVertexInVariableExplainLogicalOr: function () {
      const condition = "v.value == 75 || v.value == 25";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} FILTER pruneCondition RETURN 1`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
      });

      // We cannot find an extra LET, as the PruneCondition can be optimized into the Traversal and be transformed into a local
      assertFalse(foundExtraLet);
    },

    testStorePruneConditionVertexInVariableExplainLogicalOrWithOptions: function () {
      const condition = "v.value == 75 || v.value == 25";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} OPTIONS {bfs: true} FILTER pruneCondition RETURN 1`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
        if(node.type === "TraversalNode") {
          assertEqual(node["options"]["order"], "bfs");
        }
      });

      // We cannot find an extra LET, as the PruneCondition can be optimized into the Traversal and be transformed into a local
      assertFalse(foundExtraLet);
    },

    testStorePruneConditionEdgeInVariableExplain: function () {
      const condition = "e.right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} FILTER pruneCondition RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
      });

      // We cannot find an extra LET, as the PruneCondition can be optimized into the Traversal and be transformed into a local
      assertFalse(foundExtraLet);
    },

    testStorePruneConditionEdgeInVariableExplainWithOptions: function () {
      const condition = "e.right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} OPTIONS {bfs: true} FILTER pruneCondition RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
        if(node.type === "TraversalNode") {
          assertEqual(node["options"]["order"], "bfs");
        }
      });

      // We cannot find an extra LET, as the PruneCondition can be optimized into the Traversal and be transformed into a local
      assertFalse(foundExtraLet);
    },

    testStorePruneConditionPathInVariableExplain: function () {
      // note: using NOOPT(...) here to disable the optimization that pulls
      // certain FILTER conditions into the traversal
      const condition = "NOOPT(p.edges[0].right) == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} FILTER pruneCondition RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
      });
      assertTrue(foundExtraLet);
    },

    testStorePruneConditionPathInVariableExplainOptimized: function () {
      const condition = "p.edges[0].right == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} FILTER pruneCondition RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
          return;
        }
      });
      assertFalse(foundExtraLet);
    },

    testStorePruneConditionPathInVariableExplainWithOptions: function () {
      // note: using NOOPT(...) here to disable the optimization that pulls
      // certain FILTER conditions into the traversal
      const condition = "NOOPT(p.edges[0].right) == true";
      let query = `WITH ${vn} FOR v,e,p IN 1..10 OUTBOUND @start @eCol PRUNE pruneCondition = ${condition} OPTIONS {bfs: true} FILTER pruneCondition RETURN {from: e._from, to: e._to}`;
      let bindVars = {
        'eCol': en,
        'start': vertex.A
      };
      let queryExplain = AQL_EXPLAIN(query, bindVars).plan.nodes; //check for extra LET
      let foundExtraLet = false;
      queryExplain.forEach((node) => { 
        if(node.type === "CalculationNode" && node["outVariable"]["name"] === "pruneCondition") {
          foundExtraLet = true;
        }
        if(node.type === "FilterNode") {
          assertEqual(node["inVariable"]["name"], "pruneCondition");
        }
        if(node.type === "TraversalNode") {
          assertEqual(node["options"]["order"], "bfs");
        }
      });
      assertTrue(foundExtraLet);
    },

    testVertexEarlyPruneHighDepth: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 100 OUTBOUND @start @@eCol
      FILTER p.vertices[1]._key == "wrong"
      RETURN v`;
      var bindVars = {
        '@eCol': en,
        'start': vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);

      if (isCluster) {
        // Note: In the cluster case, we're currently pre-fetching. Therefore, our scannedIndex result
        // mismatches in comparison to the SingleServer variant.
        // 1 Primary (Tri1)
        // 1 Edge (Tri1->Tri2)
        // 1 Primary (Tri2)
        assertEqual(stats.scannedIndex, 3);
      } else {
        // one edge and one vertex lookup
        assertEqual(stats.scannedIndex, 2);
      }

      assertEqual(stats.filtered, 1);
    },

    testStartVertexEarlyPruneHighDepth: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 100 OUTBOUND @start @@eCol
      FILTER p.vertices[0]._key == "wrong"
      RETURN v`;
      var bindVars = {
        '@eCol': en,
        'start': vertex.Tri1
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
      FILTER p.edges[0]._key == "wrong"
      RETURN v`;
      var bindVars = {
        '@eCol': en,
        'start': vertex.Tri1
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 0);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      // The lookup will be using the primary Index.
      // It will find 0 elements.
      if (isCluster) {
        // Note: In the cluster case, we're currently pre-fetching. Therefore, our scannedIndex result
        // mismatches in comparison to the SingleServer variant.
        assertEqual(stats.scannedIndex, 1);
      } else {
        assertEqual(stats.scannedIndex, 0);
      }

      assertEqual(stats.filtered, 0);
    },

    testVertexLevel0: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      FILTER p.vertices[0].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        '@ecol': en,
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
        '@ecol': en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 3);
      assertEqual(cursor.toArray(), ['B', 'C', 'F']);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 2 Edge Lookups (2 B) (0 D)
        // 2 Primary Lookups (C, F)
        assertTrue(stats.scannedIndex <= 11);
      } else {
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
          assertEqual(stats.scannedIndex, 13);
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
        '@ecol': en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      // We expect to find C, F
      // B and D will be post filtered
      assertEqual(cursor.count(), 2);
      assertEqual(cursor.toArray(), ['C', 'F']);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Primary lookup B,D
        // 4 Primary Lookups (C, F, E, G)
        assertTrue(stats.scannedIndex <= 13);
      } else {
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
        assertEqual(stats.scannedIndex, 18);
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
        '@ecol': en,
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
        assertTrue(stats.scannedIndex <= 11);
      } else {
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
          assertEqual(stats.scannedIndex, 7);
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
        '@ecol': en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 3);
      assertEqual(cursor.toArray(), ['B', 'C', 'F']);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 2 Edge
        // 1 Primary (B)
        // 2 Edge
        // 2 Primary (C,F)
        assertTrue(stats.scannedIndex <= 8);
      } else {
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
        assertEqual(stats.scannedIndex, 11);
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
        '@ecol': en,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 2);
      assertEqual(cursor.toArray(), ['C', 'F']);
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
          assertTrue(stats.scannedIndex <= 11);
        }
      } else {
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
        assertEqual(stats.scannedIndex, 14);
        */
      }
      // 2 Filter On (B, D) too short
      // 2 Filter On (D->E, D->G)
      assertTrue(stats.filtered <= 4);
    },

    testVertexLevel1Less: function () {
      var filters = [
        'FILTER p.vertices[1].value <= 50',
        'FILTER p.vertices[1].value <= 25',
        'FILTER 25 >= p.vertices[1].value',
        'FILTER 50 >= p.vertices[1].value',
        'FILTER p.vertices[1].value < 50',
        'FILTER p.vertices[1].value < 75',
        'FILTER 75 > p.vertices[1].value',
        'FILTER 50 > p.vertices[1].value'
      ];
      for (var f of filters) {
        var query = `WITH ${vn}
        FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
        ${f}
        SORT v._key
        RETURN v._key`;
        var bindVars = {
          '@ecol': en,
          start: vertex.A
        };
        var cursor = db._query(query, bindVars);
        assertEqual(cursor.count(), 3, query);
        assertEqual(cursor.toArray(), ['B', 'C', 'F']);
        var stats = cursor.getExtra().stats;
        assertEqual(stats.scannedFull, 0);
        if (isCluster) {
          // 1 Primary lookup A
          // 2 Edge Lookups (A)
          // 2 Primary lookup B,D
          // 2 Edge Lookups (2 B) (0 D)
          // 2 Primary Lookups (C, F)
          assertTrue(stats.scannedIndex <= 11, stats.scannedIndex);
        } else {
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
          assertEqual(stats.scannedIndex, 13);
          */
        }
        // 1 Filter On D
        assertEqual(stats.filtered, 1);
      }
    },

    testVertexLevel1Greater: function () {
      var filters = [
        'FILTER p.vertices[1].value > 50',
        'FILTER p.vertices[1].value > 25',
        'FILTER 25 < p.vertices[1].value',
        'FILTER 50 < p.vertices[1].value',
        'FILTER p.vertices[1].value > 50',
        'FILTER p.vertices[1].value >= 75',
        'FILTER 75 <= p.vertices[1].value',
        'FILTER 50 < p.vertices[1].value'
      ];
      for (var f of filters) {
        var query = `WITH ${vn}
        FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
        ${f}
        SORT v._key
        RETURN v._key`;
        var bindVars = {
          '@ecol': en,
          start: vertex.A
        };
        var cursor = db._query(query, bindVars);
        assertEqual(cursor.count(), 3, query);
        assertEqual(cursor.toArray(), ['D', 'E', 'G']);
        var stats = cursor.getExtra().stats;
        assertEqual(stats.scannedFull, 0);
        if (isCluster) {
          // 1 Primary lookup A
          // 2 Edge Lookups (A)
          // 2 Primary lookup B,D
          // 2 Edge Lookups (2 B) (0 D)
          // 2 Primary Lookups (C, F)
          assertTrue(stats.scannedIndex <= 11);
        } else {
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
          assertEqual(stats.scannedIndex, 13);
          */
        }
        // 1 Filter On D
        assertEqual(stats.filtered, 1);
      }
    },

    testModify: function () {
      var query = `WITH ${vn}
      FOR v, e, p IN 1..2 OUTBOUND @start @@ecol
      UPDATE v WITH {updated: true} IN @@vcol
      FILTER p.vertices[1].left == true
      SORT v._key
      RETURN v._key`;
      var bindVars = {
        '@ecol': en,
        '@vcol': vn,
        start: vertex.A
      };
      var cursor = db._query(query, bindVars);
      assertEqual(cursor.count(), 3);
      assertEqual(cursor.toArray(), ['B', 'C', 'F']);
      var stats = cursor.getExtra().stats;
      assertEqual(stats.writesExecuted, 6);
      assertEqual(stats.scannedFull, 0);
      if (isCluster) {
        // 1 Primary lookup A
        // 2 Edge Lookups (A)
        // 2 Primary lookup B,D
        // 2 Edge Lookups (2 B) (0 D)
        // 2 Primary Lookups (C, F)
        assertTrue(stats.scannedIndex <= 13, stats.scannedIndex);
      } else {
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
        assertTrue(stats.scannedIndex <= 28);
        /*
        assertEqual(stats.scannedIndex, 13);
        */
      }
      // 1 Filter On D
      assertEqual(stats.filtered, 3);
    },

  };
}

jsunity.run(complexFilteringSuite);
return jsunity.done();
