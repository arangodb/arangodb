/*jshint globalstrict:true, strict:true, esnext: true */
/*global assertTrue, instanceManager */

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
////////////////////////////////////////////////////////////////////////////////

const {protoGraphs} = require('@arangodb/testutils/aql-graph-traversal-generic-graphs.js');
const {testsByGraph, metaTests} = require('@arangodb/testutils/aql-graph-traversal-generic-tests.js');

const jsunity = require('jsunity');
const console = require('console');
const _ = require("lodash");
const internal = require("internal");

let getMetric = require('@arangodb/test-helper').getMetric;

function graphTraversalGenericGeneralGraphClusterSuite() {
  let testGraphs = _.fromPairs(_.keys(protoGraphs).map(x => [x, {}]));
  _.each(protoGraphs, function (protoGraph) {
    _.each(protoGraph.prepareGeneralGraphs(), function (testGraph) {
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

let beforeQueries;
let beforeTrxs;

function checkMetricsSuite() {
  return {
    testCheckMetrics : function() {
      internal.wait(0.5);  // Wait until metrics updated
      let afterQueries = getMetric(instanceManager.url, "arangodb_dirty_read_queries_total");
      let afterTrxs = getMetric(instanceManager.url, "arangodb_dirty_read_transactions_total");
      // The following checks that indeed dirty reads have been happening. The
      // tests execute well over a thousand queries, so we should be on the safe
      // side with the threshold 10, even if some tests might be removed in
      // the future.
      assertTrue(afterQueries - beforeQueries < 10);
      assertTrue(afterTrxs - beforeTrxs < 10);
    }
  };
}

// We want to verify that this testsuite runs without dirty reads, therefore
// we check some metrics before and after:
beforeQueries = getMetric(instanceManager.url, "arangodb_dirty_read_queries_total");
beforeTrxs = getMetric(instanceManager.url, "arangodb_dirty_read_transactions_total");

jsunity.run(graphTraversalGenericGeneralGraphClusterSuite);

jsunity.run(checkMetricsSuite);

return jsunity.done();
