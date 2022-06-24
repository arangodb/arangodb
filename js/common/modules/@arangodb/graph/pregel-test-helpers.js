/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests: graph generation
// /
// / @file
// /
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
let general_graph_module = require("@arangodb/general-graph");
let smart_graph_module = require("@arangodb/smart-graph");
let internal = require("internal");
let pregel = require("@arangodb/pregel");
let graphGeneration = require("@arangodb/graph/graphs-generation");

const graphGenerator = graphGeneration.graphGenerator;

const loadGraphGenerators = function (isSmart) {
    if (isSmart) {
        return {
            makeEdgeBetweenVertices:
                require("@arangodb/graph/graphs-generation-enterprise").enterpriseMakeEdgeBetweenVertices,
            verticesEdgesGenerator:
                require("@arangodb/graph/graphs-generation-enterprise").enterpriseGenerator
        };
    }
    return {
        makeEdgeBetweenVertices: graphGeneration.makeEdgeBetweenVertices,
        verticesEdgesGenerator: graphGeneration.communityGenerator
    };
};

// TODO make this more flexible: input maxWaitTimeSecs and sleepIntervalSecs
const pregelRunSmallInstance = function (algName, graphName, parameters) {
    const pid = pregel.start("wcc", graphName, parameters);
    const maxWaitTimeSecs = 120;
    const sleepIntervalSecs = 0.2;
    let wakeupsLeft = maxWaitTimeSecs / sleepIntervalSecs;
    while (pregel.status(pid).state !== "done" && wakeupsLeft > 0) {
        wakeupsLeft--;
        internal.sleep(0.2);
    }
    return pregel.status(pid);
};

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function makeSetUp(smart, smartAttribute, numberOfShards) {
    return function () {
        if (smart) {
            smart_graph_module._create(graphName, [smart_graph_module._relation(eColl, vColl, vColl)], [],
                {smartGraphAttribute: smartAttribute, numberOfShards: numberOfShards});
        } else {
            db._create(vColl, {numberOfShards: numberOfShards});
            db._createEdgeCollection(eColl, {
                numberOfShards: numberOfShards,
                replicationFactor: 1,
                shardKeys: ["vertex"],
                distributeShardsLike: vColl
            });
            general_graph_module._create(graphName, [general_graph_module._relation(eColl, vColl, vColl)], []);
        }
    };

}

function makeTearDown(smart) {
    if (smart) {
        graphGeneration.smart = false;
        return function () {
            smart_graph_module._drop(graphName, true);
        };
    }
    return function () {
        general_graph_module._drop(graphName, true);
    };
}

class ComponentGenerator {
    constructor(generator, parameters) {
        this.generator = generator;
        this.parameters = parameters;
    }
}

/**
 * Create disjoint components using parameters from componentGenerators, apply WCC on the graph and test
 * the result.
 * @param componentGenerators is a list of ComponentGenerator's. Expected that a ComponentGenerator
 *        has fields generator and parameters, the latter should be an object with the field length.
 */
const testWCCDisjointComponents = function (componentGenerators) {
    // we expect that '_' appears in the string (should it not, return the whole string)
    const extractSizeUniqueLabel = function (v) {
        const indexOf_ = v.indexOf('_');
        return {size: v.substr(0, indexOf_), uniqueLabel: v.substr(indexOf_ + 1)};
    };

    // Produce the graph.
    for (let componentGenerator of componentGenerators) {
        let {vertices, edges} = componentGenerator.generator(componentGenerator.parameters.length);
        db[vColl].save(vertices);
        db[eColl].save(edges);
    }

    // Get the result of Pregel's WCC.
    const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
    assertEqual(status.state, "done", "Pregel Job did never succeed.");

    // Now test the result.
    const query = `
                FOR v IN ${vColl}
                  COLLECT component = v.result WITH COUNT INTO size
                  SORT size DESC
                  RETURN {component, size}
            `;
    const computedComponents = db._query(query).toArray();
    assertEqual(computedComponents.length, componentGenerators.length,
        `We expected ${componentGenerators.length} components, instead got ${JSON.stringify(computedComponents)}`);

    // Test that components contain correct vertices:
    //   we saved the components such that a has vertices with labels
    //   `${s}_i` where s is the size of the component and i is unique among all components
    for (let computedComponent of computedComponents) {
        const componentId = computedComponent.component;
        // get the vertices of the computed component
        const queryVerticesOfComponent = `
        FOR v IN ${vColl}
        FILTER v.result == ${componentId}
          RETURN v.label
      `;
        const vertexLabelsOfComponent = db._query(queryVerticesOfComponent).toArray();
        // note that vertexLabelsOfComponent cannot be empty
        const expectedSize = extractSizeUniqueLabel(vertexLabelsOfComponent[0]).size;
        // test component size
        assertEqual(computedComponent.size, expectedSize,
            `Expected ${expectedSize} as the size of the current component, obtained ${computedComponent.size}`);
        // Test that the labels of vertices in the computed component are all the same.
        for (let vertexLabel of vertexLabelsOfComponent) {
            assertEqual(vertexLabel, `${vertexLabelsOfComponent[0]}`,
                `Expected vertex label is ${vertexLabelsOfComponent[0]}, obtained ${vertexLabel}`);
        }
    }
};

