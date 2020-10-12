/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for crash handler
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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

var internal = require('internal');
var jsunity = require('jsunity');

function runSetup () {
  'use strict';
  // intentionall do nothing, so we will see a normal shutdown!
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testLogOutput: function () {
      let fs = require("fs");
      let crashFile = internal.env["crash-log"];

      assertTrue(fs.isFile(crashFile), crashFile);

      let platform = internal.platform;
      if (platform !== 'linux') {
        return;
      }

      // find all warnings, errors, fatal errors...
      let lines = fs.readFileSync(crashFile).toString().trim().split("\n").filter(function(line) {
        // we are interested in errors, warnings, fatal errors
        if (!line.match(/ (ERROR|WARNING|FATAL) \[[a-f0-9]+\] /)) {
          return false;
        }

        if (line.match(/\{(memory|performance)\}/)) {
          // ignore errors about memory/performance configuration
          return false;
        }
        
        if (line.match(/\[0458b\].*DO NOT USE IN PRODUCTION/i)) {
          // intentionally ignore DO NOT USE IN PRODUCTION line
          return false;
        }
        if (line.match(/\[bd666\].*=+/)) {
          // and its following lie ===============================
          return false;
        }

        return true;
      });

      // and expected that there are none
      assertEqual(0, lines.length, lines);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
