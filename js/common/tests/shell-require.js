/*jshint globalstrict: false */
/*global require, assertEqual, assertTrue, assertFalse, assertNotEqual, assertNull, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the module and package require
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

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for "require"
////////////////////////////////////////////////////////////////////////////////

function RequireTestSuite () {
  "use strict";
  var console = require("console");
  function createTestPackage () {
    var test = module.createTestEnvironment("./js/common/test-data/modules");

    test.exports.print = require("internal").print;

    test.exports.assert = function (guard, message) {
      console.log("running test %s", message);
      assertEqual(guard !== false, true);
    };

    return test;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test requiring JSON
////////////////////////////////////////////////////////////////////////////////

    testRequireJson : function () {
      var test = createTestPackage();
      var data = test.require("test-data");

      assertTrue(data.hasOwnProperty("tags"));
      assertEqual([ "foo", "bar", "baz" ], data.tags);

      assertTrue(data.hasOwnProperty("author"));
      assertEqual({ "first" : "foo", "last" : "bar" }, data.author);

      assertTrue(data.hasOwnProperty("number"));
      assertEqual(42, data.number);

      assertTrue(data.hasOwnProperty("sensible"));
      assertFalse(data.sensible);

      assertTrue(data.hasOwnProperty("nullValue"));
      assertNull(data.nullValue);

      assertFalse(data.hasOwnProperty("schabernack"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test coffee script execution
////////////////////////////////////////////////////////////////////////////////

    testRequireCoffeeScript : function () {
      var test = createTestPackage();
      var script = test.require("coffee-test");

      assertTrue(script.hasOwnProperty("coffeeSquare"));
      assertEqual("function", typeof script.coffeeSquare);
      assertEqual(49, script.coffeeSquare(7));

      assertTrue(script.hasOwnProperty("coffeeValue"));
      assertEqual("string", typeof script.coffeeValue);
      assertEqual("test", script.coffeeValue);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test package loading
////////////////////////////////////////////////////////////////////////////////

    testPackage : function () {
      var test = createTestPackage();
      var m = test.require("TestA");

      assertEqual(m.version, "A 1.0.0");
      assertEqual(m.x.version, "x 1.0.0");
      assertEqual(m.B.version, "B 2.0.0");
      assertEqual(m.C.version, "C 1.0.0");
      assertEqual(m.C.B.version, "B 2.0.0");
      assertEqual(m.D.version, "D 1.0.0");

      assertEqual(m.B, m.C.B);

      var n = test.require("TestB");

      assertEqual(n.version, "B 1.0.0");
      assertNotEqual(n, m.B);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for "commonjs"
////////////////////////////////////////////////////////////////////////////////

function CommonJSTestSuite () {
  "use strict";
  var fs = require("fs");
  var console = require("console");

  function createTestPackage (testPath) {
    var lib = "./" + fs.join(
      "./js/common/test-data/modules/commonjs/tests/modules/1.0/",
      testPath);

    console.log(lib);

    var test = module.createTestEnvironment(lib);

    test.exports.print = require("internal").print;

    test.exports.assert = function (guard, message) {
      console.log("running test %s", message);
      assertEqual(guard !== false, true);
    };

    return test;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test module loading
////////////////////////////////////////////////////////////////////////////////

    testRequireCommonJS : function () {
      var i;
      var tests = [
        "absolute", "cyclic", "determinism", "exactExports", "hasOwnProperty",
        "method", "missing", "monkeys", "nested", "relative", "transitive" ];

      for (i = 0;  i < tests.length;  i++) {
        var name = tests[i];

        console.log("running CommonJS test '%s'", name);

        var test = createTestPackage(name);
        test.require("program");
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequireTestSuite);
jsunity.run(CommonJSTestSuite);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}"
// End:
