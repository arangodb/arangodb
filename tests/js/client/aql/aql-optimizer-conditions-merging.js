/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual */

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

const jsunity = require('jsunity');
const db = require('@arangodb').db;

function OptimizerConditionsMergingSuite () {
  const cn = "UnitTestsCollection";
  const minValue = 0;
  const maxValue = 1002;

  let runTests = (queries, conditions) => {
    const bounds = [1, 9, 10, 99, 100, 999, 1000];
    bounds.forEach((v) => {
      assertTrue(v >= minValue);
      assertTrue(v <= maxValue);
    });

    for (let i = 0; i < queries; ++i) {
      let min = minValue;
      let minInclusive = true;
      let max = maxValue;
      let maxInclusive = true;
      let minSet = false;
      let maxSet = false;
      let operations = [];
      while (operations.length < conditions) {
        let v = bounds[Math.floor(Math.random() * bounds.length)];
        let r = Math.random();
        let op;
        if (r >= 0.75) {
          op = '>=';
          if (v > min) {
            min = v;
            minInclusive = true;
          }
          minSet = true;
        } else if (r >= 0.5) {
          op = '>';
          if (v >= min) {
            min = v;
            minInclusive = false;
          }
          minSet = true;
        } else if (r >= 0.25) {
          op = '<=';
          if (v < max) {
            max = v;
            maxInclusive = true;
          }
          maxSet = true;
        } else {
          op = '<';
          if (v <= max) {
            max = v;
            maxInclusive = false;
          }
          maxSet = true;
        }
        operations.push(`doc.value ${op} ${v}`);
      }

      let query = `FOR doc IN ${cn} FILTER ${operations.join(' && ')} SORT doc.value RETURN doc.value`;
      let res = db._query(query).toArray();
      let buildContext = () => {
        return { query, min, max, minInclusive, maxInclusive, minSet, maxSet, result: JSON.stringify(res) };
      };
      let emptyCondition = false;
      if (min > max) {
        emptyCondition = true;
        assertEqual(0, res.length, buildContext());
      } else if (min === max && (!minInclusive || !maxInclusive)) {
        emptyCondition = true;
        assertEqual(0, res.length, buildContext());
      } else if (min + 1 === max && !minInclusive && !maxInclusive) {
        assertEqual(0, res.length, buildContext());
      } else {
        assertNotEqual(0, res.length);
        let v = res[0];
        if (minInclusive) {
          assertTrue(v >= min, { v, min }, buildContext());
        } else {
          assertTrue(v > min, { v, min }, buildContext());
        }
        v = res[res.length - 1];
        if (maxInclusive) {
          assertTrue(v <= max, { v, max }, buildContext());
        } else {
          assertTrue(v < max, { v, max }, buildContext());
        }
      }

      let nodes = db._createStatement(query).explain().plan.nodes.filter((n) => n.type === 'IndexNode');
      assertEqual(1, nodes.length);
      let condition = nodes[0].condition;
      if (emptyCondition) {
        assertEqual({}, condition, buildContext());
      } else {
        assertEqual('n-ary or', condition.type, condition, buildContext());
        let subNodes = condition.subNodes;
        assertEqual(1, subNodes.length);
        assertEqual('n-ary and', subNodes[0].type, condition, buildContext());
        subNodes = subNodes[0].subNodes;
        let expectedConditions = 0;
        if (minSet) {
          let cond = subNodes[expectedConditions];
          if (min === max) {
            assertEqual('compare ==', cond.type);
          } else if (minInclusive) {
            assertEqual('compare >=', cond.type);
          } else {
            assertEqual('compare >', cond.type);
          }
          assertEqual(2, cond.subNodes.length, cond);
          assertEqual('attribute access', cond.subNodes[0].type);
          assertEqual('value', cond.subNodes[1].type);
          assertEqual(min, cond.subNodes[1].value);
          ++expectedConditions;
        } 
        if (maxSet && (min !== max)) {
          let cond = subNodes[expectedConditions];
          if (maxInclusive) {
            assertEqual('compare <=', cond.type);
          } else {
            assertEqual('compare <', cond.type);
          }
          assertEqual(2, cond.subNodes.length, cond);
          assertEqual('attribute access', cond.subNodes[0].type);
          assertEqual('value', cond.subNodes[1].type);
          assertEqual(max, cond.subNodes[1].value);
          ++expectedConditions;
        }
        assertEqual(expectedConditions, subNodes.length, buildContext());
      }
    }
  };

  return {
    setUpAll : function () {
      let c = db._create(cn);
      let docs = [];
      for (let i = minValue; i <= maxValue; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value"] });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testConditionsMerging2Conditions : function () {
      runTests(100, 2);
    },
    
    testConditionsMerging3Conditions : function () {
      runTests(100, 3);
    },
    
    testConditionsMerging4Conditions : function () {
      runTests(100, 4);
    },

  };
}

jsunity.run(OptimizerConditionsMergingSuite);

return jsunity.done();
