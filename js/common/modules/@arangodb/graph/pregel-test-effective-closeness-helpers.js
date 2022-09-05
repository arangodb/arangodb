/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / @author Roman Rabinovich
// //////////////////////////////////////////////////////////////////////////////

let db = require("@arangodb").db;
const isEnterprise = require('internal').isEnterprise();
let smart_graph_module;
if (isEnterprise) {
    smart_graph_module = require("@arangodb/smart-graph");
}

const {
    graphName,
    vColl,
    eColl
} = require("@arangodb/graph/graph-in-database");

const {
    loadGraphGenerators,
    unionGraph,
    graphGenerator,
    Graph
} = require("@arangodb/graph/graphs-generation");

const {
    runPregelInstance,
    assertAlmostEquals,
    epsilon,
    makeSetUp,
    makeTearDown
} = require("@arangodb/graph/pregel-test-helpers");


const computeCloseness = function (graph) {
    for (let [vKey, v] of graph.vertices) {
        graph.bfs(vKey, graph.writeDistance, undefined);
        let sumOfDistances = 0;
        let count = 0;
        for (let [, w] of graph.vertices) {
            if (w.distance === 0 || w.distance === undefined) { // w is not reachable from v
                continue;
            }
            ++count;
            sumOfDistances += w.distance;
            w.distance = 0; // reset for the next v

        }
        v.closenessFromTestAlgorithm = sumOfDistances / count;
    }
};

const testEffectiveClosenessOnGraph = function (vertices, edges) {
    db[vColl].save(vertices);
    db[eColl].save(edges);
    const query = `
                  FOR v in ${vColl}
                  RETURN {"_key": v._key, "closeness": v.closeness, "v": v}  
              `;

    let parameters = {resultField: "closeness"};
    let algName = "effectivecloseness";
    const result = runPregelInstance(algName, graphName, parameters, query);

    let graph = new Graph(vertices, edges);
    computeCloseness(graph); // write result into the closenessFromTestAlgorithm field
    // check that Pregel returned the same result as our test algorithm.

    for (const resultV of result) {
        const vKey = resultV._key;
        const v = graph.vertex(vKey);
        const doThrow = false;
        assertAlmostEquals(resultV.closeness, v.closenessFromTestAlgorithm, epsilon,
            `Different closeness values for vertex ${vKey}`,
            "Pregel returned",
            "test returned",
            "",
            doThrow
        );
    }
};

function makeEffectiveClosenessTestSuite(isSmart, smartAttribute, numberOfShards) {

    const verticesEdgesGenerator = loadGraphGenerators(isSmart).verticesEdgesGenerator;
    const makeEdgeBetweenVertices = loadGraphGenerators(isSmart).makeEdgeBetweenVertices;

    return function () {
        'use strict';

        return {

            setUp: makeSetUp(isSmart, smartAttribute, numberOfShards),

            tearDown: makeTearDown(isSmart),

            testEffectiveClosenessOnPath: function () {
                const length = 143;
                const kind = "bidirected";
                const {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makePath(length, kind);
                testEffectiveClosenessOnGraph(vertices, edges);

            },
            testEffectiveClosenessOnTwoDisjointDirectedCycles: function () {
                const length = 3;
                const subgraph01 = graphGenerator(verticesEdgesGenerator(vColl, "v0")).makeDirectedCycle(length);
                const subgraph02 = graphGenerator(verticesEdgesGenerator(vColl, "v1")).makeDirectedCycle(length);
                const {vertices, edges} = unionGraph([subgraph01, subgraph02]);
                testEffectiveClosenessOnGraph(vertices, edges);
            },
            testEffectiveClosenessOnClique: function () {
                const size = 10;
                const kind = "bidirected";
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeClique(size, kind);
                testEffectiveClosenessOnGraph(vertices, edges);
            },
            testEffectiveClosenessOnStar: function () {
                const numLeaves = 10;
                const kind = "bidirected";
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeStar(numLeaves, kind);
                testEffectiveClosenessOnGraph(vertices, edges);
            }
        };
    };
}

// Effective Closeness does not return correct results. On an undirected path with at least 143 vertices
// an overflow or an underflow seems to happen and Pregel tells closeness == 4465789918.386364
// (the ad-hoc test computeCloseness gives for the same vertex which is a leaf of the path 70.5).
// For shorter paths, the error reaches 16%.

// exports.makeEffectiveClosenessTestSuite = makeEffectiveClosenessTestSuite;