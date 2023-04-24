/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
// / @author Simon Grätzer
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var graph_module = require("@arangodb/general-graph");
var internal = require("internal");
var console = require("console");
let pregel = require("@arangodb/pregel");
let pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");
// const pregelTestEffectiveClosenessHelpers = require("@arangodb/graph/pregel-test-effective-closeness-helpers");
const pregelTestReadWriteHelpers = require("@arangodb/graph/pregel-test-read-write-helpers");

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function componentsTestSuite() {

    const numComponents = 20; // components
    const n = 200; // vertices
    const m = 300; // edges
    const problematicGraphName = 'problematic';

    // a simple LCG to create deterministic pseudorandom numbers,
    // from https://gist.github.com/Protonk/5389384
    let createRand = function (seed) {
        const m = 25;
        const a = 11;
        const c = 17;

        let z = seed || 3;
        return function () {
            z = (a * z + c) % m;
            return z / m;
        };
    };

    return {

        setUpAll: function () {

            console.log("Beginning to insert test data with " + (numComponents * n) +
              " vertices, " + (numComponents * (m + n)) + " edges");

            var graph = graph_module._create(graphName);
            db._create(vColl, {
                numberOfShards: 4,
                replicationFactor: 1
            });
            graph._addVertexCollection(vColl);
            db._createEdgeCollection(eColl, {
                shardKeys: ["vertex"],
                distributeShardsLike: vColl
            });

            console.log("created graph");

            var rel = graph_module._relation(eColl, [vColl], [vColl]);
            graph._extendEdgeDefinitions(rel);

            console.log("extended edge definition");

            for (let c = 0; c < numComponents; c++) {
                let x = 0;
                let vertices = [];
                while (x < n) {
                    vertices.push({_key: String(c) + ":" + String(x++)});
                }
                db[vColl].insert(vertices);
            }

            assertEqual(db[vColl].count(), numComponents * n);

            console.log("Done inserting vertices, inserting edges");

            let lcg = createRand();

            let edges = [];
            let vertices = [];
            for (let c = 0; c < numComponents; c++) {
                for (let x = 0; x < m; x++) {
                    let fromID = String(c) + ":" + Math.floor(lcg() * n);
                    let toID = String(c) + ":" + Math.floor(lcg() * n);
                    let from = vColl + '/' + fromID;
                    let to = vColl + '/' + toID;
                    edges.push({_from: from, _to: to, vertex: String(fromID)});
                }

                for (let x = 0; x < n; x++) {
                    let fromID = String(c) + ":" + x;
                    let toID = String(c) + ":" + (x + 1);
                    let from = vColl + '/' + fromID;
                    let to = vColl + '/' + toID;
                    vertices.push({_from: from, _to: to, vertex: String(fromID)});
                }
            }
            db[eColl].insert(edges);
            db[eColl].insert(vertices);

            console.log("Got %s edges", db[eColl].count());
            assertEqual(db[eColl].count(), numComponents * m + numComponents * n);

            {
                const v = 'problematic_components';
                const e = 'problematic_connections';

                const edges = [
                    ["A1", "A2"],
                    ["A2", "A3"],
                    ["A3", "A4"],
                    ["A4", "A1"],
                    ["B1", "B3"],
                    ["B2", "B4"],
                    ["B3", "B6"],
                    ["B4", "B3"],
                    ["B4", "B5"],
                    ["B6", "B7"],
                    ["B7", "B8"],
                    ["B7", "B9"],
                    ["B7", "B10"],
                    ["B7", "B19"],
                    ["B11", "B10"],
                    ["B12", "B11"],
                    ["B13", "B12"],
                    ["B13", "B20"],
                    ["B14", "B13"],
                    ["B15", "B14"],
                    ["B15", "B16"],
                    ["B17", "B15"],
                    ["B17", "B18"],
                    ["B19", "B17"],
                    ["B20", "B21"],
                    ["B20", "B22"],
                    ["C1", "C2"],
                    ["C2", "C3"],
                    ["C3", "C4"],
                    ["C4", "C5"],
                    ["C4", "C7"],
                    ["C5", "C6"],
                    ["C5", "C7"],
                    ["C7", "C8"],
                    ["C8", "C9"],
                    ["C8", "C10"],
                ];

                const vertices = new Set(edges.flat());

                try {
                    graph_module._drop(problematicGraphName, true);
                } catch (err) {
                }

                graph_module._create(problematicGraphName, [graph_module._relation(e, v, v)]);

                let vertdocs = [];
                vertices.forEach(vertex => {
                    vertdocs.push({_key: vertex});
                });
                db[v].save(vertdocs);

                let edgedocs = [];
                edges.forEach(([from, to]) => {
                    edgedocs.push({_from: `${v}/${from}`, _to: `${v}/${to}`});
                });
                db[e].save(edgedocs);
            }
        },

        tearDownAll: function () {
            graph_module._drop(graphName, true);
            graph_module._drop(problematicGraphName, true);
        },

        testWCC: function () {
            var pid = pregel.start("wcc", graphName, {resultField: "result", store: true});
            const stats = pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);

            assertEqual(stats.vertexCount, numComponents * n, stats);
            assertEqual(stats.edgeCount, numComponents * (m + n), stats);

            let c = db[vColl].all();
            const uniquePregelResults = pregelTestHelpers.uniquePregelResults(c);
            assertEqual(uniquePregelResults.size, numComponents);

        },

        testWCC2: function () {
            const v = 'problematic_components';

            if (internal.isCluster()) {
                return;
            }

            // weakly connected components algorithm
            var pid = pregel.start('wcc', problematicGraphName, {
                maxGSS: 250, resultField: 'component'
            });
            pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid, 120, 0.2);

            const counts = db._query(
                `FOR vert IN @@v
          COLLECT component = vert.component
          WITH COUNT INTO count
          SORT count DESC
          RETURN count`,
                {"@v": v}
            ).toArray();

            assertEqual(counts.length, 3);
            assertEqual(counts[0], 22);
            assertEqual(counts[1], 10);
            assertEqual(counts[2], 4);
        }
    };
}

