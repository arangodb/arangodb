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
const vColl = "UnitTest_pregel_v";
const eColl = "UnitTest_pregel_e";

/**
 * Create disjoint components using parameters from componentGenerators, apply WCC on the graph and test
 * the result.
 * @param componentGenerators is a list of ComponentGenerator's. Expected that a ComponentGenerator
 *        has fields generator and parameters, the latter should be an object with the field length.
 */
const checkWCCDisjointComponents = function (parameters) {
    // Get the result of Pregel's WCC.
    const status = pregelRunSmallInstance("wcc", graphName, {resultField: "result", store: true});
    assertEqual(status.state, "done", "Pregel Job did never succeed.");

    // Now test the result.
    const query = `
                FOR v IN ${vColl}
                  COLLECT component = v.result INTO components
                  RETURN {component, components}
            `;
    const computedComponents = db._query(query).toArray();
    assertEqual(computedComponents.length, parameters.length,
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

function makeWCCTestSuite(generator) {
    return function () {
        'use strict';

        return {
          setUp: generator.setUp,
          tearDown: generator.tearDown,
          testWCCFourDirectedCycles: function () {
            parameters = [ { label: "C_2", size: 2 },
                           { label: "C_10", size: 10 },
                           { label: "C_5", size: 5  },
                           { label: "C_23", size: 23 } ];

            for (let p of parameters) {
              generator.store(generator.labeled(p.label).makeDirectedCycle(p.size));
            }

            checkWCCDisjointComponents(parameters);
          }
        };
    };
}

exports.makeWCCTestSuite = makeWCCTestSuite;
// exports.makeHeavyWCCTestSuite = makeHeavyWCCTestSuite;
