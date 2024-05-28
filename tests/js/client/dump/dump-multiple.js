/*jshint globalstrict:false, strict:false, maxlen:4000 */
/*global assertEqual, assertTrue, assertFalse */

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
/// @author Jan Steemann
/// @author Wilfried Goesgens
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(
  internal.pathForTesting('client'),
  'dump',
  'dump-test.inc');
const baseTests = require("internal").load(base);


jsunity.run(function dump_single_testsuite() {

  let suite = {};

  deriveTestSuite(
    baseTests({
      // testEmpty
      emptyIndexes: 1,
      // testMany
      manyIndexes: 1,
      manyCountStart: 0,
      manyCount: 100000,
      manyCountInc: 1,
      // testEdges
      edgesIndexCount: 2,
      edgeIndexLoopCount: 10,
      edgeIndexLoopInc: 1,
      // testRemoved
      removedCount: 9000,
      removedSingleIndices: 1,
      removedModulo: 10,
      // testIndexes
      indexesCount: 9,
      // testKeygenAutoInc
      keygenAutoInc: 42049,
      autoIncDocCount: 1001,
      // testKeygenPadded
      paddedDocCount: 1001,
      // testKeygenUuid
      uuidDocCount: 1001,
    }),
    suite,
    "_multiple",
    [
      // no auth support here
      "testUsers",
      // cluster tests:
      "testDatabaseProperties",
      "testShards",
      "testReplicationFactor",

      // TODO: should we also have these tests in this scenario?
      "testKeygenPadded",
      "testKeygenUuid",
      "testAnalyzers",
      "testIndexAnalyzerCollection",
      "testJobsAndQueues",
      "testReplicationFactor",

      // enterprise graph tests:
      "testVertices",
      "testVerticesAqlRead",
      "testVerticesAqlInsert",
      "testOrphans",
      "testOrphansAqlRead",
      "testOrphansAqlInsert",
      "testSatelliteCollections",
      "testSatelliteGraph",
      "testHiddenCollectionsOmitted",
      "testShadowCollectionsOmitted",
      "testEEEdges",
      "testEdgesAqlRead",
      "testEdgesAqlInsert",
      "testAqlGraphQueryOutbound",
      "testAqlGraphQueryAny",
      "testSmartGraphSharding",
      "testViewOnSmartEdgeCollection",
      "testSmartGraphAttribute"
    ]
  );

  return suite;

});

return jsunity.done();
