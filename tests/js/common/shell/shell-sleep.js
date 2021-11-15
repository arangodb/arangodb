/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertTrue, process */

////////////////////////////////////////////////////////////////////////////////
/// @brief test filesystem functions
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

const jsunity = require("jsunity");
const internal = require("internal");

function SleepSuite () {
  'use strict';
  return {
    
    testSleep : function () {
      let time = require("internal").time;
      let start = time();
      internal.sleep(1);
      let stop = time();
      assertTrue(stop - start > 0.99);
    },

    testSleepFor : function () {
      // this is not a proper test. its purpose is just to serve as an
      // example for invoking some test suite with memory profiling. this
      // is explained in README_maintainers.md
      let sleepFor = 0.01;
      if (process.env.hasOwnProperty('SLEEP_FOR')) {
        sleepFor = parseInt(process.env['SLEEP_FOR']);
      }
      internal.sleep(sleepFor);
    }
  };
}

jsunity.run(SleepSuite);

return jsunity.done();
