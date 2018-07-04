/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, subqueries in cluster
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const db = require("internal").db;
const c1 = "UnitTestSubQuery1";
const c2 = "UnitTestSubQuery2";
const c3 = "UnitTestSubQuery3";
const c4 = "UnitTestSubQuery4";
const c5 = "UnitTestSubQuery5";
const c6 = "UnitTestSubQuery6";
const c7 = "UnitTestSubQuery7";
const c8 = "UnitTestSubQuery8";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////


/**
 * @brief This suite is supposed to test subquery execution
 * in the cluster case. Planning for subqueries tend to be
 * complicated and thereby error prone.
 * 
 * NOTE: Please do not take the following AQLs as well designed
 * production queries or best practices. They are made artificially
 * complex for the internals.
 *
 * @return The TestStuie
 */
function clusterSubqueriesTestSuite () {
  const clean = () => {
    db._drop(c1);
    db._drop(c2);
    db._drop(c3);
    db._drop(c4);
    db._drop(c5);
    db._drop(c6);
    db._drop(c7);
    db._drop(c8);
  };

  return {

    setUp: function () {
      clean();
      db._create(c1, {numberOfShards: 4});
      db._create(c2, {numberOfShards: 4});
      db._create(c3, {numberOfShards: 4});
      db._create(c4, {numberOfShards: 4});
      db._create(c5, {numberOfShards: 4});
      db._create(c6, {numberOfShards: 4});
      db._create(c7, {numberOfShards: 4});
      db._create(c8, {numberOfShards: 4});
    },

    tearDown: clean,

    // This test validates that all input values `x` are
    // transportet to the input register of the subquery
    // This is done via successful initializeCursor calls
    testSimpleInitialzeCursor: function () {
      let docs = [];
      // We add 5 times each of the values 0 -> 19
      // In the query we will just use 1->10
      for (let i = 0; i < 20; ++i) {
        for (let j = 0; j < 5; ++j) {
          docs.push({foo: `bar${i}`});
        }
      }
      db[c1].save(docs);
      let q = `
        FOR x IN 1..10
          LET sub = (
            FOR y IN ${c1}
              FILTER y.foo == CONCAT("bar", TO_STRING(x))
              RETURN y
          )
          RETURN sub[*].foo
      `;
      let c = db._query(q).toArray();
      assertEqual(c.length, 10); // we have 10 values for X
      let seen = new Set();
      for (let d of c) {
        assertEqual(d.length, 5); // we have 5 values for each x
        let first = d[0]; 
        seen.add(first);
        for (let q of d) {
          assertEqual(q, first);
        }
      }
      assertEqual(seen.size, 10); // We have 10 different values
    },

    // Tests if all snippets are created correctly
    // in subquery case if both root nodes
    // end on DBServers
    testSnippetsRootDB: function () {
      let docsA = [];
      let docsB = [];
      let docsC = [];
      let fooVals = new Map();
      // We add 5 times each of the values 1 -> 10
      for (let i = 1; i < 11; ++i) {
        fooVals.set(`${i}`, 0);
        docsA.push({foo: `${i}`});
        for (let j = 0; j < 5; ++j) {
          docsB.push({foo: `${i}`});
          docsC.push({foo: `${i}`});
        }
      }
      db[c1].save(docsA);
      db[c2].save(docsB);
      db[c3].save(docsC);
  
      let q = `
        FOR a IN ${c1}
          LET sub = (
            FOR b IN ${c2}
              FILTER b.foo == a.foo
              SORT b._key // For easier check in result
              RETURN b
          )
          FOR c IN ${c3}
            FILTER c.foo == a.foo
            FILTER c.foo == sub[0].foo
            RETURN {
              a: a._key,
              b: sub[*]._key,
              c: c._key,
              foos: {
                a: a.foo,
                c: c.foo,
                b: sub[*].foo
              }
            }
      `;
      let c = db._query(q).toArray();
      assertEqual(c.length, 10 * 5);
      // For 10 A values we find 5 C values. Each sharing identical 5 B values.
      let seen = new Set();
      for (let d of c) {
        // We found all 5 elements in subquery
        assertEqual(d.b.length, 5);
        assertEqual(d.foos.b.length, 5);
        // Check that all a,b,c combinations are unique
        let key = d.a + d.b + d.c;
        assertFalse(seen.has(key)); // Every combination is unique
        seen.add(key);
        // Check that all foo values are correct
        assertEqual(d.foos.a, d.foos.c);
        for (let f of d.foos.b) {
          assertEqual(d.foos.a, f);
        }
        // Increase that we found this specific value.
        // We need to find exactly 5 of each
        fooVals.set(d.foos.a, fooVals.get(d.foos.a) + 1);
      }
      assertEqual(fooVals.size, 10); // we have 10 different foos
      for (let [key, value] of fooVals) {
        assertEqual(value, 5, `Found to few of foo: ${key}`);
      }
    },

    // Tests if all snippets are created correctly
    // in subquery case if both root nodes
    // end on Coordinator
    testSnippetsRootCoordinator: function () {
      let docsA = [];
      let docsB = [];
      let docsC = [];
      let fooVals = new Map();
      // We add 5 times each of the values 1 -> 10
      for (let i = 1; i < 11; ++i) {
        fooVals.set(`${i}`, 0);
        docsA.push({foo: `${i}`, val: "baz"});
        for (let j = 0; j < 5; ++j) {
          docsB.push({foo: `${i}`, val: "bar"});
          docsC.push({foo: `${i}`});
        }
      }
      db[c1].save(docsA);
      db[c2].save(docsB);
      db[c3].save(docsC);
      db[c4].save([
        {_key: "a", val: "baz"},
        {_key: "b", val: "bar"}
      ]);
  
      let q = `
        LET src = DOCUMENT("${c4}/a")
        FOR a IN ${c1}
          FILTER a.val == src.val
          LET sub = (
            LET src2 = DOCUMENT("${c4}/b")
            FOR b IN ${c2}
              FILTER b.foo == a.foo
              FILTER b.val == src2.val
              RETURN b
          )
          FOR c IN ${c3}
            FILTER c.foo == a.foo
            FILTER c.foo == sub[0].foo
            RETURN {
              a: a._key,
              b: sub[*]._key,
              c: c._key,
              foos: {
                a: a.foo,
                c: c.foo,
                b: sub[*].foo
              }
            }
      `;
      let c = db._query(q).toArray();
      assertEqual(c.length, 10 * 5);
      // For 10 A values we find 5 C values. Each sharing identical 5 B values.
      let seen = new Set();
      for (let d of c) {
        // We found all 5 elements in subquery
        assertEqual(d.b.length, 5);
        assertEqual(d.foos.b.length, 5);
        // Check that all a,b,c combinations are unique
        let key = d.a + d.b + d.c;
        assertFalse(seen.has(key)); // Every combination is unique
        seen.add(key);
        // Check that all foo values are correct
        assertEqual(d.foos.a, d.foos.c);
        for (let f of d.foos.b) {
          assertEqual(d.foos.a, f);
        }
        // Increase that we found this specific value.
        // We need to find exactly 5 of each
        fooVals.set(d.foos.a, fooVals.get(d.foos.a) + 1);
      }
      assertEqual(fooVals.size, 10); // we have 10 different foos
      for (let [key, value] of fooVals) {
        assertEqual(value, 5, `Found to few of foo: ${key}`);
      }
    },

    // Tests if we have a wierd
    // combination of a subquery in a subquery
    // and switching from Coordinator to DBServer
    // all the time.
    //
    // General IDEA:
    // We have a subquery node,
    // where the "main query" contains a subquery
    // and its "subquery" contains a subquery as well.
    // Filters use the allowed amount of objects within
    // the different scopes (e.g. in every subquery we
    // validate that we still have access to the main
    // query variables)
    testSubqueryInception: function () {
      let docsA = [];
      let docsB = [];
      let docsC = [];
      let docsD = [];
      let docsE = [];
      let docsF = [];
      let docsG = [];
      let numDocs = 200;

      for (let i = 0; i < numDocs; ++i) {
        docsA.push({val: `A${i}`});
        docsB.push({val: `B${i}`, valA: `A${i}`});
        docsC.push({val: `C${i}`, valA: `A${i}`, valB: `B${i}`});
        docsD.push({val: `D${i}`, valA: `A${i}`, valB: `B${i}`, valC: `C${i}`});
        docsE.push({val: `E${i}`, valA: `A${i}`, valB: `B${i}`, valC: `C${i}`, valD: `D${i}`});
        docsF.push({val: `F${i}`, valA: `A${i}`, valB: `B${i}`, valC: `C${i}`, valD: `D${i}`, valE: `E${i}`});
        docsG.push({val: `G${i}`, valA: `A${i}`, valB: `B${i}`, valC: `C${i}`, valD: `D${i}`, valE: `E${i}`, valF: `F${i}`});
      }

      db[c1].save(docsA);
      db[c2].ensureHashIndex("valA");
      db[c2].save(docsB);
      db[c3].ensureHashIndex("valA", "valB");
      db[c3].save(docsC);
      db[c4].ensureHashIndex("valA", "valB", "valC");
      db[c4].save(docsD);
      db[c5].ensureHashIndex("valA", "valB", "valC", "valD");
      db[c5].save(docsE);
      db[c6].ensureHashIndex("valA", "valB", "valC", "valD", "valE");
      db[c6].save(docsF);
      db[c7].ensureHashIndex("valA", "valB", "valC", "valF");
      db[c7].save(docsG);
 
      let q = `
        FOR a IN ${c1}
          LET s1 = (
            FOR b IN ${c2}
              FILTER b.valA == a.val
              RETURN b
          )
          FOR c IN ${c3}
            FILTER c.valB == s1[0].val
            FILTER c.valA == a.val
            LET s2 = (
              FOR d IN ${c4}
                FILTER d.valA == a.val
                FILTER d.valB == s1[0].val
                FILTER d.valC == c.val
                LET s3 = (
                  FOR e IN ${c5}
                    FILTER e.valA == a.val
                    FILTER e.valB == s1[0].val
                    FILTER e.valC == c.val
                    FILTER e.valD == d.val
                    RETURN e
                )
                FOR f IN ${c6}
                  FILTER f.valA == a.val
                  FILTER f.valB == s1[0].val
                  FILTER f.valC == c.val
                  FILTER f.valD == d.val
                  FILTER f.valE == s3[0].val
                  RETURN f
              )
              FOR g IN ${c7}
                FILTER g.valA == a.val
                FILTER g.valB == s1[0].val
                FILTER g.valC == c.val
                FILTER g.valF == s2[0].val
                RETURN g
      `;

      let c = db._query(q).toArray();
      // We expect numDocs result documents
      assertEqual(c.length, numDocs);
      let seen = new Set();
      for (let d of c) {
        seen.add(d.val);
      }
      // Check that we have numDocs distinct ones
      assertEqual(seen.size, numDocs);
      for (let i = 0; i < numDocs; ++i) {
        assertTrue(seen.has(`G${i}`));
      }
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(clusterSubqueriesTestSuite);

return jsunity.done();
