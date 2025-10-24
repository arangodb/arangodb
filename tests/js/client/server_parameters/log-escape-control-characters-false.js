/*jshint globalstrict:false, strict:false */
/* global GLOBAL, print, getOptions, assertTrue, arango, assertEqual, assertMatch */

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
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'log.hostname': 'delorean',
    'log.process': 'false',
    'log.ids': 'false',
    'log.role': 'true',
    'log.thread': 'true',
    'log.foreground-tty': 'false',
    'log.level': 'debug',
    'log.escape-control-chars': 'false',
  };
}

const fs = require('fs');
const jsunity = require('jsunity');
const { logServer } = require('@arangodb/test-helper');
const IM = GLOBAL.instanceManager;

function EscapeControlFalseSuite() {
  'use strict';

  return {
    testEscapeControlFalse: function() {
      IM.rememberConnection();
      IM.arangods.forEach(arangod => {
        print(`testing ${arangod.name}`);
        arangod.connect();
        const escapeCharsLength = 31;
        logServer("testmann: start");
        for (let i = 1; i <= 31; ++i) {
          logServer("testmann: testi" + String.fromCharCode(i) + " abc123");
        }
        logServer("testmann: done", "error"); // error flushes

        // log is buffered, so give it a few tries until the log messages appear
        let tries = 0;
        let filtered = [];
        while (++tries < 60) {
          let content = fs.readFileSync(arangod.logFile, 'ascii');
          let lines = content.split('\n');

          filtered = lines.filter((line) => {
            return line.match(/testmann: /);
          });

          if (filtered.length === escapeCharsLength + 2) {
            break;
          }

          require("internal").sleep(0.5);
        }
        assertEqual(escapeCharsLength + 2, filtered.length);

        assertMatch(/testmann: start/, filtered[0]);
        for (let i = 1; i < escapeCharsLength + 1; ++i) {
          const msg = filtered[i];
          assertTrue(msg.endsWith("testmann: testi  abc123"));
        }
        assertMatch(/testmann: done/, filtered[escapeCharsLength + 1]);
      });
      IM.reconnectMe();
    },

  };
}


jsunity.run(EscapeControlFalseSuite);
return jsunity.done();
