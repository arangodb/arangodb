/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Hyderabad, India
// / Copyright 2004-2026 triAGENS GmbH, Hyderabad, India
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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function arrayLikeTestSuite () {
  const values = ["foo", "bar", "baz"];

  return {
    testAnyLike : function () {
      let result = db._query('RETURN @values ANY LIKE "b%"', { values }).toArray();
      assertEqual([ true ], result);
    },

    testAllLike : function () {
      let result = db._query('RETURN @values ALL LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testNoneLike : function () {
      let result = db._query('RETURN @values NONE LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testAnyNotLike : function () {
      let result = db._query('RETURN @values ANY NOT LIKE "b%"', { values }).toArray();
      assertEqual([ true ], result);
    },

    testAllNotLike : function () {
      let result = db._query('RETURN @values ALL NOT LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testNoneNotLike : function () {
      let result = db._query('RETURN @values NONE NOT LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testEmptyArrayAllLike : function () {
      let result = db._query('RETURN [] ALL LIKE "x%"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAnyLike : function () {
      let result = db._query('RETURN [] ANY LIKE "x%"').toArray();
      assertEqual([ false ], result);
    },

    testEmptyArrayNoneLike : function () {
      let result = db._query('RETURN [] NONE LIKE "x%"').toArray();
      assertEqual([ true ], result);
    },

    testNonArrayLike : function () {
      let result = db._query('RETURN "foo" ANY LIKE "f%"').toArray();
      assertEqual([ false ], result);
    },

    testBindParameters : function () {
      let result = db._query('RETURN @values ANY LIKE @pattern', {
        values: values,
        pattern: "b%"
      }).toArray();
      assertEqual([ true ], result);
    },

    testAtLeastLike : function () {
      let result = db._query('RETURN @values AT LEAST(2) LIKE "b%"', { values }).toArray();
      assertEqual([ true ], result);

      result = db._query('RETURN @values AT LEAST(3) LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testEquivalentBooleanExpansion : function () {
      let shorthand = db._query('RETURN @values ANY LIKE "b%"', { values }).toArray();
      let expansion = db._query('RETURN @values[? ANY FILTER CURRENT LIKE "b%"]', { values }).toArray();
      assertEqual(shorthand, expansion);
    },

    testAllLikeTrue : function () {
      let result = db._query('RETURN ["bar", "baz"] ALL LIKE "b%"').toArray();
      assertEqual([ true ], result);
    },

    testNoneLikeTrue : function () {
      let result = db._query('RETURN ["foo"] NONE LIKE "b%"').toArray();
      assertEqual([ true ], result);
    },

    testNoneNotLikeTrue : function () {
      let result = db._query('RETURN ["bar", "baz"] NONE NOT LIKE "b%"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAllNotLike : function () {
      let result = db._query('RETURN [] ALL NOT LIKE "x%"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAnyNotLike : function () {
      let result = db._query('RETURN [] ANY NOT LIKE "x%"').toArray();
      assertEqual([ false ], result);
    },

    testEmptyArrayNoneNotLike : function () {
      let result = db._query('RETURN [] NONE NOT LIKE "x%"').toArray();
      assertEqual([ true ], result);
    },

    testAtLeastNotLike : function () {
      let result = db._query('RETURN @values AT LEAST(2) NOT LIKE "x%"', { values }).toArray();
      assertEqual([ true ], result);

      result = db._query('RETURN @values AT LEAST(3) NOT LIKE "b%"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testLikeFunctionCall : function () {
      let result = db._query('RETURN LIKE("foo", "f%")').toArray();
      assertEqual([ true ], result);
    },

    testUnderscoreWildcard : function () {
      let result = db._query('RETURN ["bar", "baz"] ALL LIKE "ba_"').toArray();
      assertEqual([ true ], result);
    },

    testFilterContext : function () {
      let result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a", "b"] ANY LIKE "a%" RETURN x'
      ).toArray();
      assertEqual([ 1, 2, 3 ], result);

      result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a", "b"] NONE LIKE "a%" RETURN x'
      ).toArray();
      assertEqual([], result);
    }
  };
}

jsunity.run(arrayLikeTestSuite);

return jsunity.done();
