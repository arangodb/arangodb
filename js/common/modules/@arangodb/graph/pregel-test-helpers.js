/*jshint globalstrict:false, strict:false */
/*global assertEqual, JSON */
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
const isEnterprise = require('internal').isEnterprise();
let smart_graph_module;
if (isEnterprise) {
    smart_graph_module = require("@arangodb/smart-graph");
}
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
// TODO make this more flexible: input maxWaitTimeSecs and sleepIntervalSecs
const pregelRunSmallInstance = function (algName, graphName, parameters) {
    const pid = pregel.start(algName, graphName, parameters);
    const maxWaitTimeSecs = 120;
    const sleepIntervalSecs = 0.2;
    let wakeupsLeft = maxWaitTimeSecs / sleepIntervalSecs;
    while (pregel.status(pid).state !== "done" && wakeupsLeft > 0) {
        wakeupsLeft--;
        internal.sleep(0.2);
    }
    const statusState = pregel.status(pid).state;
    assertEqual(statusState, "done", `Pregel Job did never succeed. Status: ${statusState}`);

    // Now test the result.
    const query = `
        FOR v IN ${vColl}
          COLLECT component = v.result WITH COUNT INTO size
          SORT size DESC
          RETURN {component, size}
      `;
    return db._query(query).toArray();
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
 * the result. The function relies on the generators to produce vertex labels in the form <size>_<uniqueLabel>
 *     where <size> is the correct size of the component and uniqueLabels are the same within one generators
 *     but unique w.r.t. the other generators.
 * @param componentGenerators is a list of ComponentGenerator's. Expected that a ComponentGenerator
 *        has fields generator and parameters, the latter should be an object with the field length.
 * @param algorithmName
 */
const testComponentsAlgorithmOnDisjointComponents = function (componentGenerators, algorithmName = "wcc") {
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
    const computedComponents = pregelRunSmallInstance(algorithmName, graphName, {resultField: "result", store: true});
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

/**
 * A wrapper for testComponentsAlgorithmOnDisjointComponents. If the latter gets the same generators and the parameter
 * is only "length", pass the sizes, the generator and the algorithm name.
 * @param sizes An array of sizes of the resulting components.
 * @param generator The generator function.
 * @param algorithmName The algorithm name.
 */
const testComponentsAlgorithmOnDisjointComponentsSameKind = function (sizes, generator, algorithmName = "wcc") {
    console.warn(generator);
    let componentGenerators = [];
    for (const i of sizes) {
        componentGenerators.push(new ComponentGenerator(generator, {length: i}));
    }
    testComponentsAlgorithmOnDisjointComponents(componentGenerators, algorithmName);
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
                testComponentsAlgorithmOnDisjointComponents(componentGenerators);
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
                testComponentsAlgorithmOnDisjointComponents(componentGenerators);
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
                testComponentsAlgorithmOnDisjointComponents(componentGenerators);
            },

            testWCCOneSingleVertex: function () {
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "")).makeSingleVertexNoEdges();
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true})
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

                const computedComponents = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
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

            testWCC10BidirectedCliques: function() {
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
                testComponentsAlgorithmOnDisjointComponents(componentGenerators);
            },
        };
    };
}

