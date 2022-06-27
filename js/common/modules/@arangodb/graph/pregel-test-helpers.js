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
let graphGenerator = require("@arangodb/graph/graphs-generation").graphGenerator;

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

function makeWCCTestSuite(generatorBase) {

    const checkWCCDisjointComponents = function (parameters) {
    // Get the result of Pregel's WCC.
    const status = pregelRunSmallInstance("wcc", generatorBase.graphName, {resultField: "result", store: true});
    assertEqual(status.state, "done", "Pregel Job did never succeed.");

    // Now test the result.
    const query = `FOR v IN ${generatorBase.vColl}
                     COLLECT component = v.result INTO vertices = { _key: v._key, label: v.label }
                     RETURN { component, vertices }`;
    const computedComponents = db._query(query).toArray();

    assertEqual(computedComponents.length, Object.keys(parameters).length,
        `We expected ${Object.keys(parameters).length} components, instead got ${JSON.stringify(computedComponents)}`);

    console.log(`Compz: ${computedComponents}`);
    for (const c in computedComponents) {
      let component = computedComponents[c];
      assertTrue(component.vertices.length > 0, `Found an empty component: ${JSON.stringify(component)}`);
      let label = component.vertices[0].label;
      let expectedSize = parameters[label].size;

      assertEqual(component.vertices.length, expectedSize,
            `Expected ${expectedSize} for component with label ${label}, found ${component.vertices.length}`);
      // Test that the labels of vertices in the computed component are all the same.
      for (let vertex of component.vertices) {
          assertEqual(vertex.label, label,
                `Connected component contains vertices with at least two different labels, ${vertex.label} and  ${label}`);
        }
    }
    };
    return function () {
        'use strict';

        return {
          setUp: generatorBase.setUp,
          tearDown: generatorBase.tearDown,
          testWCCFourDirectedCycles: function () {
            let parameters = { "C_2": { size: 2 },
                               "C_10": { size: 10 },
                               "C_5": { size: 5 },
                               "C_23": { size: 23 } };

            for (const [label, options] of Object.entries(parameters)) {
              generatorBase.store(graphGenerator(generatorBase, label, label).makeDirectedCycle(options.size));
            }

            checkWCCDisjointComponents(parameters);
          }
        };
    };
}

exports.makeWCCTestSuite = makeWCCTestSuite;
// exports.makeHeavyWCCTestSuite = makeHeavyWCCTestSuite;
