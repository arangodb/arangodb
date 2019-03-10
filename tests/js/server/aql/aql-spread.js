/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertException */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, spread operator
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

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;

function spreadSuite () {
  return {
    setUp : function () {},

    tearDown : function () {},
    
    testSpreadOnNonArray : function ()  {
      let query = `LET value = "abc" RETURN [1, 2, ...value]`;
      let res = db._query(query).toArray()[0];
      assertEqual([1, 2], res);
      
      query = `LET value = NOOPT("abc") RETURN [1, 2, ...value]`;
      res = db._query(query).toArray()[0];
      assertEqual([1, 2], res);
      
      query = `LET value = {} RETURN [1, 2, ...value]`;
      res = db._query(query).toArray()[0];
      assertEqual([1, 2], res);
      
      query = `LET value = NOOPT({}) RETURN [1, 2, ...value]`;
      res = db._query(query).toArray()[0];
      assertEqual([1, 2], res);
    },
    
    testSpreadArrayAssemblingMixed : function ()  {
      let query = `LET values = [ 1, 2, 3, 4 ] RETURN ['a', 'b', ...values, 'c']`;
      let res = db._query(query).toArray()[0];
      assertEqual(["a", "b", 1, 2, 3, 4, "c"], res);
      
      query = `LET values = NOOPT([ 1, 2, 3, 4 ]) RETURN ['a', 'b', ...values, 'c']`;
      res = db._query(query).toArray()[0];
      assertEqual(["a", "b", 1, 2, 3, 4, "c"], res);
    },
    
    testSpreadArrayAssemblingJustSpread : function ()  {
      let query = `LET values = [ 1, 2, 3, 4 ] RETURN [...values]`;
      let res = db._query(query).toArray()[0];
      assertEqual([1, 2, 3, 4], res);
      
      query = `LET values = NOOPT([ 1, 2, 3, 4 ]) RETURN [...values]`;
      res = db._query(query).toArray()[0];
      assertEqual([1, 2, 3, 4], res);
    },
    
    testSpreadArrayAssemblingMultiUsage : function ()  {
      let query = `LET values = [ 1, 2, 3, 4 ] RETURN ['a', ...values, 'b', ...values, 'c', ...values]`;
      let res = db._query(query).toArray()[0];
      assertEqual(["a", 1, 2, 3, 4, "b", 1, 2, 3, 4, "c", 1, 2, 3, 4], res);
      
      query = `LET values = NOOPT([ 1, 2, 3, 4 ]) RETURN ['a', ...values, 'b', ...values, 'c', ...values]`;
      res = db._query(query).toArray()[0];
      assertEqual(["a", 1, 2, 3, 4, "b", 1, 2, 3, 4, "c", 1, 2, 3, 4], res);
    },
    
    testSpreadOnNonObject : function ()  {
      let query = `LET value = "abc" RETURN { a: 1, b: 2, ...value }`;
      let res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
      
      query = `LET value = NOOPT("abc") RETURN { a: 1, b: 2, ...value }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
      
      query = `LET value = [] RETURN { a: 1, b: 2, ...value }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
      
      query = `LET value = NOOPT([]) RETURN { a: 1, b: 2, ...value }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
    },
    
    testSpreadObjectAssemblingJustSpread : function ()  {
      let query = `LET values = { a: 1, b: 2 } RETURN { ...values }`;
      let res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
      
      query = `LET values = NOOPT({ a: 1, b: 2 }) RETURN { ...values }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2}, res);
    },
    
    testSpreadObjectAssemblingMixed : function ()  {
      let query = `LET values = { a: 1, b: 2 } RETURN { c: 3, d: 4, ...values, e: 5 }`;
      let res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2, c: 3, d: 4, e: 5 }, res);
      
      query = `LET values = NOOPT({ a: 1, b: 2 }) RETURN { c: 3, d: 4, ...values, e: 5 }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2, c: 3, d: 4, e: 5 }, res);
    },
    
    testSpreadObjectAssemblingMultiUsage : function ()  {
      let query = `LET values = { a: 1, b: 2 } RETURN { c: 3, ...values, d: 4, ...values, e: 5, ...values }`;
      let res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2, c: 3, d: 4, e: 5 }, res);
      
      query = `LET values = { a: 1, b: 2 } RETURN { c: 3, ...values, d: 4, ...values, e: 5, ...values }`;
      res = db._query(query).toArray()[0];
      assertEqual({a: 1, b: 2, c: 3, d: 4, e: 5 }, res);
    },
    
    testSpreadInFunctionCall : function ()  {
      let query = `LET foxx = ["quick", "brown", "foxx"] RETURN CONCAT("the", ...foxx)`;
      let res = db._query(query).toArray()[0];
      assertEqual("thequickbrownfoxx", res);
      
      query = `LET foxx = NOOPT(["quick", "brown", "foxx"]) RETURN CONCAT("the", ...foxx)`;
      res = db._query(query).toArray()[0];
      assertEqual("thequickbrownfoxx", res);
      
      query = `LET foxx = { a: "quick", b: "brown", c: "foxx" } RETURN CONCAT("the", ...foxx)`;
      res = db._query(query).toArray()[0];
      assertEqual("the", res);
      
      query = `LET foxx = NOOPT({ a: "quick", b: "brown", c: "foxx" }) RETURN CONCAT("the", ...foxx)`;
      res = db._query(query).toArray()[0];
      assertEqual("the", res);
    },
    
    testSpreadInvalidNumberOfArgumentsForFunctionCall : function ()  {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = ["quick", 2, 10, 11, 12] RETURN SUBSTRING(...values)`);
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = NOOPT(["quick", 2, 10, 11, 12]) RETURN SUBSTRING(...values)`);
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = {a: "quick", b: 2, c: 10} RETURN SUBSTRING(...values)`);
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = NOOPT({a: "quick", b: 2, c: 10}) RETURN SUBSTRING(...values)`);
    },
    
    testSpreadNonArrayInFunctionCall : function ()  {
      let query = `LET foxx = "abc" RETURN CONCAT("the", ...foxx)`;
      let res = db._query(query).toArray()[0];
      assertEqual("the", res);
      
      query = `LET foxx = NOOPT("abc") RETURN CONCAT("the", ...foxx)`;
      res = db._query(query).toArray()[0];
      assertEqual("the", res);
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = "abc" RETURN SUBSTRING(...values)`);
      
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, 
                       `LET values = NOOPT("abc") RETURN SUBSTRING(...values)`);
    },
    
    testSpreadWithRuntimeValues : function ()  {
      let query = `FOR i IN 1..4 LET values = 1..i RETURN SUM([...values])`;
      let res = db._query(query).toArray();
      assertEqual([1, 3, 6, 10], res);
      
      query = `FOR i IN 1..4 LET values = 1..i RETURN SUM([5, ...values, 15])`;
      res = db._query(query).toArray();
      assertEqual([21, 23, 26, 30], res);
    }, 
    
  };
}

jsunity.run(spreadSuite);

return jsunity.done();
