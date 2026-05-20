/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;

function objectSplicingSuite () {
  return {
    testObjectSplicingSpread: function () {
      let query = `LET o = { a: 1, b: 2 } RETURN { c: 3, ...o }`;
      let res = db._query(query).toArray();
      assertEqual([{ c: 3, a: 1, b: 2 }], res);
    },
    testObjectSplicingLaterOverrides: function () {
      let query = `LET o = { a: 1, b: 2 } RETURN { ...o, a: 9 }`;
      let res = db._query(query).toArray();
      assertEqual([{ a: 9, b: 2 }], res);
    },
    testObjectSplicingEarlierOverriddenBySpread: function () {
      let query = `LET o = { a: 5 } RETURN { a: 1, ...o }`;
      let res = db._query(query).toArray();
      assertEqual([{ a: 5 }], res);
    },
    testObjectSplicingNonObjectNoOp: function () {
      let query = `LET x = 7 RETURN { ...x }`;
      let res = db._query(query).toArray();
      assertEqual([{}], res);
    },
    testObjectSplicingNullNoOp: function () {
      let query = `LET x = null RETURN { ...x }`;
      let res = db._query(query).toArray();
      assertEqual([{}], res);
    },
    testObjectSplicingEmptyObject: function () {
      let query = `LET o = {} RETURN { ...o, x: 1 }`;
      let res = db._query(query).toArray();
      assertEqual([{ x: 1 }], res);
    },
    testObjectSplicingTwoSpreads: function () {
      let query = `LET a = { u: 1 } LET b = { v: 2 } RETURN { ...a, ...b }`;
      let res = db._query(query).toArray();
      assertEqual([{ u: 1, v: 2 }], res);
    },
    testObjectSplicingWithDynamicKeyTwoSpreads: function () {
      let query = ` LET a = "prefix" LET x = {name: "Rahul", prefixname: "Lanka" } LET y = { [CONCAT(a, "name")]: "newprefix"} RETURN [{...x, ...y}] `;
      let res = db._query(query).toArray();
      assertEqual([[{ "name" : "Rahul", "prefixname" : "newprefix" }]], res);
    },
    testObjectSplicingEarlierOverriddenTwoSpreads: function () {
      let query = `LET x = { name: "Rahul", age: 21 } LET y = { "name" : "Sam", "age" : 35 } RETURN { ...x, ...y }`;
      let res = db._query(query).toArray();
      assertEqual([{ name: "Sam", age: 35 }], res);
    },
    testObjectSplicingTwoSpreadsAndOneNormal: function () {
      let query = `LET x = { name: "Pavani",  age: 35, city: "Hyderabad"  } LET y = { city: "Chennai"} LET z = {name: "Rahul",  age: 21, city: "Delhi" } RETURN {... x,...y, z}`;
      let res = db._query(query).toArray();
      assertEqual([ { "name" : "Pavani", "age" : 35, "city" : "Chennai", "z" : { "name" : "Rahul", "age" : 21, "city" : "Delhi"  } } ], res);
    },
    testObjectSplicingOneSpreadOneNormalAndOneSpread: function () {
      let query = `LET x = { name: "Pavani",  age: 35, city: "Hyderabad"  } LET y = { city: "Chennai"} LET z = {name: "Rahul",  age: 21, city: "Delhi" } RETURN {...x, y, ...z}`;
      let res = db._query(query).toArray();
      assertEqual([ { "name" : "Rahul", "age" : 21, "city" : "Delhi", "y" : {"city" : "Chennai" }} ], res);
    },
    testObjectSplicingTwoSpreadOneObjectLiteral: function () {
      const {aql, db, errors} = require("@arangodb");
      let query = `LET x = { name: "Pavani",  age: 35, city: "Hyderabad"  } LET y = { city: "Delhi"} RETURN {... x,...y, {city: "Chennai"}}`;
      try{
        let res = db._query(query).toArray();
      } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_QUERY_PARSE.code);
      }
    },
    testObjectSplicingTwoSpreadOnePropertyDefinition: function () {
      let query = `LET x = { name: "Pavani",  age: 35, city: "Hyderabad"  } LET y = { city: "Delhi"} RETURN {... x,...y, city: "Chennai"}`;
      let res = db._query(query).toArray();
      assertEqual([ { "name" : "Pavani", "age" : 35, "city" : "Chennai" } ], res);  
    },
    testObjectSplicingThreeSpreadWithOverriddenProperty: function () {
      let query = `LET x = { name: "Pavani",  age: 35, city: "Hyderabad"  } LET y = { city: "Chennai"} LET z = {name: "Rahul",  age: 21, city: "Delhi" } RETURN {...x, ...y, ...z}`;
      let res = db._query(query).toArray();
      assertEqual([ { "name" : "Rahul", "age" : 21, "city" : "Delhi" } ], res);  
    },
    testLastKeyWinWithDuplicateDynamicKey: function () {
      let query = `FOR i IN 1..3 RETURN {[CONCAT("x",i)]: 0, x2: 1}`;
      let res = db._query(query).toArray();
      assertEqual([ { "x1" : 0, "x2" : 1 }, { "x2" : 1 }, { "x3" : 0, "x2" : 1 }], res);  
    },
    testCalculatedKeyCollidingWithStaticKey: function () {
      let query = `LET a = "x" RETURN {   [a]: 1,   x: 2 }`;
      let res = db._query(query).toArray();
      assertEqual([ { "x" : 2 }], res);  
    },
    testStaticKeyCollidingWithCalculatedKeyLater: function () {
      let query = `LET a = "x" RETURN {   x: 1,   [a]: 2 }`;
      let res = db._query(query).toArray();
      assertEqual([ { "x" : 2 }], res);
    },
    testTwoCalculatedKeysResolvingToSameValue: function () {
      let query = `RETURN { [CONCAT("x", 1)]: 1, ["x1"]: 2}`;
      let res = db._query(query).toArray();
      assertEqual([ { "x1" : 2 }], res);
    },
    testNestedObjectLiterals: function () {
      let query = `RETURN { outer: {    x: 1,    x: 2  } }`;
      let res = db._query(query).toArray();
      assertEqual([ { "outer" : { "x" : 2 } }], res);
    }
  };
}

jsunity.run(objectSplicingSuite);
return jsunity.done();
