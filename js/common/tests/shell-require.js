/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertNull, module */

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

var jsunity = require('jsunity');
var Module = require('module');
var path = require('path');

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for 'require'
////////////////////////////////////////////////////////////////////////////////

function RequireTestSuite () {
  'use strict';
  var console = require('console');
  function createTestPackage () {
    var test = new Module(Module.globalPaths[0] + '/.test');

    test.exports.print = require('internal').print;

    test.exports.assert = function (guard, message) {
      console.log('running test %s', message);
      assertEqual(guard !== false, true);
    };

    return test;
  }

  return {

    setUp() {
      Module.globalPaths.unshift(path.resolve('./js/common/test-data/modules'));
      Module.globalPaths.unshift(path.resolve(Module.globalPaths[0], 'node_modules'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown() {
      Module.globalPaths.splice(0, 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test requiring JSON
////////////////////////////////////////////////////////////////////////////////

    testRequireJson() {
      var test = createTestPackage();
      var data = test.require('./test-data');

      assertTrue(data.hasOwnProperty('tags'));
      assertEqual(['foo', 'bar', 'baz'], data.tags);

      assertTrue(data.hasOwnProperty('author'));
      assertEqual({'first': 'foo', 'last': 'bar'}, data.author);

      assertTrue(data.hasOwnProperty('number'));
      assertEqual(42, data.number);

      assertTrue(data.hasOwnProperty('sensible'));
      assertFalse(data.sensible);

      assertTrue(data.hasOwnProperty('nullValue'));
      assertNull(data.nullValue);

      assertFalse(data.hasOwnProperty('schabernack'));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test coffee script execution
////////////////////////////////////////////////////////////////////////////////

    testRequireCoffeeScript() {
      var test = createTestPackage();
      var script = test.require('./coffee-test');

      assertTrue(script.hasOwnProperty('coffeeSquare'));
      assertEqual('function', typeof script.coffeeSquare);
      assertEqual(49, script.coffeeSquare(7));

      assertTrue(script.hasOwnProperty('coffeeValue'));
      assertEqual('string', typeof script.coffeeValue);
      assertEqual('test', script.coffeeValue);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test package loading
////////////////////////////////////////////////////////////////////////////////

    testPackage() {
      var test = createTestPackage();
      var m = test.require('./TestA');

      assertEqual(m.version, 'A 1.0.0');
      assertEqual(m.x.version, 'x 1.0.0');
      assertEqual(m.B.version, 'B 2.0.0');
      assertEqual(m.C.version, 'C 1.0.0');
      assertEqual(m.C.B.version, 'B 2.0.0');
      assertEqual(m.D.version, 'D 1.0.0');

      assertEqual(m.B, m.C.B);

      var n = test.require('./TestB');

      assertEqual(n.version, 'B 1.0.0');
      assertNotEqual(n, m.B);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for 'commonjs'
////////////////////////////////////////////////////////////////////////////////

function CommonJSTestSuite () {
  'use strict';
  var console = require('console');
  var basePath = path.resolve('./js/common/test-data/modules/commonjs/tests/modules/1.0/');

  function createTestPackage (testPath) {

    var lib = path.resolve(basePath, testPath, '.fake');

    var test = new Module(lib);

    test.exports.print = require('internal').print;

    test.exports.assert = function (guard, message) {
      console.log('running test %s', message);
      assertEqual(guard !== false, true);
    };

    return test;
  }

  var tests = {

    setUp() {
      Module.globalPaths.unshift('.');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown() {
      Module.globalPaths.shift();
    }

  };

////////////////////////////////////////////////////////////////////////////////
/// @brief test module loading
////////////////////////////////////////////////////////////////////////////////

  [
    'absolute',
    'cyclic',
    'determinism',
    'exactExports',
    'hasOwnProperty',
    'method',
    'missing',
    'monkeys',
    'nested',
    'relative',
    'transitive'
  ].forEach(function (name) {
    tests[`testCommonJS${name.charAt(0).toUpperCase()}${name.slice(1)}`] = function () {
      Module.globalPaths[0] = path.resolve(basePath, name);
      var test = createTestPackage(name);
      test.require('./program');
    };
  });

  return tests;
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
