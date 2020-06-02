/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertMatch */

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
  let platform = internal.platform;
  if (platform !== 'linux') {
    // crash handler only available on Linux
    return;
  }
  // produces an assertion failure in the server
  internal.debugTerminate('CRASH-HANDLER-TEST-ASSERT');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testLogOutput: function () {
      // check if crash handler is turned off
      let envVariable = internal.env["ARANGODB_OVERRIDE_CRASH_HANDLER"];
      if (typeof envVariable === 'string') {
        envVariable = envVariable.trim().toLowerCase();
        if (!(envVariable === 'true' || envVariable === 'yes' || envVariable === 'on' || envVariable === 'y')) {
          return;
        }
      }

      let fs = require("fs");
      let crashFile = internal.env["crash-log"];

      assertTrue(fs.isFile(crashFile), crashFile);

      let platform = internal.platform;
      if (platform !== 'linux') {
        // crash handler only available on Linux
        return;
      }

      let lines = fs.readFileSync(crashFile).toString().split("\n").filter(function(line) {
        return line.match(/\{crash\}/);
      });
      assertTrue(lines.length > 0);

      // check message
      let line = lines.shift();
      assertMatch(/FATAL.*thread \d+.*caught unexpected signal 6.*assertion failed.*: a == 2/, line);
     
      // check debug symbols
      // it is a bit compiler- and optimization-level-dependent what
      // symbols we get
      let expected = [ /assertionFailure/, /TerminateDebugging/, /JS_DebugTerminate/ ];
      let matches = 0;
      lines.forEach(function(line) {
        expected.forEach(function(ex) {
          if (line.match(/ frame /) && line.match(ex)) {
            ++matches;
          }
        });
      });
      assertTrue(matches > 0, lines);
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
