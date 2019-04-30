/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertTrue, assertFalse*/

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for iresearch usage
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

const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require("fs").join(require('internal').pathForTesting('common'),
  'aql', 'aql-view-arangosearch-cluster.inc');
const IResearchAqlTestSuite = require("internal").load(base);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(function IResearchAqlTestSuite_s1_r1() {
  let suite = {};

  deriveTestSuite(
    IResearchAqlTestSuite({ numberOfShards: 1, replicationFactor: 1 }),
    suite,
    "_OneShard"
  );

  // same order as for single-server (valid only for 1 shard case)
  suite.testInTokensFilterSortTFIDF_OneShard = function () {
    var result = require('internal').db._query(
      "FOR doc IN UnitTestsView SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'text_en'), 'text_en') OPTIONS { waitForSync : true } SORT TFIDF(doc) LIMIT 4 RETURN doc"
    ).toArray();

    assertEqual(result.length, 4);
    assertEqual(result[0].name, 'half');
    assertEqual(result[1].name, 'quarter');
    assertEqual(result[2].name, 'other half');
    assertEqual(result[3].name, 'full');
  };

  return suite;
});

return jsunity.done();
