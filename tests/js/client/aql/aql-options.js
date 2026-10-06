/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail, print */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlOptionsTestSuite () {
  var errors = internal.errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test maxRuntime option
////////////////////////////////////////////////////////////////////////////////

    testMaxRuntime : function () {
      try {
        internal.db._query("LET x = SLEEP(10) RETURN 1", {} /*bind*/, { maxRuntime : 1} /*options*/);
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code);
      }
    },

    testMaxRuntimeStopsSlowRegexTest : function () {
      const q = 
        `RETURN REGEX_TEST(NOOPT(CONCAT(REPEAT("a", 28), "!")), "(a+)+$")`;
      try {
        internal.db._query(q, {}, { maxRuntime : 1 });
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code, q);
      }
    },

    testMaxRuntimeStopsSlowLike : function () {
      const q = 
        `RETURN LIKE(NOOPT(CONCAT(REPEAT("a", 160), "!")), "%a%a%a%a%b")`;
      try {
        internal.db._query(q, {}, { maxRuntime : 1});
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code, q);
      }
    },

    testMaxRuntimeStopsSlowRegexMatches : function () {
      const q = 
        `RETURN REGEX_MATCHES(NOOPT(CONCAT(REPEAT("a", 28), "!")), "(a+)+$")`;
      try {
        internal.db._query(q, {}, { maxRuntime : 1 });
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code, q);
      }
    },

    testMaxRuntimeStopsSlowRegexSplit : function () {
      const q = 
        `RETURN REGEX_SPLIT(NOOPT(CONCAT(REPEAT("a", 28), "!")), "(a+)+$")`;
      try {
        internal.db._query(q, {}, { maxRuntime : 1 });
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code, q);
      }
    },

    testMaxRuntimeStopsSlowRegexReplace : function () {
      const q = 
        `RETURN REGEX_REPLACE(NOOPT(CONCAT(REPEAT("a", 28), "!")), "(a+)+$", "x")`;
      try {
        internal.db._query(q, {}, { maxRuntime : 1 });
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_KILLED.code, q);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlOptionsTestSuite);

return jsunity.done();