function makeWCCTestSuite(isSmart, smartAttribute, numberOfShards) {

    const {makeEdgeBetweenVertices, verticesEdgesGenerator} = loadGraphGenerators(isSmart);

    return function () {
        'use strict';

        return {

            setUp: makeSetUp(isSmart, smartAttribute, numberOfShards),

            tearDown: makeTearDown(isSmart),

            testWCCFourDirectedCycles: function () {
                const componentSizes = [2, 10, 5, 23];
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size of componentSizes) {
                    const label = `${size}_${uniqueLabel}`;
                    uniqueLabel++;
                    const parameters = {length: size};
                    let componentGenerator = new ComponentGenerator(
                        graphGenerator(verticesEdgesGenerator(vColl, `${label}`)).makeDirectedCycle, parameters);
                    componentGenerators.push(componentGenerator);
                }
                testWCCDisjointComponents(componentGenerators);
            },

            testWCC20DirectedCycles: function () {
                let componentSizes = [];
                // cycles of length smaller than 2 cannot be produced
                for (let i = 2; i < 22; ++i) {
                    componentSizes.push(i);
                }
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size of componentSizes) {
                    const label = `${size}_${uniqueLabel}`;
                    uniqueLabel++;
                    const parameters = {length: size};
                    let componentGenerator = new ComponentGenerator(
                        graphGenerator(verticesEdgesGenerator(vColl, `${label}`)).makeDirectedCycle, parameters);
                    componentGenerators.push(componentGenerator);
                }
                testWCCDisjointComponents(componentGenerators);
            },

            testWCC20AlternatingCycles: function () {
                let componentSizes = [];
                // cycles of length smaller than 2 cannot be produced
                for (let i = 2; i < 22; ++i) {
                    componentSizes.push(i);
                }
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size of componentSizes) {
                    const label = `${size}_${uniqueLabel}`;
                    uniqueLabel++;
                    const parameters = {length: size};
                    let componentGenerator = new ComponentGenerator(
                        graphGenerator(verticesEdgesGenerator(vColl, `${label}`)).makeAlternatingCycle, parameters);
                    componentGenerators.push(componentGenerator);
                }
                testWCCDisjointComponents(componentGenerators);
            },

            testWCCOneSingleVertex: function () {
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "")).makeSingleVertexNoEdges();
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                // We expect one component with one element.
                const query = `
                FOR v IN ${vColl}
                  COLLECT component = v.result WITH COUNT INTO size
                  RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 1, `We expected 1 components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, 1, `We expected 1 element, instead got ${JSON.stringify(computedComponents[0])}`);
            },

            testWCCTwoIsolatedVertices: function () {
                let isolatedVertex0 = graphGenerator(verticesEdgesGenerator(vColl, "v0")).makeOneVertex();
                let isolatedVertex1 = graphGenerator(verticesEdgesGenerator(vColl, "v1")).makeOneVertex();
                const vertices = [isolatedVertex0, isolatedVertex1];
                db[vColl].save(vertices);
                const edges = [];
                db[eColl].save(edges);


                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                FOR v IN ${vColl}
                  COLLECT component = v.result WITH COUNT INTO size
                  RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 2,
                    `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, 1);
                assertEqual(computedComponents[1].size, 1);
            },

            testWCCOneDirectedTree: function () {
                const depth = 3;
                let {
                    vertices,
                    edges
                } = graphGenerator(verticesEdgesGenerator(vColl, "")).makeFullBinaryTree(depth, false);
                db[vColl].save(vertices);
                db[eColl].save(edges);


                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                  FOR v IN ${vColl}
                    COLLECT component = v.result WITH COUNT INTO size
                    SORT size DESC
                    RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);

                assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree

            },

            testWCCOneDepth3AlternatingTree: function () {
                const depth = 3;
                let {
                    vertices,
                    edges
                } = graphGenerator(verticesEdgesGenerator(vColl, "")).makeFullBinaryTree(depth, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);


                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                  FOR v IN ${vColl}
                    COLLECT component = v.result WITH COUNT INTO size
                    SORT size DESC
                    RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);

                assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
            },

            testWCCOneDepth4AlternatingTree: function () {
                const depth = 4;
                let {
                    vertices,
                    edges
                } = graphGenerator(verticesEdgesGenerator(vColl, "")).makeFullBinaryTree(depth, vColl, "v", true);
                db[vColl].save(vertices);
                db[eColl].save(edges);


                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                  FOR v IN ${vColl}
                    COLLECT component = v.result WITH COUNT INTO size
                    SORT size DESC
                    RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);

                assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
            },

            testWCCAlternatingTreeAlternatingCycle: function () {
                // tree
                const depth = 3;
                const {
                    vertices,
                    edges
                } = graphGenerator(verticesEdgesGenerator(vColl, "t")).makeFullBinaryTree(depth, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                // cycle
                const length = 5;
                const resultC = graphGenerator(verticesEdgesGenerator(vColl, "c")).makeAlternatingCycle(length);
                db[vColl].save(resultC.vertices);
                db[eColl].save(resultC.edges);

                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                  FOR v IN ${vColl}
                    COLLECT component = v.result WITH COUNT INTO size
                    SORT size DESC
                    RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);

                assertEqual(computedComponents[0].size, Math.pow(2, depth + 1) - 1); // number of vertices in a full binary tree
                assertEqual(computedComponents[1].size, length);
            },

            testWCCTwoCliquesConnectedByDirectedCycle: function () {
                // first clique
                const size0 = 20;
                const {
                    vertices,
                    edges
                } = graphGenerator(verticesEdgesGenerator(vColl, "c0")).makeBidirectedClique(size0, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                // second clique
                const size1 = 21;
                const resultC1 = graphGenerator(verticesEdgesGenerator(vColl, "c1")).makeAlternatingCycle(size1);
                db[vColl].save(resultC1.vertices);
                db[eColl].save(resultC1.edges);

                // connecting edge
                const connectingEdge = makeEdgeBetweenVertices(vColl, "0", "c0", "0", "c1");
                db[eColl].save([connectingEdge]);

                const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
                assertEqual(status.state, "done", "Pregel Job did never succeed.");

                // Now test the result.
                const query = `
                  FOR v IN ${vColl}
                    COLLECT component = v.result WITH COUNT INTO size
                    SORT size DESC
                    RETURN {component, size}
                `;
                const computedComponents = db._query(query).toArray();
                assertEqual(computedComponents.length, 1, `We expected 1 component, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, size0 + size1);
            },

        };
    };
}

function makeHeavyWCCTestSuite(isSmart, smartAttribute, numberOfShards) {

    const verticesEdgesGenerator = loadGraphGenerators(isSmart).verticesEdgesGenerator;

    return function () {
        'use strict';

        return {

            setUp: makeSetUp(isSmart, smartAttribute, numberOfShards),

            tearDown: makeTearDown(isSmart),

            testWCC10BidirectedCliques() {
                let componentSizes = [];
                for (let i = 120; i < 130; ++i) {
                    componentSizes.push(i);
                }
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size of componentSizes) {
                    const label = `${size}_${uniqueLabel}`;
                    uniqueLabel++;
                    const parameters = {length: size};
                    let componentGenerator = new ComponentGenerator(
                        graphGenerator(verticesEdgesGenerator(vColl, `${label}`)).makeBidirectedClique, parameters);
                    componentGenerators.push(componentGenerator);
                }
                testWCCDisjointComponents(componentGenerators);
            },
        };
    };
}

exports.makeWCCTestSuite = makeWCCTestSuite;
exports.makeHeavyWCCTestSuite = makeHeavyWCCTestSuite;