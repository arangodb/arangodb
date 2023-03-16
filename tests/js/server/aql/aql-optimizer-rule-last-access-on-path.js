/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global AQL_EXPLAIN */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for optimizer rule that transforms last access on path to vertex and edge
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const gm = require("@arangodb/general-graph");
const {db} = require("@arangodb");

function TraversalOptimizeLastPathAccessTestSuite() {
    const makePath = (length, keyOffset = 0) => {
        const vertices = [];
        const edges = [];
        for (let i = 0; i < length; ++i) {
          vertices.push({_key: `${i}`, _id: `v/${i}`});
          if (i > 0) {
            edges.push({_key: `${i-1}-${i}`, _id: `e/${i-1}-${i}`, _from: `v/${i-1}`, _to: `v/${i}`});
          }
        }
        return {vertices, edges};
    };
    // These paths are used to "simulate" traversal output, to make sure optimizer does not do funny stuff
    const hardCodedPaths = [
        makePath(1, 0),
        makePath(2, 0),
        makePath(3, 0),
        makePath(1, 10),
        makePath(2, 10),
        makePath(3, 10),
    ];

    const graphName = "UnitTestGraph";
    const vc = "UnitTestVertexCollection";
    const ec = "UnitTestEdgeCollection";
    const startVertex = "UnitTestVertexCollection/1";

    return {
        setUpAll: function () {
            gm._create(graphName, [gm._relation(ec, vc, vc)], [], {numberOfShards: 3});
        },

        tearDownAll: function () {
            gm._drop(graphName, true);
        },

        testShouldNotImpactNonTraversals: function () {
            const queriesToTest = [
                `FOR p IN ${JSON.stringify(hardCodedPaths)} RETURN p.vertices[-1]`,
                `FOR p IN ${JSON.stringify(hardCodedPaths)} RETURN p.edges[-1]`,
                `FOR p IN ${vc} RETURN p.vertices[-1]`,
                `FOR p IN ${vc} RETURN p.edges[-1]`,
            ];
            for (const q of queriesToTest) {
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                require("internal").print(rules);

            }
        },

        testShouldImpactTraversals: function () {
            const queriesToTest = [
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" GRAPH "${graphName}" RETURN p.vertices[-1]`,
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" GRAPH "${graphName}" RETURN p.edges[-1]`,
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.vertices[-1]`,
                `FOR v, e, p IN 1..2 OUTBOUND "${startVertex}" ${ec} RETURN p.edges[-1]`
            ];
            for (const q of queriesToTest) {
                const {nodes, rules} = AQL_EXPLAIN(q).plan;
                require("internal").print(rules);
                require("internal").print(nodes);
            }
        }
    };
}



jsunity.run(TraversalOptimizeLastPathAccessTestSuite);

return jsunity.done();
