/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const cn = "UnitTestsOptimizer";

function projectionsTestSuite () {
  return {
    setUpAll : function () {
      db._drop(cn);
      let c = db._createEdgeCollection(cn, { numberOfShards: 4 });

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _from: "v/test" + i, _to: "v/test" + i, _key: "test" + i, value1: i, value2: "test" + i, value3: i, foo: { bar: i, baz: i, bat: i } });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testProjections : function () {
      let queries = [
        // [ query, vertex projections, edge projections, produce vertices, produce paths vertices, produce paths edges ]
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN 1`, [], ["_from", "_to"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[1]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[1].x`, ["x"], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0][1]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0][1].x`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p`, [], [], true, true, true],

        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, e]`, [], [], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, e, p]`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, e, p.vertices]`, [], [], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, e, p.edges]`, [], [], true, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, e, p.vertices, p.edges]`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.vertices, p.edges]`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v, v.testi]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [e, e.testi]`, [], [], false, false, false],
        
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v.testi`, ["testi"], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a, v.b]`, ["a", "b"], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a.b, v.c.d]`, [["a", "b"], ["c", "d"]], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.testi, p.vertices]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices.a`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[0]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[0].a`, ["a"], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[0][1]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[0][1].a`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.vertices[0].a, p.vertices[2].b]`, ["a", "b"], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.testi, p.vertices[0]]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.testi, p.vertices[0].a]`, ["a", "testi"], ["_from", "_to"], true, true, false],
        
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e.testi`, [], ["_from", "_to", "testi"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [e.a, e.b]`, [], ["_from", "_to", "a", "b"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [e.a.b, e.c.d]`, [], ["_from", "_to", ["a", "b"], ["c", "d"]], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[0]`, [], [], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[0].a`, [], ["_from", "_to", "a"], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.edges[0].a, p.edges[0].b]`, [], ["_from", "_to", "a", "b"], false, false, true],
        
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a, e.b]`, ["a"], ["_from", "_to", "b"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a, e.a, v.b, e.b]`, ["a", "b"], ["_from", "_to", "a", "b"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, e.two, v.three, e.four]`, ["one", "three"], ["_from", "_to", "two", "four"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one.two, v.three.four, e.five.six]`, [["one", "two"], ["three", "four"]], ["_from", "_to", ["five", "six"]], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, p.vertices[0].two, e.three, p.edges[1].four]`, ["one", "two"], ["_from", "_to", "three", "four"], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, p.vertices[-1].one.two, e.three, p.edges[-11].three.four]`, ["one"], ["_from", "_to", "three"], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.vertices[-1].one.two, p.edges[-11].three.four]`, [["one", "two"]], ["_from", "_to", ["three", "four"]], true, true, true],
      ];

      // normalize projections
      const normalize = (v) => {
        if (v === undefined || v === null) {
          return [];
        }
        return v.sort();
      };

      queries.forEach(function(q) {
        let [query, vertexProjections, edgeProjections, produceVertices, producePathsVertices, producePathsEdges] = q;
        
        let plan = AQL_EXPLAIN(query).plan;
        let nodes = plan.nodes.filter(function(node) { return node.type === 'TraversalNode'; });
        assertEqual(1, nodes.length, query);

        let traversalNode = nodes[0];
        assertEqual(normalize(vertexProjections), normalize(traversalNode.options.vertexProjections), { q, traversalNode });
        assertEqual(normalize(edgeProjections), normalize(traversalNode.options.edgeProjections), { q, traversalNode });
        assertEqual(produceVertices, traversalNode.options.produceVertices, { q, traversalNode });
        assertEqual(producePathsVertices, traversalNode.options.producePathsVertices, { q, traversalNode });
        assertEqual(producePathsEdges, traversalNode.options.producePathsEdges, { q, traversalNode });
      });
    },

  };
}

jsunity.run(projectionsTestSuite);

return jsunity.done();
