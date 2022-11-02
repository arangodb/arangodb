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
const isCluster = require("internal").isCluster();

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
      // Will generate an array of characters 'a', 'b', ... with number many entries
      const expectProjections = (number) => {
        const charA = "a".charCodeAt(0);
        const res = [];
        for (let i = 0; i < number; ++i) {
          res.push(String.fromCharCode(charA + i));
        }
        return res;
      };

      // Will generate a return statement of RETURN [variable.a, variable.b, ...] with number many entries
      const returnProjections = (variable, number) => {
        const res = expectProjections(number).map(n => `${variable}.${n}`);
        return `RETURN [${res.join(",")}]`;
      };


      let queries = [
        // [ query, vertex projections, edge projections, produce vertices, produce paths vertices, produce paths edges ]
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN 1`, [], ["_from", "_to"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v.x`, ["x"], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[1]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[1].x`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0][1]`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN v[0][1].x`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e[0]`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e[1]`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e[1].x`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e[0][1]`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e[0][1].x`, [], [], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e.x`, [], ["_from", "_to", "x"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p[*]`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p[*].v`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p[*].vertices`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p[*].vertices.x`, [], [], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p[*].vertices[0].x`, [], [], true, true, true],

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
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[*]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[*].a`, ["a"], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[*][1]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.vertices[*][1].a`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.vertices[*].a, p.vertices[*].b]`, ["a", "b"], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.testi, p.vertices[*]]`, [], ["_from", "_to"], true, true, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.testi, p.vertices[*].a]`, ["a", "testi"], ["_from", "_to"], true, true, false],
        
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN e.testi`, [], ["_from", "_to", "testi"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [e.a, e.b]`, [], ["_from", "_to", "a", "b"], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [e.a.b, e.c.d]`, [], ["_from", "_to", ["a", "b"], ["c", "d"]], false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[0]`, [], [], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[0].a`, [], ["_from", "_to", "a"], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.edges[0].a, p.edges[0].b]`, [], ["_from", "_to", "a", "b"], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[*]`, [], [], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN p.edges[*].a`, [], ["_from", "_to", "a"], false, false, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.edges[*].a, p.edges[*].b]`, [], ["_from", "_to", "a", "b"], false, false, true],
        
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a, e.b]`, ["a"], ["_from", "_to", "b"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.a, e.a, v.b, e.b]`, ["a", "b"], ["_from", "_to", "a", "b"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, e.two, v.three, e.four]`, ["one", "three"], ["_from", "_to", "two", "four"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one.two, v.three.four, e.five.six]`, [["one", "two"], ["three", "four"]], ["_from", "_to", ["five", "six"]], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, p.vertices[0].two, e.three, p.edges[1].four]`, ["one", "two"], ["_from", "_to", "three", "four"], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [v.one, p.vertices[-1].one.two, e.three, p.edges[-11].three.four]`, ["one"], ["_from", "_to", "three"], true, true, true],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} RETURN [p.vertices[-1].one.two, p.edges[-11].three.four]`, [["one", "two"]], ["_from", "_to", ["three", "four"]], true, true, true],

        /* Test for PRUNE */
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE v.c == "foo" RETURN e`, ["c"], [], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE e.c == "foo" RETURN v`, [], ["_from", "_to", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE v.c == "foo" && v.a == 1 RETURN e`, ["a", "c"], [], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE e.c == "foo" && e.a == 1 RETURN v`, [], ["_from", "_to", "a", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE e.c == "foo" AND v.bar == "foo" RETURN [v.testi, e.a]`, ["bar", "testi"], ["_from", "_to", "a", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} PRUNE v.c == "foo" AND e.d == "foo" RETURN [v.a, e.b]`, ["a", "c"], ["_from", "_to", "b", "d"], true, false, false],

        /* Test for ALL Filter */
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.vertices[*].c ALL == "foo" RETURN e`, ["c"], [], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.edges[*].c ALL == "foo" RETURN v`, [], ["_from", "_to", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.vertices[*].c ALL == "foo" && p.vertices[*].a ALL == 1 RETURN e`, ["a", "c"], [], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.edges[*].c ALL == "foo" && p.edges[*].a ALL == 1 RETURN v`, [], ["_from", "_to", "a", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.edges[*].c ALL == "foo" AND p.vertices[*].bar ALL == "foo" RETURN [v.testi, e.a]`, ["bar", "testi"], ["_from", "_to", "a", "c"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} FILTER p.vertices[*].c ALL == "foo" AND p.edges[*].d ALL == "foo" RETURN [v.a, e.b]`, ["a", "c"], ["_from", "_to", "b", "d"], true, false, false],

        /* Test for WEIGHT. NOTE: Only cluster needs to retain the distance on projection, as it is internally used on coordinator */
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} OPTIONS {order: "weighted", weightAttribute: "distance", defaultWeight: 1.0} RETURN v`, [], isCluster ? ["_from", "_to", "distance"] : ["_from", "_to"], true, false, false],

        /* Test for max Projections */
        /* Cap to 10 by default*/
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} ${returnProjections("v", 11)}`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} ${returnProjections("e", 9)}`, [], [], false, false, false],
        /* Increase maxProjections beyond cap of 10*/
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} OPTIONS {maxProjections: 20} ${returnProjections("v", 11)}`, expectProjections(11), ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} OPTIONS {maxProjections: 20} ${returnProjections("e", 9)}`, [], expectProjections(9).concat(["_from", "_to"]), false, false, false],
        /* Test decrease maxProjections */
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} ${returnProjections("v", 5)}`, expectProjections(5), ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} ${returnProjections("e", 2)}`, [], expectProjections(2).concat(["_from", "_to"]), false, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} OPTIONS {maxProjections: 4} ${returnProjections("v", 5)}`, [], ["_from", "_to"], true, false, false],
        [`FOR v, e, p IN 1..2 OUTBOUND 'v/test0' ${cn} OPTIONS {maxProjections: 4} ${returnProjections("e", 3)}`, [], [], false, false, false],
      ];


      // normalize projections
      const normalize = (v) => {
        if (v === undefined || v === null) {
          return [];
        }
        return v.sort();
      };

      queries.forEach(function (q) {
        let [query, vertexProjections, edgeProjections, produceVertices, producePathsVertices, producePathsEdges] = q;
        if (isCluster) {
          // In the cluster variant we require the edge_id to be extracted to
          // match unique edges on coordinator.
          if (edgeProjections.length !== 0) {
            edgeProjections.push("_id");
          }
        }
        let plan = AQL_EXPLAIN(query).plan;
        let nodes = plan.nodes.filter(function (node) {
          return node.type === 'TraversalNode';
        });
        assertEqual(1, nodes.length, query);

        let traversalNode = nodes[0];
        assertEqual(normalize(vertexProjections), normalize(traversalNode.options.vertexProjections), {
          q,
          traversalNode
        });
        assertEqual(normalize(edgeProjections), normalize(traversalNode.options.edgeProjections), {q, traversalNode});
        assertEqual(produceVertices, traversalNode.options.produceVertices, {q, traversalNode});
        assertEqual(producePathsVertices, traversalNode.options.producePathsVertices, {q, traversalNode});
        assertEqual(producePathsEdges, traversalNode.options.producePathsEdges, {q, traversalNode});
      });
    },

  };
}

jsunity.run(projectionsTestSuite);

return jsunity.done();
