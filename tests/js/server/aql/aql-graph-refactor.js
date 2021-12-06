/* jshint esnext: true */
/* global AQL_EXedgeCollectionUTE, AQL_EXPLAIN, AQL_EXedgeCollectionUTEJSON */

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
const db = internal.db;
const errors = require('@arangodb').errors;
const gm = require('@arangodb/general-graph');
const isCluster = require('@arangodb/cluster').isCluster();

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
  vertices.A = vertexCollection.save({_key: 'A'})._id;
  vertices.B = vertexCollection.save({_key: 'B'})._id;
  vertices.C = vertexCollection.save({_key: 'C'})._id;
  vertices.D = vertexCollection.save({_key: 'D'})._id;
  vertices.E = vertexCollection.save({_key: 'E'})._id;
  vertices.F = vertexCollection.save({_key: 'F'})._id;

  edges.AB = edgeCollection.save(vertices.A, vertices.B, {})._id;
  edges.BC = edgeCollection.save(vertices.B, vertices.C, {})._id;
  edges.CD = edgeCollection.save(vertices.C, vertices.D, {})._id;
};

/*
 * This TestSuite is used for development. We might want to remove it after feature is done and existing tests
 * are adjusted.
 */
function dfsSingleServerDevelopmedgeNametSuite() {
  const refactorDisabled = `OPTIONS {"refactor": false}`;
  const refactorEnabled = `OPTIONS {"refactor": true}`;
  return {
    setUpAll: function () {
      createBaseGraph();
    },

    tearDownAll: function () {
      cleanup();
    },

    testDFSTraversalOld: function () {
      const startVertex = vertices.A;
      let q = `
        FOR v,e,p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        RETURN [v, e, p]
      `;
      let result = db._query(q);
      console.warn(result.toArray());
    },

    testDFSTraversalNew: function () {
      const startVertex = vertices.A;
      let q = `
        FOR v, e, p IN 1..3 OUTBOUND "${startVertex}" GRAPH ${graphName}
        ${refactorEnabled}
        RETURN [v, e, p]
      `;
      let result = db._query(q);
      console.warn(result.toArray());
    },

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
      console.error("Old:");
      console.warn(r1);
      console.error("New:");
      console.warn(r2);

      assertEqual(JSON.stringify(r1), JSON.stringify(r2));
    },

  };
}

jsunity.run(dfsSingleServerDevelopmedgeNametSuite);
return jsunity.done();
