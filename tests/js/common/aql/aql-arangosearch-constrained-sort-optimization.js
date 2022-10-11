/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for  constrained sort arangosearch rule
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('server'),
             'aql', 'aql-arangosearch-constrained-sort-optimization.inc');
const ArangoSearchConstrainedSortTestSuite = require("internal").load(base);
const isCluster = require("internal").isCluster();

function arangoSearchConstrainedSortRuleTestSuite () {
  let suite = {};
  deriveTestSuite(
    ArangoSearchConstrainedSortTestSuite(isCluster, "UnitTestsDB", {}),
    suite,
    "_NonOneShard"
  );
  return suite;
}

jsunity.run(arangoSearchConstrainedSortRuleTestSuite);

return jsunity.done();