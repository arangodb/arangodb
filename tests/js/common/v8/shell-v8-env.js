/*jshint globalstrict:false, strict:false, sub: true */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual */

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
/// @author Wilfried Goesgens
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");


////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function EnvironmentSuite () {
  'use strict';
  var env = require("process").env;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief exists() - test if a file / directory exists
////////////////////////////////////////////////////////////////////////////////

    testEnv : function () {
      assertNotEqual(Object.keys(env).length, 0);

      assertFalse(env.hasOwnProperty('UNITTEST'));

      env['UNITTEST'] = 1;

      assertTrue(env.hasOwnProperty('UNITTEST'));

      assertEqual(env['UNITTEST'], 1);

      assertEqual(env.UNITTEST, 1);

      assertEqual(typeof env.UNITTEST, "string");

      delete env.UNITTEST;
      
      assertFalse(env.hasOwnProperty('UNITTEST'));

      assertEqual(typeof env.UNITTEST, "undefined");
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EnvironmentSuite);

return jsunity.done();

