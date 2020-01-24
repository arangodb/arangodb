/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, skiplist index queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSkiplistOverlappingTestSuite () {
  const rulesDisabled = {optimizer: {rules: ["-all"]}};
  const rulesEnabled = {optimizer: {rules: [""]}};

  let skiplist;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlSkiplist");
      skiplist = internal.db._create("UnitTestsAhuacatlSkiplist");
      skiplist.ensureSkiplist("a");

      let docs = [];
      for (var i = 0; i < 10000; ++i) {
        docs.push({a: i});
      }
      skiplist.insert(docs);
    },

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlSkiplist");
    },

    testLargeOverlappingRanges : function () {
      let query = `FOR x IN @@cn FILTER (1000 <= x.a && x.a < 2500) 
                                     || (1500 <= x.a && x.a < 3000)
                                     RETURN x`;

      let bindVars = {"@cn": skiplist.name()};
      assertEqual(2000, AQL_EXECUTE(query, bindVars, rulesDisabled).json.length);
      assertEqual(2000, AQL_EXECUTE(query, bindVars, rulesEnabled).json.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSkiplistOverlappingTestSuite);

return jsunity.done();

