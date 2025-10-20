/*jshint globalstrict:false, strict:false */
/* global GLOBAL, print, getOptions, assertTrue, arango, assertMatch, assertEqual */

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
    'log.use-json-format': 'false',
    'log.role': 'false',
    'log.foreground-tty': 'false',
    'log.prefix': 'PREFIX',
    'log.thread': 'true',
    'log.thread-name': 'true',
    'log.line-number': 'true',
    'log.ids': 'true',
    'log.hostname': 'HOSTNAME',
    'log.process': 'true',
    'log.level': 'info',
  };
}

const fs = require('fs');
const jsunity = require('jsunity');
const { logServer } = require('@arangodb/test-helper');
const IM = GLOBAL.instanceManager;

function LoggerSuite() {
  'use strict';

  return {

    testLogEntries: function() {
      IM.rememberConnection();
      IM.arangods.forEach(arangod => {
        print(`testing ${arangod.name}`);
        arangod.connect();
        logServer("testmann: start");
        for (let i = 0; i < 50; ++i) {
          logServer("testmann: testi" + i);
        }
        logServer("testmann: done", "error");
        // log is buffered, so give it a few tries until the log messages appear
        let tries = 0;
        let filtered = [];
        while (++tries < 60) {
          let content = fs.readFileSync(arangod.logFile, 'ascii');
          let lines = content.split('\n');

          filtered = lines.filter((line) => {
            return line.match(/testmann: /);
          });

          if (filtered.length === 52) {
            break;
          }

          require("internal").sleep(0.5);
        }
        assertEqual(52, filtered.length);

        assertMatch(/testmann: start/, filtered[0]);
        for (let i = 1; i < 51; ++i) {
          const msg = filtered[i];
          assertTrue(msg.startsWith("HOSTNAME"));
          assertMatch(/.*PREFIX.*/, msg);
          assertMatch(/.*\[\d{1,}-\d{1,}-[A-Za-z0-9]+\].*/, msg);
          assertMatch(/testmann: testi\d+/, msg);

        }
        assertMatch(/testmann: done/, filtered[51]);

        let res = arango.GET("/_admin/log");
        assertTrue(res.hasOwnProperty("totalAmount"));
        assertTrue(res.hasOwnProperty("lid"));
        assertTrue(res.hasOwnProperty("topic"));
        assertTrue(res.hasOwnProperty("level"));
        assertTrue(res.hasOwnProperty("timestamp"));
        assertTrue(res.hasOwnProperty("text"));

        for (let i = 0; i < res.totalAmount; ++i) {
          assertMatch(/^\d+$/, res.lid[i]);
          assertMatch(/^[A-Za-z]{1,}$/, res.topic[i]);
          assertMatch(/^\d+$/, res.level[i]);
          assertMatch(/^\d+$/, res.timestamp[i]);
        }
      });
      IM.reconnectMe();
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
