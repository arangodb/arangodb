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

const internal = require('internal');
const db = internal.db;


const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';









function exampleGraphsSuite() {
  let ex = require('@arangodb/graph-examples/example-graph');

  const ruleList = [["-all"], ["+all"], ["-all", "+optimize-traversals"], ["+all", "-optimize-traversals"]];

  const evaluate = (q, expected) => {
    for (const rules of ruleList) {
      let res = db._query(q, {}, { optimizer: { rules } });
      const info = `Query ${q} using rules ${rules}`;
      assertEqual(res.count(), expected.length, info);
      let resArr = res.toArray().sort();
      assertEqual(resArr, expected.sort(), info);
    }
  };

  return {
    setUpAll: () => {
      ex.dropGraph('traversalGraph');
      ex.loadGraph('traversalGraph');
    },

    tearDownAll: () => {
      ex.dropGraph('traversalGraph');
    },

    testMinDepthFilterNEQ: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label != "left_blub"
      RETURN v._key`;

      evaluate(q, ['B', 'C', 'D']);

      q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER "left_blub" != p.edges[1].label
      RETURN v._key`;

      evaluate(q, ['B', 'C', 'D']);
    },

    testMinDepthFilterEq: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label == null
      RETURN v._key`;
      evaluate(q, ['B']);

      q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER null == p.edges[1].label
      RETURN v._key`;
      evaluate(q, ['B']);
    },

    testMinDepthFilterIn: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label IN [null, "left_blarg", "foo", "bar", "foxx"]
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);
    },

    testMinDepthFilterLess: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label < "left_blub"
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);

      q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER "left_blub" > p.edges[1].label
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);
    },

    testMinDepthFilterNIN: () => {
      let q = `FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label NOT IN ["left_blub", "foo", "bar", "foxx"]
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);
    },

    testMinDepthFilterComplexNode: () => {
      let q = `LET condition = { value: "left_blub" }
      FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER p.edges[1].label != condition.value
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);

      q = `LET condition = { value: "left_blub" }
      FOR v,e,p IN 1..3 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.vertices[1]._key != "G"
      FILTER condition.value != p.edges[1].label
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'D']);
    },

    testMinDepthFilterReference: () => {
      let q = `FOR snippet IN ["right"]
      LET test = CONCAT(snippet, "_blob")
      FOR v, e, p IN 1..2 OUTBOUND "circles/A" GRAPH "traversalGraph"
      FILTER p.edges[1].label != test
      RETURN v._key`;
      evaluate(q, ['B', 'C', 'E', 'G', 'J']);
    }
  };
}

jsunity.run(exampleGraphsSuite);
return jsunity.done();
