/* global describe, it, before, after, AQL_EXPLAIN*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const db = require('internal').db;

const OptimizeTraversalRule = "optimize-traversals";
const FilterRemoveRule = "remove-filter-covered-by-traversal";
const deactivateOptimizer = { optimizer: { rules: [ "-all" ] } };
const activateOptimizer = { optimizer: { rules: [ "+all" ] } };
const helper = require("@arangodb/aql-helper");
const findExecutionNodes = helper.findExecutionNodes;

describe('Single Traversal Optimizer', function () {
  const vertexCollection = 'UnitTestsOptimizerVertices';
  const edgeCollection = 'UnitTestsOptimizerEdges';
  const startId = `${vertexCollection}/start`;

  const bindVars = {
    "@vertices": vertexCollection,
    "@edges": edgeCollection,
    start: startId
  };

  const dropCollections = () => {
    db._drop(vertexCollection);
    db._drop(edgeCollection);
  };

  const hasNoFilterNode = (plan) => {
    expect(findExecutionNodes(plan, "FilterNode").length).to.equal(0, "Plan should have no filter node");
  };

  const validateResult = (query, bindVars) => {
    let resultOpt = db._query(query, bindVars, activateOptimizer).toArray().sort();
    let resultNoOpt = db._query(query, bindVars, deactivateOptimizer).toArray().sort();
    expect(resultOpt).to.deep.equal(resultNoOpt);
  };

  before(function () {
    dropCollections();
    let v = db._create(vertexCollection);
    let e = db._createEdgeCollection(edgeCollection);
    let vertices = [
      {_key: "start"}
    ];
    let edges = [
    ];

    for (let i = 0; i < 10; ++i) {
      vertices.push({_key: `a${i}`, foo: i});
      edges.push({_from: startId, _to: `${vertexCollection}/a${i}`, foo: i});
      for (let j = 0; j < 10; ++j) {
        vertices.push({_key: `a${i}${j}`, foo: j});
        edges.push({_from: `${vertexCollection}/a${i}`, _to: `${vertexCollection}/a${i}${j}`, foo: j});
      }
    }

    v.save(vertices);
    e.save(edges);
    
  });

  after(dropCollections);

  describe('should remove a single', () => {

    describe('equality filter', () => {
      it('on p.vertices[1].foo', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[1].foo == 3
                       RETURN v`;

        let plan = AQL_EXPLAIN(query, bindVars, activateOptimizer);
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.vertices[2]', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[2].foo == 3
                       RETURN v`;
        let plan = AQL_EXPLAIN(query, bindVars, activateOptimizer);
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.vertices[*] ALL', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 OUTBOUND @start @@edges
                       FILTER p.vertices[*].foo ALL == 3
                       RETURN v`;
        let plan = AQL_EXPLAIN(query, bindVars, activateOptimizer);
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.edges[*] ALL', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 2 ANY @start @@edges
                       FILTER p.edges[*].foo ALL == 3
                       RETURN v`;
        let plan = AQL_EXPLAIN(query, bindVars, activateOptimizer);
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });

      it('on p.edges[*] ALL AND p.vertices[*] NONE', () => {
        let query = `WITH @@vertices
                       FOR v, e, p IN 0..2 ANY @start @@edges
                       FILTER p.edges[*].foo ALL <= 5
                       FILTER p.vertices[*].foo NONE > 6
                       RETURN v`;
        let plan = AQL_EXPLAIN(query, bindVars, activateOptimizer);
        hasNoFilterNode(plan);
        validateResult(query, bindVars);
      });
 
    });

  });


});
