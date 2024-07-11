/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertMatch */

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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const jsunity = require('jsunity');

function runSetup () {
  'use strict';
  // make log level more verbose, as by default we hide most messages from
  // the test output
  require("internal").logLevel("crash=info");
  // calls std::terminate() in the server
  internal.debugTerminate('CRASH-HANDLER-TEST-TERMINATE-ACTIVE');
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

      let versionDetails = internal.db._version(true).details;
      assertTrue(versionDetails.hasOwnProperty("asan"));
      const asan = versionDetails.asan === "true";

      let lines = fs.readFileSync(crashFile).toString().split("\n").filter(function(line) {
        return line.match(/\{crash\}/);
      });
      assertTrue(lines.length > 0);

      // check message
      let line = lines.shift();
      if (asan) {
        // using asan,  
        assertMatch(/FATAL.*thread \d+.*caught unexpected signal 6.*handler for std::terminate\(\) invoked without active exception/, line);
      } else {
        assertMatch(/FATAL.*thread \d+.*caught unexpected signal 6.*handler for std::terminate\(\) invoked with an std::exception: /, line);
      }

      // check debug symbols
      // it is a bit compiler- and optimization-level-dependent what
      // symbols we get
      let expected = [ /std::rethrow_exception/, /installCrashHandler/, /TerminateDebugging/, /JS_DebugTerminate/ ];
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
