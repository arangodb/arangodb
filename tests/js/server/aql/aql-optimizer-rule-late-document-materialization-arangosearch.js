/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for late document materialization arangosearch rule
///
/// @file
///
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
/// @author Andrei Lobov
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('server'),
             'aql', 'aql-optimizer-rule-late-document-materialization-arangosearch.inc');
const ArangoResearchLateMaterializationTestSuite = require("internal").load(base);
const isCluster = require("internal").isCluster();

function lateDocumentMaterializationArangoSearchRuleTestSuite () {
  let suite = {};
  deriveTestSuite(
    ArangoResearchLateMaterializationTestSuite(isCluster, "UnitTestsDB", {}),
    suite,
    "_NonOneShard"
  );
  return suite;
}

jsunity.run(lateDocumentMaterializationArangoSearchRuleTestSuite);

return jsunity.done();
