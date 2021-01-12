/* jshint globalstrict:false, strict:false */
/* global assertTrue, start_pretty_print, stop_pretty_print, start_color_print, stop_color_print */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for client-specific functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');

function clientTestSuite () {
  'use strict';
  return {
    
    testIsArangod: function () {
      assertTrue(require("internal").isArangod());
    },

    testPrettyStart: function () {
      start_pretty_print();
    },

    testPrettyStop: function () {
      stop_pretty_print();
    },

    testColorStart: function () {
      start_color_print();
    },

    testColorStop: function () {
      stop_color_print();
    },
  };
}

jsunity.run(clientTestSuite);

return jsunity.done();
