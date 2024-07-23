/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('client'),
  'aql', 'aql-optimizer-rule-views-sorted-indexes.inc');
const AqlOptimizerRuleViewsSortedIndexesTestSuite = require("internal").load(base);

function optimizerRuleArangoSearchTestSuite() {
  let suite = {};
  deriveTestSuite(
    AqlOptimizerRuleViewsSortedIndexesTestSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function optimizerRuleSearchAliasTestSuite() {
  let suite = {};
  deriveTestSuite(
    AqlOptimizerRuleViewsSortedIndexesTestSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(optimizerRuleArangoSearchTestSuite);
jsunity.run(optimizerRuleSearchAliasTestSuite);

return jsunity.done();
