/*jshint globalstrict:false, strict:false, maxlen:4000 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

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
    baseTests(),
    suite,
    "_mixed",
    [
      // TODO: these tests need to actually be fixed!
      "testAnalyzers",
      // no auth support here
      "testUsers",
      // single server tests that weren't supported in cluster:
      "testTransactionCommit",
      "testTransactionUpdate",
      "testTransactionAbort",

      // cluster tests:
      "testDatabaseProperties",
      "testShards",
      "testReplicationFactor",

      // enterprise graph tests: (TODO: should they be able to be run in single?)
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