function wccRegressionTestSuite() {

    const makeEdge = (from, to) => {
        return {_from: `${vColl}/${from}`, _to: `${vColl}/${to}`, vertex: `${from}`};
    };
    return {

        setUp: function () {
            db._create(vColl, {
                numberOfShards: 4,
                replicationFactor: 1
            });
            db._createEdgeCollection(eColl, {
                numberOfShards: 4,
                replicationFactor: 1,
                shardKeys: ["vertex"],
                distributeShardsLike: vColl
            });

            graph_module._create(graphName, [graph_module._relation(eColl, vColl, vColl)], []);
        },

        tearDown: function () {
            graph_module._drop(graphName, true);
        },

        testWCCLineComponent: function () {
            const vertices = [];
            const edges = [];

            // We create one forward path 100 -> 120 (100 will be component ID, and needs to be propagated outbound)
            for (let i = 100; i < 120; ++i) {
                vertices.push({_key: `${i}`});
                if (i > 100) {
                    edges.push(makeEdge(i - 1, i));
                }
            }

            // We create one backward path 200 <- 220 (200 will be component ID, and needs to be propagated inbound)
            for (let i = 200; i < 220; ++i) {
                vertices.push({_key: `${i}`});
                if (i > 200) {
                    edges.push(makeEdge(i, i - 1));
                }
            }
            db[vColl].save(vertices);
            db[eColl].save(edges);

            const pid = pregel.start("wcc", graphName, {resultField: "result", store: true});
            pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);

            // Now test the result.
            // We expect two components
            const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          RETURN {component, size}
      `;
            const computedComponents = db._query(query).toArray();
            assertEqual(computedComponents.length, 2, `We expected 2 components instead got ${JSON.stringify(computedComponents)}`);
            // Both have 20 elements
            assertEqual(computedComponents[0].size, 20);
            assertEqual(computedComponents[1].size, 20);
        },

        testWCCLostBackwardsConnection: function () {
            const vertices = [];
            const edges = [];

            /*
             * This test's background is a bit tricky and tests a deep technical
             * detail.
             * The idea in this test is the following:
             * We have two Subgroups A and B where there are only OUTBOUND
             * Edges from A to B.
             * In total and ignoring those edges A has a higher ComponentID then B.
             * Now comes the Tricky Part:
             * 1) The Smallest ID in B, needs to have a a long Distance to all contact points to A.
             * 2) The ContactPoints A -> B need to have the second Smallest IDs
             * This now creates the following Effect:
             * The contact points only communitcate in the beginning, as they will not see smaller IDs
             * until the SmallestID arrives.
             * In the implementation showing this Bug, the INBOUND connection was not retained,
             * so as soon as the Smallest ID arrived at B it was not communicated back into the A
             * Cluster, which resulted in two components instead of a single one.
             */

            // We create 20 vertices
            for (let i = 1; i <= 20; ++i) {
                vertices.push({_key: `${i}`});
            }
            // By convention the lowest keys will have the Lowest ids.
            // We need to pick two second lowest Vertices as the ContactPoints of our two clusters:
            edges.push(makeEdge(2, 3));
            // Generate the A side
            edges.push(makeEdge(4, 2));
            edges.push(makeEdge(4, 5));
            edges.push(makeEdge(5, 2));
            // Generate the B side with a Long path to 3
            // A path 6->7->...->20 (the 6 is already low, so not much updates happing)
            for (let i = 7; i <= 20; ++i) {
                edges.push(makeEdge(i - 1, i));
            }
            // Now connect 3 -> 6 and 20 -> 1
            edges.push(makeEdge(3, 6));
            edges.push(makeEdge(20, 1));

            db[vColl].save(vertices);
            db[eColl].save(edges);

            const pid = pregel.start("wcc", graphName, {resultField: "result", store: true});
            pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);

            // Now test the result.
            // We expect two components
            const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          RETURN {component, size}
      `;
            const computedComponents = db._query(query).toArray();
            assertEqual(computedComponents.length, 1, `We expected 1 component instead got ${JSON.stringify(computedComponents)}`);
            // we have all 20 elements
            assertEqual(computedComponents[0].size, 20);
        },

    };
}

