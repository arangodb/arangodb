/*jshint globalstrict:false, strict:false, maxlen : 4000 */

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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Wilfried Goesgens
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var internal = require("internal");
var jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(
  internal.pathForTesting('server'),
  'dump',
  'dump-test.inc');
const baseTests = require("internal").load(base);

jsunity.run(function dump_cluster_testsuite() {

  let suite = {};
  let enterpriseTests = [];
  if (!internal.isEnterprise()) {
    enterpriseTests = [
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
      "testSmartGraphAttribute",
      "testLatestId"
    ];
  }
  deriveTestSuite(
    baseTests(),
    suite,
    "_cluster",
    [
      "testUsers",
      "testKeygenAutoInc",
      "testTransactionCommit",
      "testTransactionUpdate",
      "testTransactionAbort",
    ].concat(enterpriseTests)
  );

  return suite;

});

return jsunity.done();
