/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, assertTrue, assertFalse, arango */

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
const explainer = require("@arangodb/aql/explainer");

function arrayRegexTestSuite () {
  const values = ["foo", "bar", "baz"];

  return {
    testAnyRegexMatch : function () {
      let result = db._query('RETURN @values ANY =~ "^ba"', { values }).toArray();
      assertEqual([ true ], result);
    },

    testAllRegexMatch : function () {
      let result = db._query('RETURN @values ALL =~ "^ba"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testNoneRegexMatch : function () {
      let result = db._query('RETURN @values NONE =~ "^ba"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testAnyRegexNonMatch : function () {
      let result = db._query('RETURN @values ANY !~ "^ba"', { values }).toArray();
      assertEqual([ true ], result);
    },

    testAllRegexNonMatch : function () {
      let result = db._query('RETURN ["foo", "bat", "baz"] ALL !~ "b[aeiou]{2}"').toArray();
      assertEqual([ true ], result);
    },

    testNoneRegexNonMatch : function () {
      let result = db._query('RETURN @values NONE !~ "^ba"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testAtLeastRegexMatch : function () {
      let result = db._query(
        'RETURN ["foo1", "bar2", "baz3baz"] AT LEAST(2) =~ "\\\\w\\\\d$"'
      ).toArray();
      assertEqual([ true ], result);

      result = db._query(
        'RETURN ["foo1", "bar2", "baz3baz"] AT LEAST(3) =~ "\\\\w\\\\d$"'
      ).toArray();
      assertEqual([ false ], result);
    },

    testNoneRegexMatchExample : function () {
      let result = db._query(
        'RETURN ["foo", "bat", "baz"] NONE =~ "b[aeiou]{2}"'
      ).toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAllRegexMatch : function () {
      let result = db._query('RETURN [] ALL =~ "x.*"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAnyRegexMatch : function () {
      let result = db._query('RETURN [] ANY =~ "x.*"').toArray();
      assertEqual([ false ], result);
    },

    testEmptyArrayNoneRegexMatch : function () {
      let result = db._query('RETURN [] NONE =~ "x.*"').toArray();
      assertEqual([ true ], result);
    },

    testNonArrayRegexMatch : function () {
      let result = db._query('RETURN "foo" ANY =~ "f.*"').toArray();
      assertEqual([ false ], result);
    },

    testBindParameters : function () {
      let result = db._query('RETURN @values ANY =~ @pattern', {
        values: values,
        pattern: "^ba"
      }).toArray();
      assertEqual([ true ], result);
    },

    testEquivalentBooleanExpansion : function () {
      let shorthand = db._query('RETURN @values ANY =~ "^ba"', { values }).toArray();
      let expansion = db._query(
        'RETURN @values[? ANY FILTER CURRENT =~ "^ba"]', { values }
      ).toArray();
      assertEqual(shorthand, expansion);
    },

    testEquivalentAtLeastBooleanExpansion : function () {
      let shorthand = db._query(
        'RETURN ["foo1", "bar2", "baz3baz"] AT LEAST(2) =~ "\\\\w\\\\d$"'
      ).toArray();
      let expansion = db._query(
        'RETURN ["foo1", "bar2", "baz3baz"][? AT LEAST(2) FILTER CURRENT =~ "\\\\w\\\\d$"]'
      ).toArray();
      assertEqual(shorthand, expansion);
    },

    testAllRegexMatchTrue : function () {
      let result = db._query('RETURN ["bar", "baz"] ALL =~ "^ba"').toArray();
      assertEqual([ true ], result);
    },

    testNoneRegexMatchTrue : function () {
      let result = db._query('RETURN ["foo"] NONE =~ "^ba"').toArray();
      assertEqual([ true ], result);
    },

    testNoneRegexNonMatchTrue : function () {
      let result = db._query('RETURN ["bar", "baz"] NONE !~ "^ba"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAllRegexNonMatch : function () {
      let result = db._query('RETURN [] ALL !~ "x.*"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAnyRegexNonMatch : function () {
      let result = db._query('RETURN [] ANY !~ "x.*"').toArray();
      assertEqual([ false ], result);
    },

    testEmptyArrayNoneRegexNonMatch : function () {
      let result = db._query('RETURN [] NONE !~ "x.*"').toArray();
      assertEqual([ true ], result);
    },

    testEmptyArrayAtLeastRegexNonMatch : function () {
      let result = db._query('RETURN [] AT LEAST(0) !~ "x.*"').toArray();
      assertEqual([ true ], result);

      result = db._query('RETURN [] AT LEAST(1) !~ "x.*"').toArray();
      assertEqual([ false ], result);
    },

    testAtLeastRegexNonMatch : function () {
      let result = db._query('RETURN @values AT LEAST(2) !~ "^x"', { values }).toArray();
      assertEqual([ true ], result);

      result = db._query('RETURN @values AT LEAST(3) !~ "^b"', { values }).toArray();
      assertEqual([ false ], result);
    },

    testRegexTestFunctionCall : function () {
      let result = db._query('RETURN REGEX_TEST("foo", "f.*")').toArray();
      assertEqual([ true ], result);
    },

    testFilterContext : function () {
      let result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a", "b"] ANY =~ "^a" RETURN x'
      ).toArray();
      assertEqual([ 1, 2, 3 ], result);

      result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a", "b"] NONE =~ "^a" RETURN x'
      ).toArray();
      assertEqual([], result);
    },

    testAtLeastFilterContext : function () {
      let result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a1", "b2", "ab"] AT LEAST(2) =~ "\\\\w\\\\d$" RETURN x'
      ).toArray();
      assertEqual([ 1, 2, 3 ], result);

      result = db._query(
        'FOR x IN [1, 2, 3] FILTER ["a1", "b2", "ab"] AT LEAST(3) =~ "\\\\w\\\\d$" RETURN x'
      ).toArray();
      assertEqual([], result);
    },

    testAtLeastBindParameters : function () {
      let result = db._query('RETURN @values AT LEAST(@count) =~ @pattern', {
        values: values,
        count: 2,
        pattern: "^ba"
      }).toArray();
      assertEqual([ true ], result);
    },

    testAtLeastZeroRegexMatch : function () {
      let result = db._query('RETURN @values AT LEAST(0) =~ "^x"', { values }).toArray();
      assertEqual([ true ], result);

      result = db._query('RETURN [] AT LEAST(0) =~ "x.*"').toArray();
      assertEqual([ true ], result);
    },

    testNonArrayScalarTypesRegexMatch : function () {
      let result = db._query('RETURN null ANY =~ "f.*"').toArray();
      assertEqual([ false ], result);

      result = db._query('RETURN 123 ANY =~ "1.*"').toArray();
      assertEqual([ false ], result);

      result = db._query('RETURN {} ANY =~ ".*"').toArray();
      assertEqual([ false ], result);
    },

    testNotArrayAllRegexNonMatch : function () {
      // "abcde" matches ^.{5}$ so ALL !~ is false; NOT => true
      let result = db._query(
        'RETURN NOT ["abcde", "bar"] ALL !~ "^.{5}$"'
      ).toArray();
      assertEqual([ true ], result);
    },

    testAtLeastRegexExplain : function () {
      let text = explainer.explain({
        query: 'RETURN ["foo", "bar", "baz"] AT LEAST(2) =~ "^ba"'
      }, { colors: false }, false);
      assertTrue(text.includes('AT LEAST(2)'));
      assertTrue(text.includes('FILTER'));
      assertFalse(text.includes('FILTER AT LEAST'));
    },

    testNotArrayAllRegexExplain : function () {
      let text = explainer.explain({
        query: 'RETURN NOT ["foo", "bar", "baz"] ALL !~ "^.{5}$"'
      }, { colors: false }, false);
      assertFalse(text.includes('false[?'));
      assertTrue(text.includes('! ['));
    }
  };
}

jsunity.run(arrayRegexTestSuite);

return jsunity.done();
