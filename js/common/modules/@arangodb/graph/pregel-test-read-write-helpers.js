/*jshint globalstrict:false, strict:false */
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
} = require("@arangodb/graph/graphs-generation");

const {
    runPregelInstance,
    assertAlmostEquals,
    epsilon,
    makeSetUp,
    makeTearDown
} = require("@arangodb/graph/pregel-test-helpers");

const testReadWriteOnGraph = function (vertices, edges) {
    db[vColl].save(vertices);
    db[eColl].save(edges);
    const query = `
                  FOR v in ${vColl}
                  RETURN {"_key": v._key, "input": v.input, "output": v.output}  
              `;

    let parameters = {sourceField: "input", resultField: "output"};
    let algName = "readwrite";
    const result = runPregelInstance(algName, graphName, parameters, query);
    
    for (const resultV of result) {
        const vKey = resultV._key;
        const doThrow = false;
        assertAlmostEquals(resultV.input, resultV.output, epsilon,
            `Different input an output values for vertex ${vKey}`,
            "Input",
            "Output",
            `For vertex ${resultV._key}`,
            doThrow
        );
    }
};

const setConsecutiveValuesFrom0 = function (vertices) {
    let i = 0;
    for (let v of vertices) {
        v.input = i++;
    }
};

function makeReadWriteTestSuite(isSmart, smartAttribute, numberOfShards) {

    const verticesEdgesGenerator = loadGraphGenerators(isSmart).verticesEdgesGenerator;

    return function () {
        return {

            setUp: makeSetUp(isSmart, smartAttribute, numberOfShards),

            tearDown: makeTearDown(isSmart),

            testReadWritePath: function () {
                const length = 143;
                const kind = "bidirected";
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makePath(length, kind);
                setConsecutiveValuesFrom0(vertices);
                testReadWriteOnGraph(vertices, edges);

            },
            testReadWriteOnTwoDisjointDirectedCycles: function () {
                const length = 3;
                const subgraph01 = graphGenerator(verticesEdgesGenerator(vColl, "v0")).makeDirectedCycle(length);
                const subgraph02 = graphGenerator(verticesEdgesGenerator(vColl, "v1")).makeDirectedCycle(length);
                let {vertices, edges} = unionGraph([subgraph01, subgraph02]);
                setConsecutiveValuesFrom0(vertices);
                testReadWriteOnGraph(vertices, edges);
            },
            testReadWriteOnClique: function () {
                const size = 10;
                const kind = "bidirected";
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeClique(size, kind);
                setConsecutiveValuesFrom0(vertices);
                testReadWriteOnGraph(vertices, edges);
            },
            testReadWriteOnStar: function () {
                const numLeaves = 10;
                const kind = "bidirected";
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeStar(numLeaves, kind);
                setConsecutiveValuesFrom0(vertices);
                testReadWriteOnGraph(vertices, edges);
            }
        };
    };
}

exports.makeReadWriteTestSuite = makeReadWriteTestSuite;