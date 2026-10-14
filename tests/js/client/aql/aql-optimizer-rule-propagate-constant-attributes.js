/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
const db = require('internal').db;

function optimizerRulePropagateConstantAttributesTestSuite () {
  const ruleName = "propagate-constant-attributes";
  const outer = "UnitTestsPropagateConstantOuter";
  const inner = "UnitTestsPropagateConstantInner";

  const runWithAndWithoutRule = (query) => {
    const withRule = db._query(query).toArray();
    const withoutRule = db._query(query, null, {optimizer: {rules: ["-" + ruleName]}}).toArray();
    return [withRule, withoutRule];
  };

  return {
    setUpAll : function () {
      db._drop(outer);
      db._drop(inner);
      db._create(outer);
      db._create(inner);
      db[outer].insert([
        {site: "a", key: "k1"},
        {site: "a", key: "k2"},
      ]);
      db[inner].insert([
        {key: "k1"},
        {key: "k2"},
      ]);
    },

    tearDownAll : function () {
      db._drop(outer);
      db._drop(inner);
    },

    testPropagatesConstantIntoSubquery : function () {
      const query = `FOR o IN ${outer} FILTER o.key == 'k2' LET sub = (FOR i IN ${inner} FILTER i.key == o.key RETURN i.key) SORT o.key RETURN [o.key, sub]`;
      const plan = db._createStatement(query).explain().plan;
      assertNotEqual(-1, plan.rules.indexOf(ruleName));

      const [withRule, withoutRule] = runWithAndWithoutRule(query);
      assertEqual([["k2", ["k2"]]], withRule);
      assertEqual(withoutRule, withRule);
    },

    testComparisonOnFunctionResultIsNotAConstantAttribute : function () {
      const queries = [
        `FOR o IN ${outer} FILTER o.site == 'a' FILTER FIRST(FOR i IN ${inner} FILTER i.key == o.key RETURN i).key == 'k2' RETURN o.key`,
        `FOR o IN ${outer} FILTER o.site == 'a' FILTER (FOR i IN ${inner} FILTER i.key == o.key RETURN i)[0].key == 'k2' RETURN o.key`,
        `FOR o IN (FOR x IN ${outer} FILTER x.site == 'a' RETURN x) FILTER FIRST(FOR i IN ${inner} FILTER i.key == o.key RETURN i).key == 'k2' RETURN o.key`,
      ];

      queries.forEach((query) => {
        const [withRule, withoutRule] = runWithAndWithoutRule(query);
        assertEqual(["k2"], withRule, query);
        assertEqual(withoutRule, withRule, query);
      });
    },
  };
}

jsunity.run(optimizerRulePropagateConstantAttributesTestSuite);

return jsunity.done();
