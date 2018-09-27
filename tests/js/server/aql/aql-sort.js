/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

// returns some values of array around index (from index-before to index+after)
// as a comma separated string. Appends/prepends "..." when not printing the
// whole array.
function arrayStringExcerpt(array, index, before, after) {
  const lastIdx = array.length - 1;
  const from = Math.max(index-before, 0);
  const to = Math.min(index+after, lastIdx);
  const slice = array.slice(from, to).join(', ');
  const prefix = from > 0
    ? '..., '
    : '';
  const suffix = to < lastIdx
    ? ', ...'
    : '';
  return prefix + slice + suffix;
}

function sortTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 8 });

      for (var i = 0; i < 11111; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test without index
////////////////////////////////////////////////////////////////////////////////

    testSortNoIndex: function() {
      const result =
          AQL_EXECUTE(`FOR doc IN ${c.name()} SORT doc.value RETURN doc.value`)
              .json;
      assertEqual(11111, result.length);

      let last = -1;
      for (let i = 0; i < result.length; ++i) {
        let slice = arrayStringExcerpt(result, i, 2, 1);
        assertTrue(result[i] > last, `failure at index ${i}: ${slice}`);
        last = result[i];
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test with index
////////////////////////////////////////////////////////////////////////////////

    testSortSkiplist: function() {
      c.ensureIndex({type: "skiplist", fields: ["value"]});
      const result =
          AQL_EXECUTE(`FOR doc IN ${c.name()} SORT doc.value RETURN doc.value`)
              .json;
      assertEqual(11111, result.length);

      let last = -1;
      for (let i = 0; i < result.length; ++i) {
        let slice = arrayStringExcerpt(result, i, 2, 1);
        assertTrue(result[i] > last, `failure at index ${i}: ${slice}`);
        last = result[i];
      }
    }

  };
}

jsunity.run(sortTestSuite);

return jsunity.done();

