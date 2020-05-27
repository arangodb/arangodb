/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

let jsunity = require("jsunity");

function optimizerRuleTestSuite () {
  const ruleName = "simplify-conditions";

  // various choices to control the optimizer: 
  const paramNone     = { optimizer: { rules: [ "-all" ] } };
  const paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  const paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

    testRuleDisabled : function () {
      let queries = [
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[0]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[3]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[4]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[10]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-1]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-2]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-3]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-4]",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['0']",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['3']",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['4']",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['fffff']",
        "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['-2']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['a']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['d']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['d']['x']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['z']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['0']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['1']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['2']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['999']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.a",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.d",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.d.x",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.z",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, {}, paramNone);
        assertEqual([], result.plan.rules);
      });
    },

    testRuleNoEffect : function () {
      let queries = [
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT(0)]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT(3)]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT(-1)]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT('0')]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT('3')]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT('fffff')]",
        "LET data = [ 1, 2, 3, 4 ] RETURN data[NOOPT('-2')]",
        "LET data = [ 1, 2, 3, 4 ] FOR i IN 1..2 RETURN data[i]",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: 4 } } RETURN data[NOOPT('a')]",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: 4 } } RETURN data[NOOPT('d')]['x']",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: 4 } } RETURN data[NOOPT('d')][NOOPT('x')]",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: 4 } } RETURN data[NOOPT('2')]",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: 4 } } RETURN data[NOOPT('999')]",
        "LET data = { a: 1, b: 2, c: 3, [NOOPT('foo')] : 4 } RETURN data.z",
        "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', [NOOPT('foo')] : 4 } } RETURN data.d.z",
        "LET data = { a: RAND() } RETURN data.a",
        "LET data = { a: RAND(), b: RAND() } RETURN [data.a, data.b]",
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query, {}, paramEnabled);
        assertEqual([], result.plan.rules, query);
      });
    },

    testResults : function () {
      let queries = [ 
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[0]", 1 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[3]", 4 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[4]", null ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[10]", null ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-1]", 4 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-2]", 3 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-3]", 2 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-4]", 1 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data[-5]", null ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['0']", 1 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['3']", 4 ],
        [ "LET data = [ 1, 2, 3, NOEVAL(4) ] RETURN data['4']", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['a']", 1 ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['d']", { x: 'x', y: 'y', foo: 4 } ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['d']['x']", 'x' ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['z']", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['0']", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['1']", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['2']", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data['999']", null ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data['1']", 1 ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data[1]", 1 ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data['99']", 3 ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data[99]", 3 ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data['d']['2']", 'y' ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data.d.`2`", 'y' ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data.`d`.`2`", 'y' ],
        [ "LET data = { '1': 1, '2': 2, '99': 3, d: { '1': 'x', '2': 'y', '3': NOEVAL(4) } } RETURN data.d['2']", 'y' ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.a", 1 ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.d", { x: 'x', y: 'y', foo: 4 } ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.d.x", 'x' ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', foo: NOEVAL(4) } } RETURN data.z", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', [NOEVAL('foo')] : 4 } } RETURN data.z", null ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', [NOEVAL('foo')] : 4 } } RETURN data.b", 2 ],
        [ "LET data = { a: 1, b: 2, c: 3, d: { x: 'x', y: 'y', [NOEVAL('foo')] : 4 } } RETURN data.d.y", 'y' ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0], {}, paramEnabled);
        assertEqual([ ruleName ], result.plan.rules, query);
        result = AQL_EXECUTE(query[0], {}, paramEnabled).json[0];
        assertEqual(query[1], result, query);
      });
    },

    testIssue8108 : function () {
      let query = "FOR i IN 1..10 LET x = { y : i } COLLECT z = x.y._id INTO out RETURN 1";
      let result = AQL_EXECUTE(query).json;
      // the main thing here is that the query can be executed successfully and 
      // does not throw a runtime error "variable not found"
      assertEqual([ 1 ], result);
    },

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
