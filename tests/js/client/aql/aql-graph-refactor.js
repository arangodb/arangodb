/* jshint esnext: true */
/* global AQL_EXECUTE, AQL_EXPLAIN */

// //////////////////////////////////////////////////////////////////////////////
// / @brief SpedgeCollection for the AQL FOR x IN GRAPH name statemedgeNamet
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / LicedgeNamesed under the Apache LicedgeNamese, Version 2.0 (the "LicedgeNamese");
// / you may not use this file except in compliance with the LicedgeNamese.
// / You may obtain a copy of the LicedgeNamese at
// /
// /     http://www.apache.org/licedgeNameses/LICedgeNameSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the LicedgeNamese is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the LicedgeNamese for the spedgeCollectionific language governing permissions and
// / limitations under the LicedgeNamese.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = require('internal').db;
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const isCluster = require("internal").isCluster();

const _ = require('lodash');

const graphName = "UnitTestGraph";
const vertexName = 'UnitTestVertexCollection';
const edgeName = 'UnitTestEdgeCollection';


let vertices = {};
let edges = {};
let vertexCollection;
let edgeCollection;

let cleanup = function () {
  gm._drop(graphName, true);
};

var createBaseGraph = function () {
  // create graph
  const relation = gm._relation(edgeName, [vertexName], [vertexName]);
  gm._create(graphName, [relation], []);

  vertexCollection = db[vertexName];
  edgeCollection = db[edgeName];

  // populate graph
  vertices.A = vertexCollection.save({_key: 'A', theTruth: true})._id;
  vertices.B = vertexCollection.save({_key: 'B', theTruth: true})._id;
  vertices.C = vertexCollection.save({_key: 'C', theTruth: true})._id;
  vertices.D = vertexCollection.save({_key: 'D', theTruth: false})._id;
  vertices.E = vertexCollection.save({_key: 'E', theTruth: false})._id;
  vertices.F = vertexCollection.save({_key: 'F', theTruth: false})._id;

  edges.AB = edgeCollection.save(vertices.A, vertices.B, {theTruth: true})._id;
  edges.BC = edgeCollection.save(vertices.B, vertices.C, {theTruth: true})._id;
  edges.CD = edgeCollection.save(vertices.C, vertices.D, {theTruth: true})._id;
  edges.DE = edgeCollection.save(vertices.D, vertices.E, {theTruth: false})._id;
  edges.EF = edgeCollection.save(vertices.E, vertices.F, {theTruth: false})._id;
};

/*
 * This TestSuite is used for development. We might want to remove it after feature is done and existing tests
 * are adjusted. Means, as soon as we've also implemented the ClusterProvider in our GraphRefactor for BFS, DFS
 * and WEIGHTED traversals.
 */
function dfsSingleServerDevelopmentEdgeNameSuite() {
  const refactorDisabled = `OPTIONS {"refactor": false}`;
  const refactorEnabled = `OPTIONS {"refactor": true} `;
  return {
    setUpAll: function () {
      createBaseGraph();
    },

    tearDownAll: function () {
      cleanup();
    },

    /* Currently disabled, for testing only.
    testDFSTraversalOld: function () {
      const startVertex = vertices.A;
      let q = `
        FOR v,e,p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        RETURN [v, e, p]
      `;
      db._query(q);
    },

    testDFSTraversalNew: function () {
      const startVertex = vertices.A;
      let q = `
        FOR v, e, p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        ${refactorEnabled}
        RETURN [v, e, p]
      `;
7      db._query(q);
    },

    testBFSTraversalNewWithUniqueVerticesGlobal: function () {
      const startVertex = vertices.A;
      const refactorEnabled = `OPTIONS {"refactor": true, "order": "bfs", "uniqueVertices": "global"} `;
      let q = `
        FOR v, e, p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        ${refactorEnabled}
        RETURN [v, e, p]
      `;
      db._query(q);
    },*/

    testCompareDFSTraverals: function () {
      const startVertex = vertices.A;
      let q1 = `
        FOR v,e,p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        RETURN [v, e, p]
      `;
      let q2 = `
        FOR v, e, p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        ${refactorEnabled}
        RETURN [v, e, p]
      `;
      let r1 = db._query(q1).toArray();
      let r2 = db._query(q2).toArray();
      assertTrue(_.isEqual(r1, r2));
    },

    testDFSTraveralsNewWithPruneOnVertex: function () {
      const startVertex = vertices.A;
      let qOld = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        PRUNE v.theTruth == false
        RETURN p.vertices[*]._key
      `;
      let qRefactor = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        PRUNE v.theTruth == false
        ${refactorEnabled}
        RETURN p.vertices[*]._key
      `;
      let rOld = db._query(qOld);
      let rRefactor = db._query(qRefactor);
      assertTrue(_.isEqual(rOld.toArray(), rRefactor.toArray()));
    },

    testDFSTraveralsNewWithPruneOnEdge: function () {
      const startVertex = vertices.A;
      let qOld = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        PRUNE e.theTruth == true
        RETURN p
      `;
      let qRefactor = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        PRUNE e.theTruth == true
        ${refactorEnabled}
        RETURN p
      `;
      let rOld = db._query(qOld);
      let rRefactor = db._query(qRefactor);
      assertTrue(_.isEqual(rOld.toArray(), rRefactor.toArray()));
    },

    testDFSTraveralsNewWithFilterOnEdge: function () {
      const startVertex = vertices.A;
      let qOld = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        FILTER e.theTruth == true
        RETURN p
      `;
      let qRefactor = `
        FOR v,e,p IN 1..5 OUTBOUND "${startVertex}" GRAPH ${graphName}
        ${refactorEnabled}
        FILTER e.theTruth == true
        RETURN p
      `;
      let rOld = db._query(qOld);
      let rRefactor = db._query(qRefactor);
      assertTrue(_.isEqual(rOld.toArray(), rRefactor.toArray()));
    }
  };
}

if (!internal.isCluster()) {
  jsunity.run(dfsSingleServerDevelopmentEdgeNameSuite);
}
return jsunity.done();