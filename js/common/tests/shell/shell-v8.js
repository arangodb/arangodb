/*jshint globalstrict:false, strict:false */
/*jshint -W034, -W098, -W016 */
/*eslint no-useless-computed-key: "off"*/
/*global assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test v8
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
var console = require("console");

////////////////////////////////////////////////////////////////////////////////
/// @brief test crash resilience
////////////////////////////////////////////////////////////////////////////////

function V8CrashSuite () {
  'use strict';

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
/// @brief https://bugs.chromium.org/p/v8/issues/detail?id=5033
////////////////////////////////////////////////////////////////////////////////

    testTypeFeedbackOracle : function () {
      "use strict";

      // code below is useless, but it triggered a segfault in V8 code optimization
      var test = function () {
        var t = Date.now();
        var o = {
          ['p'] : 1,
          t
        };
      };

      for (var n = 0; n < 100000; n++) {
        test();
      }

      // simply need to survive the above code      
      assertTrue(true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief https://bugs.chromium.org/p/v8/issues/detail?id=5033
////////////////////////////////////////////////////////////////////////////////

    testTypeFeedbackOracle2 : function () {
      "use strict";

      // code below is useless, but it triggered a segfault in V8 code optimization
      var test = function () {
        var random = 0 | Math.random() * 1000;
        var today = Date.now();
        var o = {
          ['prop_' + random] : today,
          random,
          today
        };
      };

      console.time('test');
      for (var n = 0; n < 100000; n++) {
        test();
      }
      console.timeEnd('test');

      // simply need to survive the above code      
      assertTrue(true);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(V8CrashSuite);

return jsunity.done();
