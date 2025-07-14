/*jshint globalstrict:true, strict:true, esnext: true */
/*global print */

"use strict";

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Tobias Gödderz
/// @author Heiko Kernbach
// //////////////////////////////////////////////////////////////////////////////

const {protoGraphs} = require('@arangodb/testutils/aql-graph-traversal-generic-graphs.js');
const {testsByGraph, metaTests} = require('@arangodb/testutils/aql-graph-traversal-generic-tests.js');
const console = require('console');
const jsunity = require("jsunity");
const _ = require("lodash");

function graphTraversalGenericGeneralGraphStandaloneSuite() {
  let testGraphs = _.fromPairs(_.keys(protoGraphs).map(x => [x, {}]));
  _.each(protoGraphs, function (protoGraph) {
    _.each(protoGraph.prepareSingleServerGraph(), function (testGraph) {
      testGraphs[protoGraph.name()][`${testGraph.name()}_SingleServerGeneralGraph`] = testGraph;
    });
  });

  const suite = {
    setUpAll: function () {
      try {
        const startTime = Date.now();
        const numGraphs = _.sumBy(_.values(testGraphs), g => _.keys(g).length);
        const graphsByType = _.mapValues(testGraphs, g => _.keys(g).length);

        print(`[${new Date().toISOString()}] ========== GRAPH CREATION STARTED ==========`);
        print(`[${new Date().toISOString()}] Creating ${numGraphs} graphs total:`);
        _.each(graphsByType, (count, type) => {
          if (count > 0) {
            print(`[${new Date().toISOString()}]   - ${type}: ${count} graphs`);
          }
        });

        let graphCount = 0;
        _.each(testGraphs, function (graphs, protoGraphName) {
          if (_.keys(graphs).length > 0) {
            print(`[${new Date().toISOString()}] Creating ${protoGraphName} graphs (${_.keys(graphs).length} variations)...`);
          }
          _.each(graphs, function (graph) {
            graphCount++;
            print(`[${new Date().toISOString()}] [${graphCount}/${numGraphs}] Starting: ${graph.name()}`);
            graph.create();
            print(`[${new Date().toISOString()}] [${graphCount}/${numGraphs}] Completed: ${graph.name()}`);
          });
        });

        const totalTime = Date.now() - startTime;
        print(`[${new Date().toISOString()}] ========== GRAPH CREATION COMPLETED ==========`);
        print(`[${new Date().toISOString()}] All ${numGraphs} graphs created successfully in ${totalTime}ms (${(totalTime/1000).toFixed(1)}s)`);
      } catch (e) {
        console.error(e);
        console.error(e.stack);
        throw(e);
      }
    },

    tearDownAll: function () {
      try {
        _.each(testGraphs, function (graphs) {
          _.each(graphs, function (graph) {
            graph.drop();
          });
        });
      } catch (e) {
        console.error(e);
        console.error(e.stack);
        throw(e);
      }
    }
  };

  _.each(metaTests, (test, testName) => {
    suite[testName] = test;
  });

  _.each(testsByGraph, function (localTests, graphName) {
    let graphs = testGraphs[graphName];
    _.each(localTests, function (test, testName) {
      _.each(graphs, function (graph, name){
        suite[`${testName}_${name}`] = function () {
          test(graph);
        };
      });
    });
  });

  return suite;
}

jsunity.run(graphTraversalGenericGeneralGraphStandaloneSuite);

return jsunity.done();
