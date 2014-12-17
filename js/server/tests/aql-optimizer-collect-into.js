/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ INTO var = expr
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

var jsunity = require("jsunity");
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCollectExpressionTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 1000; ++i) {
        c.save({ gender: (i % 2 === 0 ? "m" : "f"), age: 11 + (i % 71), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test expression
////////////////////////////////////////////////////////////////////////////////

    testReference : function () {
      var query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO docs = i RETURN { gender: gender, age: MIN(docs[*].age) }";

      var results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(11, results.json[0].age);
      assertEqual("m", results.json[1].gender);
      assertEqual(11, results.json[1].age);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test expression
////////////////////////////////////////////////////////////////////////////////

    testSubAttribute : function () {
      var query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO ages = i.age RETURN { gender: gender, age: MIN(ages) }";

      var results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(11, results.json[0].age);
      assertEqual("m", results.json[1].gender);
      assertEqual(11, results.json[1].age);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test expression
////////////////////////////////////////////////////////////////////////////////

    testConst : function () {
      var query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO values = 1 RETURN { gender: gender, values: values }";

      var values = [ ];
      for (var i = 0; i < 500; ++i) {
        values.push(1);
      }

      var results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(values, results.json[0].values);
      assertEqual("m", results.json[1].gender);
      assertEqual(values, results.json[1].values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test expression
////////////////////////////////////////////////////////////////////////////////

    testDocAttribute : function () {
      var query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO values = i.value RETURN { gender: gender, values: values }";

      var f = [ ], m = [ ];
      for (var i = 0; i < 500; ++i) {
        m.push(i * 2);
        f.push((i * 2) + 1);
      }

      var results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(f, results.json[0].values.sort(function (l, r) { return l - r; }));
      assertEqual("m", results.json[1].gender);
      assertEqual(m, results.json[1].values.sort(function (l, r) { return l - r; }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test expression
////////////////////////////////////////////////////////////////////////////////

    testCalculation : function () {
      var query = "FOR i IN " + c.name() + " COLLECT gender = i.gender INTO names = (i.gender == 'f' ? 'female' : 'male') RETURN { gender: gender, names: names }";
      
      var m = [ ], f = [ ];
      for (var i = 0; i < 500; ++i) {
        m.push('male');
        f.push('female');
      }

      var results = AQL_EXECUTE(query);
      assertEqual(2, results.json.length);
      assertEqual("f", results.json[0].gender);
      assertEqual(f, results.json[0].names);
      assertEqual("m", results.json[1].gender);
      assertEqual(m, results.json[1].names);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCollectExpressionTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
