/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

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
const errors = internal.errors;

function getReturnCalculation(query) {
  let plan = db._createStatement({query: query}).explain();
  let nodes = plan.plan.nodes;
  let returnNode = nodes.find(n => n.type === 'ReturnNode');
  assertTrue(returnNode !== undefined);
  let calcId = returnNode.dependencies[0];
  let calc = nodes.find(n => n.id === calcId);
  assertTrue(calc !== undefined);
  return calc;
}

function expressionHasObjectSplice(expression) {
  if (expression.type === 'object splice') {
    return true;
  }
  if (expression.subNodes) {
    for (let subNode of expression.subNodes) {
      if (expressionHasObjectSplice(subNode)) {
        return true;
      }
    }
  }
  return false;
}

function assertFoldedConstantObject(query, expected) {
  assertEqual([expected], db._query(query).toArray());
  let calc = getReturnCalculation(query);
  assertEqual('json', calc.expressionType);
  assertEqual('object', calc.expression.type);
  assertFalse(expressionHasObjectSplice(calc.expression));
}

function assertNotFoldedObjectSplice(query) {
  let calc = getReturnCalculation(query);
  assertEqual('simple', calc.expressionType);
  assertTrue(expressionHasObjectSplice(calc.expression));
}

function assertFlattenedObjectLiteralSplice(query) {
  let calc = getReturnCalculation(query);
  assertEqual('simple', calc.expressionType);
  assertEqual('object', calc.expression.type);
  assertFalse(expressionHasObjectSplice(calc.expression));
}

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
      let data = db._query(query).data;
      assertEqual([{}], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_OBJECT_EXPECTED.code,
        data.extra.warnings[0].code);
      assertTrue(data.extra.warnings[0].message.includes('object splice'));
    },
    testObjectSplicingNonObjectBoolean: function () {
      let query = `LET x = true RETURN { ...x }`;
      let data = db._query(query).data;
      assertEqual([{}], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_OBJECT_EXPECTED.code,
        data.extra.warnings[0].code);
      assertTrue(data.extra.warnings[0].message.includes('object splice'));
    },
    testObjectSplicingNonObjectArray: function () {
      let query = `LET x = [] RETURN { ...x }`;
      let data = db._query(query).data;
      assertEqual([{}], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_OBJECT_EXPECTED.code,
        data.extra.warnings[0].code);
    },
    testObjectSplicingNull: function () {
      let query = `LET x = null RETURN { ...x }`;
      let data = db._query(query).data;
      assertEqual([{}], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_OBJECT_EXPECTED.code,
        data.extra.warnings[0].code);
      assertTrue(data.extra.warnings[0].message.includes('object splice'));
    },
    testObjectSplicingInlineNull: function () {
      let query = `RETURN { a: 1, ...null, c: 3 }`;
      let data = db._query(query).data;
      assertEqual([{ a: 1, c: 3 }], data.result);
      assertEqual(1, data.extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_OBJECT_EXPECTED.code,
        data.extra.warnings[0].code);
      assertTrue(data.extra.warnings[0].message.includes('object splice'));
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
    },
    testObjectWithDuplicateKey: function () {
      let query = `LET x = { foo: 1, bar: 2, foo: 3} RETURN x.foo`;
      let res = db._query(query).toArray();
      assertEqual([ 3 ], res);
    },
    testInnerObjectWithDuplicateKey: function () {
      let query = `LET xx= { outer: {  x: 1,  x: 2  }} RETURN xx.outer.x`;
      let res = db._query(query).toArray();
      assertEqual([ 2 ], res);
    },
    testSpreadMergeWithComputedPropertyNameusingPlusOperator: function () {
      let query = `LET a = "prefix" LET x = {name: "Chrome", prefixname: "Google" } LET y = { [a + "name"]: "GoogleChrome"} RETURN [{...x, ...y}]`;
      let res = db._query(query).toArray();
      assertEqual([[{ "name" : "Chrome", "prefixname" : "GoogleChrome" }]], res);
    },

    testConstantLetObjectSpliceFolding: function () {
      let query = `LET x = {foo:1, bar:2, baz:3} RETURN {...x, foo:2}`;
      assertFoldedConstantObject(query, {foo: 2, bar: 2, baz: 3});
    },

    testEmptyConstantObjectSpliceFolding: function () {
      assertFoldedConstantObject(`LET o = {} RETURN { ...o, x: 1 }`, {x: 1});
    },

    testEmptyInlineObjectLiteralSpliceFolding: function () {
      assertFoldedConstantObject(`RETURN { ...{}, x: 1 }`, {x: 1});
    },

    testInlineConstantSpliceLaterKeyWins: function () {
      assertFoldedConstantObject(`RETURN { ...{ a: 1 }, a: 99 }`, {a: 99});
    },

    testInlineConstantSpliceOverridesEarlierKey: function () {
      assertFoldedConstantObject(`RETURN { a: 1, ...{ a: 2 } }`, {a: 2});
    },

    testMultipleConstantObjectSpliceFolding: function () {
      let query = `LET a = { u: 1 } LET b = { v: 2 } RETURN { ...a, ...b, w: 3 }`;
      assertFoldedConstantObject(query, {u: 1, v: 2, w: 3});
    },

    testConstantObjectSpliceOverwritePrecedence: function () {
      let query = `LET o = { a: 1, b: 2 } RETURN { a: 9, ...o, b: 3 }`;
      assertFoldedConstantObject(query, {a: 1, b: 3});
    },

    testNestedConstantObjectSpliceFolding: function () {
      let query = `LET x = { a: 1 } RETURN { ...{ ...x, b: 2 }, c: 3 }`;
      assertFoldedConstantObject(query, {a: 1, b: 2, c: 3});
    },

    testNonConstantLetObjectSpliceNotFolded: function () {
      let query = `FOR i IN 1..1 LET x = { a: i } RETURN { ...x, b: 2 }`;
      assertEqual([{a: 1, b: 2}], db._query(query).toArray());
      assertNotFoldedObjectSplice(query);
    },

    testCalculatedAttributeWithConstantSplicesDoesNotCrash: function () {
      let query = `RETURN {
        ...{ a:1, b:2 },
        [ CONCAT("he","llo") ] : "world",
        c: {
          ...{ x:10, y:20 },
          z:30
        }
      }`;
      let expected = { a: 1, b: 2, hello: "world", c: { x: 10, y: 20, z: 30 } };
      assertEqual([expected], db._query(query).toArray());

      let plan = db._createStatement({
        query: query,
        options: { matchStatement: "experimental" }
      }).explain();
      assertTrue(plan.plan !== undefined);

      let calc = getReturnCalculation(query);
      assertEqual('simple', calc.expressionType);
      assertFalse(expressionHasObjectSplice(calc.expression));
    },

    testObjectLiteralSpliceFlattening: function () {
      assertFlattenedObjectLiteralSplice(
        `FOR i IN 1..10 RETURN { ...{ a: i }, b: 2 }`);
    },

    testObjectLiteralSpliceFlatteningPreservesOrder: function () {
      assertFlattenedObjectLiteralSplice(
        `FOR i IN 1..10 RETURN { x: 1, ...{ a: i }, y: 2 }`);
    },

    testMultipleObjectLiteralSplicesFlattening: function () {
      assertFlattenedObjectLiteralSplice(
        `FOR i IN 1..10 FOR j IN 1..10 RETURN { ...{ a: i }, ...{ b: j } }`);
    },

    testNestedObjectLiteralSpliceFlattening: function () {
      assertFlattenedObjectLiteralSplice(
        `FOR i IN 1..10 RETURN { ...{ a: i, c: { ...{ x: 1 }, y: 2 } }, b: 2 }`);
    },

    testObjectLiteralSpliceWithCalculatedAttributeFlattening: function () {
      assertFlattenedObjectLiteralSplice(
        `FOR i IN 1..10 RETURN { ...{ a: i }, [ CONCAT("he","llo") ] : "world" }`);
    },

    testVariableObjectSpliceNotFlattened: function () {
      let query = `FOR i IN 1..1 LET x = { a: i } RETURN { ...x, b: 2 }`;
      assertEqual([{a: 1, b: 2}], db._query(query).toArray());
      assertNotFoldedObjectSplice(query);
    },

    testFunctionObjectSpliceNotFlattened: function () {
      let query = `FOR i IN 1..10 RETURN { ...NOOPT({ a: i }), b: 2 }`;
      assertNotFoldedObjectSplice(query);
    }
  };
}

jsunity.run(objectSplicingSuite);
return jsunity.done();
