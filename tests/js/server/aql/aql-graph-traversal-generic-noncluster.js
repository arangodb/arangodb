/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

const {protoGraphs} = require('@arangodb/aql-graph-traversal-generic-graphs.js');
const {testsByGraph, metaTests} = require('@arangodb/aql-graph-traversal-generic-tests.js');

const jsunity = require("jsunity");
const _ = require("lodash");

function graphTraversalGenericGeneralGraphStandaloneSuite() {
  let testGraphs = _.fromPairs(_.keys(protoGraphs).map(x => [x, {}]));
  _.each(protoGraphs, function (protoGraph) {
    _.each(protoGraph.prepareSingleServerGraph(), function (testGraph) {
      testGraphs[protoGraph.name()][testGraph.name()] = testGraph;
    });
  });

  const suite = {
    setUpAll: function () {
      try {
        const numGraphs = _.sumBy(_.values(testGraphs), g => _.keys(g).length);
        console.info(`Creating ${numGraphs} graphs, this might take a few seconds.`);
        _.each(testGraphs, function (graphs) {
          _.each(graphs, function (graph) {
            graph.create();
          });
        });
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
      _.each(graphs, function (graph){
        suite[testName + '_' + graph.name()] = function () {
          test(graph);
        };
      });
    });
  });

  return suite;
}

jsunity.run(graphTraversalGenericGeneralGraphStandaloneSuite);

return jsunity.done();
