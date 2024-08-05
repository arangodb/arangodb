/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue */

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

if (runSetup === true) {
  'use strict';
  // intentionall do nothing, so we will see a normal shutdown!
  global.instanceManager.shutdownInstance();
  return 0;
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
          // and its following line ===============================
          return false;
        }
        if (internal.env["isSan"] === "true" && line.match(/\[3ad54\].*=+/)) {
          // intentionally ignore "slow background settings sync: " in case of ASan
          return false;
        }
        if (line.match(/\[2c0c6\].*extended names+/)) {
          // intentionally ignore "extended names for databases is an experimental feature...
          return false;
        }
        if (line.match(/\[de8f3\].*experimental option/)) {
          // intentionally ignore experimental options warnings
          return false;
        }
        if (line.match(/\[1afb1\].*This is an unlicensed ArangoDB instance./)) {
          // intentionally ignore "This is an unlicensed ArangoDB instance...
          return false;
        }
        if (line.match(/\[d72fb\].*Your license will expire/)) {
          // intentionally ignore "This is an unlicensed ArangoDB instance...
          return false;
        }
        if (line.match(/\[5e48f\].*blob/)) {
          // intentionally ignore startup warning about BlobDB being an
          // experimental feature
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

jsunity.run(recoverySuite);
return jsunity.done();