const wccTestSuite = pregelTestHelpers.makeWCCTestSuite(false, "", 4);

const sccTestSuite = pregelTestHelpers.makeSCCTestSuite(false, "", 4);

const labelPropagationTestSuite = pregelTestHelpers.makeLabelPropagationTestSuite(false, "", 4);

const pagerankTestSuite = pregelTestHelpers.makePagerankTestSuite(false, "", 4);

const seededPagerankTestSuite = pregelTestHelpers.makeSeededPagerankTestSuite(false, "", 4);

const ssspTestSuite = pregelTestHelpers.makeSSSPTestSuite(false, "", 4);

const hitsTestSuite = pregelTestHelpers.makeHITSTestSuite(false, "", 4);

// const effectiveClosenessTestSuite = pregelTestEffectiveClosenessHelpers.makeEffectiveClosenessTestSuite(false, ", 4");

const readWriteTestSuite = pregelTestReadWriteHelpers.makeReadWriteTestSuite(false, "", 4);

jsunity.run(componentsTestSuite);
jsunity.run(wccRegressionTestSuite);
jsunity.run(wccTestSuite);
jsunity.run(sccTestSuite);
jsunity.run(labelPropagationTestSuite);
jsunity.run(pagerankTestSuite);
jsunity.run(seededPagerankTestSuite);
jsunity.run(ssspTestSuite);
jsunity.run(hitsTestSuite);
// jsunity.run(effectiveClosenessTestSuite);
if (require('internal').db._version(true)['maintainer-mode'] === 'true') {
    jsunity.run(readWriteTestSuite);
}

return jsunity.done();
