/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, fail, Buffer */

////////////////////////////////////////////////////////////////////////////////
/// @brief test filesystem functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
/// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test attributes
////////////////////////////////////////////////////////////////////////////////

function SleepSuite () {
  'use strict';
  return {

    testExists : function () {
      var tempName;
      let sleepFor = 1;
      if (process.env.hasOwnProperty('SLEEP_FOR')) {
        sleepFor = parseInt(process.env['SLEEP_FOR']);
      }
      internal.sleep(sleepFor);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SleepSuite);

return jsunity.done();