function makeSCCTestSuite(isSmart, smartAttribute, numberOfShards) {

    const verticesEdgesGenerator = loadGraphGenerators(isSmart).verticesEdgesGenerator;
    const makeEdgesBetweenVertices = loadGraphGenerators(isSmart).makeEdgeBetweenVertices;

    return function () {
        'use strict';

        return {

            setUp: makeSetUp(isSmart, smartAttribute, numberOfShards),

            tearDown: makeTearDown(isSmart),

            // todo: refactor s.t. one can pass the generator without set unique label
            //  then the first lines of the following can be abstracted into a function
            testSCCFourDirectedCycles: function() {
                let componentGenerators = [];
                const sizes = [2, 10, 5, 23];
                let uniqueLabel = 0;
                for (const size of sizes) {
                    componentGenerators.push(new ComponentGenerator(graphGenerator(verticesEdgesGenerator(vColl,
                        `${size}_${uniqueLabel++}`)).makeDirectedCycle, {length: size}));
                }
                testComponentsAlgorithmOnDisjointComponents(componentGenerators, "scc");
            },

            testSCC20DirectedCycles: function() {
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size = 2; size < 22; ++size) {
                    componentGenerators.push(new ComponentGenerator(graphGenerator(verticesEdgesGenerator(vColl,
                        `${size}_${uniqueLabel++}`)).makeDirectedCycle, {length: size}));
                }
                testComponentsAlgorithmOnDisjointComponents(componentGenerators, "scc");
            },

            testSCC10BidirectedCliques() {
                let componentGenerators = [];
                let uniqueLabel = 0;
                for (let size = 120; size < 130; ++size) {
                    componentGenerators.push(new ComponentGenerator(graphGenerator(verticesEdgesGenerator(vColl,
                        `${size}_${uniqueLabel++}`)).makeDirectedCycle, {length: size}));
                }

                testComponentsAlgorithmOnDisjointComponents(componentGenerators, "scc");
            },

            testSCCOneSingleVertex: function() {
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeSingleVertexNoEdges("v");
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, 1, `We expected 1 components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, 1, `We expected 1 element, instead got ${JSON.stringify(computedComponents[0])}`);
            },

            testSCCTwoIsolatedVertices: function() {
                let isolatedVertex0 = graphGenerator(verticesEdgesGenerator(vColl, "v0")).makeOneVertex();
                let isolatedVertex1 = graphGenerator(verticesEdgesGenerator(vColl, "v1")).makeOneVertex();
                const vertices = [isolatedVertex0, isolatedVertex1];
                db[vColl].save(vertices);
                const edges = [];
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, 1);
                assertEqual(computedComponents[1].size, 1);
            },

            testSCCOneEdge: function() {
                let vertex0 = graphGenerator(verticesEdgesGenerator(vColl, "v0")).makeOneVertex();
                let vertex1 = graphGenerator(verticesEdgesGenerator(vColl, "v1")).makeOneVertex();
                const vertices = [vertex0, vertex1];
                db[vColl].save(vertices);
                const edges = [makeEdgesBetweenVertices(vColl, 0, "v0", 0, "v1")];
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, 2, `We expected 2 components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, 1);
                assertEqual(computedComponents[1].size, 1);
            },

            testSCCOneDirected10Path: function() {
                const length = 10;
                const {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makePath(length);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, length,
                    `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
                for (const component of computedComponents) {
                    assertEqual(component.size, 1);
                }
            },

            testSCCOneBidirected10Path: function() {
                const length = 10;
                const {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makePath(length, "bidirected");
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, 1,
                    `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
                assertEqual(computedComponents[0].size, length);
            },

            testSCCOneAlternated10Path: function() {
                const length = 10;
                const {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makePath(length, "alternating");
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, length,
                    `We expected ${length} components, instead got ${JSON.stringify(computedComponents)}`);
                for (const component of computedComponents) {
                    assertEqual(component.size, 1);
                }
            },

            testSCCOneDirectedTree: function()  {
                // Each vertex induces its own strongly connected component.
                const depth = 3;
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeFullBinaryTree(depth, false);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                // number of vertices in a full binary tree
                const numVertices = Math.pow(2, depth + 1) - 1;
                assertEqual(computedComponents.length, numVertices,
                    `We expected ${numVertices} components, instead got ${JSON.stringify(computedComponents)}`);

                for (const component of computedComponents) {
                    assertEqual(component.size, 1);
                }

            },

            testSCCOneDepth3AlternatingTree: function() {
                // Each vertex induces its own strongly connected component.
                const depth = 3;
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeFullBinaryTree(depth, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                // number of vertices in a full binary tree
                const numVertices = Math.pow(2, depth + 1) - 1;
                assertEqual(computedComponents.length, numVertices,
                    `We expected ${numVertices} component, instead got ${JSON.stringify(computedComponents)}`);

                for (const component of computedComponents) {
                    assertEqual(component.size, 1);
                }
            },

            testSCCOneDepth4AlternatingTree: function() {
                const depth = 4;
                // Each vertex induces its own strongly connected component.
                let {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "v")).makeFullBinaryTree(depth, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                // number of vertices in a full binary tree
                const numVertices = Math.pow(2, depth + 1) - 1;
                assertEqual(computedComponents.length, numVertices,
                    `We expected ${numVertices} component, instead got ${JSON.stringify(computedComponents)}`);

                for (const component of computedComponents) {
                    assertEqual(component.size, 1);
                }
            },

            testSCCAlternatingTreeAlternatingCycle: function() {
                // tree
                const depth = 3;
                const {vertices, edges} = graphGenerator(verticesEdgesGenerator(vColl, "t")).makeFullBinaryTree(depth, true);
                db[vColl].save(vertices);
                db[eColl].save(edges);

                // cycle
                const length = 5;
                const resultC = graphGenerator(verticesEdgesGenerator(vColl, "c")).makeAlternatingCycle(length);
                db[vColl].save(resultC.vertices);
                db[eColl].save(resultC.edges);

                const computedComponents = pregelRunSmallInstance("scc", graphName, { resultField: "result", store: true });
                assertEqual(computedComponents.length, Math.pow(2, depth + 1) - 1 + length,
                    `We expected ${Math.pow(2, depth + 1) - 1 + length} components, instead got ${JSON.stringify(computedComponents)}`);

                for (let component of computedComponents) {
                    assertEqual(component.size, 1);
                }
            },
        };
    };
}

exports.makeWCCTestSuite = makeWCCTestSuite;
exports.makeHeavyWCCTestSuite = makeHeavyWCCTestSuite;
exports.makeSCCTestSuite = makeSCCTestSuite;