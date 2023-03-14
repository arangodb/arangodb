/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ INTO var = expr
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
const db = require("@arangodb").db;
const internal = require("internal");

function optimizerCollectExpressionTestSuite () {
  var assertFailingQuery = function (query, code) {
    if (code === undefined) {
      code = internal.errors.ERROR_QUERY_PARSE.code;
    }
    try {
      AQL_EXECUTE(query);
      fail();
    } catch (err) {
      assertEqual(code, err.errorNum);
    }
  };

  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ gender: (i % 2 === 0 ? "m" : "f"), age: 11 + (i % 71), value: i });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },
    
    testCollectMethods : function () {
      ["hash", "sorted"].forEach((method) => {
        const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO docs = i OPTIONS { method: '" + method + "' } SORT gender RETURN { gender, docs }";

        let results = AQL_EXECUTE(query);
        assertEqual(2, results.json.length);

        let row = results.json[0];
        assertEqual("f", row.gender);
        assertEqual(500, row.docs.length);
        for (let i = 0; i < 500; ++i) {
          assertEqual("f", row.docs[i].gender);
        }
        
        row = results.json[1];
        assertEqual("m", row.gender);
        assertEqual(500, row.docs.length);
        for (let i = 0; i < 500; ++i) {
          assertEqual("m", row.docs[i].gender);
        }

        let nodes = AQL_EXPLAIN(query).plan.nodes.filter((n) => n.type === 'CollectNode');
        assertEqual(1, nodes.length);
        assertEqual(method, nodes[0].collectOptions.method);
      });
    },
    
    testCollectMethodsIntoExpression : function () {
      ["hash", "sorted"].forEach((method) => {
        const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO docs = { age: i.age, gender: i.gender, value: RAND() } OPTIONS { method: '" + method + "' } SORT gender RETURN { gender, docs }";

        let results = AQL_EXECUTE(query);
        assertEqual(2, results.json.length);

        let row = results.json[0];
        assertEqual("f", row.gender);
        assertEqual(500, row.docs.length);
        for (let i = 0; i < 500; ++i) {
          assertEqual("f", row.docs[i].gender);
          assertTrue(row.docs[i].age >= 11 && row.docs[i].age <= 81);
          assertTrue(row.docs[i].value >= 0 && row.docs[i].value < 1);
        }
        
        row = results.json[1];
        assertEqual("m", row.gender);
        assertEqual(500, row.docs.length);
        for (let i = 0; i < 500; ++i) {
          assertEqual("m", row.docs[i].gender);
          assertTrue(row.docs[i].age >= 11 && row.docs[i].age <= 81);
          assertTrue(row.docs[i].value >= 0 && row.docs[i].value < 1);
        }

        let nodes = AQL_EXPLAIN(query).plan.nodes.filter((n) => n.type === 'CollectNode');
        assertEqual(1, nodes.length);
        assertEqual(method, nodes[0].collectOptions.method);
      });
    },
    
    testCollectMethodsIntoAndAgreegate : function () {
      ["hash", "sorted"].forEach((method) => {
        const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender AGGREGATE minAge = MIN(i.age), maxAge = MAX(i.age) INTO docs = { age: i.age, gender: i.gender, value: RAND() } OPTIONS { method: '" + method + "' } SORT gender RETURN { gender, minAge, maxAge, docs }";

        let results = AQL_EXECUTE(query);
        assertEqual(2, results.json.length);

        let row = results.json[0];
        assertEqual("f", row.gender);
        assertEqual(11, row.minAge);
        assertEqual(81, row.maxAge);
        assertEqual(500, row.docs.length);
        for (let i = 0; i < 500; ++i) {
          assertEqual("f", row.docs[i].gender);
          assertTrue(row.docs[i].age >= 11 && row.docs[i].age <= 81);
          assertTrue(row.docs[i].value >= 0 && row.docs[i].value < 1);
        }
        
        row = results.json[1];
        assertEqual("m", row.gender);
        assertEqual(500, row.docs.length);
        assertEqual(11, row.minAge);
        assertEqual(81, row.maxAge);
        for (let i = 0; i < 500; ++i) {
          assertEqual("m", row.docs[i].gender);
          assertTrue(row.docs[i].age >= 11 && row.docs[i].age <= 81);
          assertTrue(row.docs[i].value >= 0 && row.docs[i].value < 1);
        }

        let nodes = AQL_EXPLAIN(query).plan.nodes.filter((n) => n.type === 'CollectNode');
        assertEqual(1, nodes.length);
        assertEqual(method, nodes[0].collectOptions.method);
      });
    },

    testReference : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO docs = i RETURN { gender: gender, age: MIN(docs[*].age) }";

      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(11, results.json[0].age);
      assertEqual("m", results.json[1].gender);
      assertEqual(11, results.json[1].age);
    },

    testSubAttribute : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO ages = i.age RETURN { gender: gender, age: MIN(ages) }";

      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(11, results.json[0].age);
      assertEqual("m", results.json[1].gender);
      assertEqual(11, results.json[1].age);
    },

    testConst : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO values = 1 RETURN { gender: gender, values: values }";

      let values = [ ];
      for (let i = 0; i < 500; ++i) {
        values.push(1);
      }

      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(values, results.json[0].values);
      assertEqual("m", results.json[1].gender);
      assertEqual(values, results.json[1].values);
    },

    testDocAttribute : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO values = i.value RETURN { gender: gender, values: values }";

      let f = [ ], m = [ ];
      for (let i = 0; i < 500; ++i) {
        m.push(i * 2);
        f.push((i * 2) + 1);
      }

      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(f, results.json[0].values.sort(function (l, r) { return l - r; }));
      assertEqual("m", results.json[1].gender);
      assertEqual(m, results.json[1].values.sort(function (l, r) { return l - r; }));
    },

    testCalculation : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO names = (i.gender == 'f' ? 'female' : 'male') RETURN { gender: gender, names: names }";
      
      let m = [ ], f = [ ];
      for (let i = 0; i < 500; ++i) {
        m.push('male');
        f.push('female');
      }

      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(f, results.json[0].names);
      assertEqual("m", results.json[1].gender);
      assertEqual(m, results.json[1].names);
    },

    testIntoWithGrouping : function () {
      const query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO g RETURN { gender: gender, ages: g[*].i.age }";
      
      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[0].ages[i] === 'number');
      }
      assertEqual("m", results.json[1].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[1].ages[i] === 'number');
      }
    },

    testIntoWithGroupingInSubquery : function () {
      const query = "LET values = (FOR i IN " + c.name() + " COLLECT gender = i.gender INTO g RETURN { gender: gender, ages: g[*].i.age }) RETURN values";
      
      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json[0].length);
      assertEqual("f", results.json[0][0].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[0][0].ages[i] === 'number');
      }
      assertEqual("m", results.json[0][1].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[0][1].ages[i] === 'number');
      }
    },

    testIntoWithGroupingInSupportFollowingFor : function () {
      const query = "LET values = (FOR i IN " + c.name() + " COLLECT gender = i.gender INTO g RETURN { gender: gender, ages: g[*].i.age }) FOR i IN values RETURN i";
      
      let results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[0].ages[i] === 'number');
      }
      assertEqual("m", results.json[1].gender);
      for (let i = 0; i < 500; ++i) {
        assertTrue(typeof results.json[1].ages[i] === 'number');
      }
    },

    testIntoWithOutVariableUsedInAssignment : function () {
      assertFailingQuery("FOR doc IN [{ age: 1, value: 1 }, { age: 1, value: 2 }, { age: 2, value: 1 }, { age: 2, value: 2 }] COLLECT v1 = doc.age, v2 = doc.value INTO g = v1 RETURN [v1,v2,g]", internal.errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code);
      assertFailingQuery("FOR doc IN [{ age: 1, value: 1 }, { age: 1, value: 2 }, { age: 2, value: 1 }, { age: 2, value: 2 }] COLLECT v1 = doc.age AGGREGATE v2 = MAX(doc.value) INTO g = v2 RETURN [v1,v2,g]", internal.errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code);
    },

    testMultiCollectWithConstExpression : function () {
      let query = `
        LET values = [ {time:1}, {time:1}, {time:2}, {time:2}, {time:3}, {time:4}, {time:2}, {time:3}, {time:6} ]
        FOR p1 IN values
          COLLECT t = FLOOR(p1.time / 2) AGGREGATE m = MAX(p1.time)
          FOR p2 IN values
            FILTER m != p2.time
            COLLECT q = 0 INTO qs = p2
            RETURN {q}
      `;
      let results = AQL_EXECUTE(query);
      assertEqual([ { q: 0 } ], results.json);
      
      query = `
        LET values = [ {time:1}, {time:1}, {time:2}, {time:2}, {time:3}, {time:4}, {time:2}, {time:3}, {time:6} ]
        FOR p1 IN values
          COLLECT t = FLOOR(p1.time / 2) AGGREGATE m = MAX(p1.time)
          FOR p2 IN values
            FILTER m == p2.time
            COLLECT q = 0 INTO qs = p2
            RETURN {q}
      `;
      results = AQL_EXECUTE(query);
      assertEqual([ { q: 0 } ], results.json);
    },

    testCollectWithEmptyInput : function () {
      let query = "FOR v IN [] COLLECT w = 1 INTO x RETURN w";
      let results = AQL_EXECUTE(query);
      assertEqual([ ], results.json);
      
      query = "FOR v IN [] COLLECT w = 1 INTO x RETURN {x}";
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json);
      
      query = "FOR v IN [] COLLECT w = 1 INTO x RETURN x";
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json);
      
      query = "FOR v IN 1..3 FILTER v > 9 COLLECT w = 1 INTO x RETURN w";
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json);
      
      query = "FOR v IN 1..3 FILTER v > 9 COLLECT w = v INTO x RETURN w";
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json);
      
      query = "FOR v IN 1..3 COLLECT w = v INTO x RETURN w";
      results = AQL_EXECUTE(query);
      assertEqual([ 1, 2, 3 ], results.json);
    },

    // regression test.
    // COLLECT should invalidate all previous variables.
    // COLLECT INTO should collect all those variables into the group variable.
    // There is one exception: Unless the COLLECT itself is on the top level,
    // top-level variables are *neither* invalidated *nor* collected.
    testCollectAfterCollect : function () {
      const query = `
        LET foo = "bar"
        FOR x IN 1..3
          COLLECT p = 'p' INTO groups
          COLLECT o = 'o' INTO ngroups
          RETURN {foo, ngroup: FIRST(ngroups)}
      `;

      const expectedResults = [
        {
          foo: 'bar',
          ngroup:
            {
              p: 'p',
              groups: [{x: 1}, {x: 2}, {x: 3}],
            },
        },
      ];

      const results = AQL_EXECUTE(query);
      assertEqual(expectedResults, results.json);
    },
     
  };
}

jsunity.run(optimizerCollectExpressionTestSuite);

return jsunity.done();
