/*jshint globalstrict:false, strict:false */
/* global GLOBAL, print, getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual, assertNotEqual */

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
    'log.use-json-format': 'true',
    'log.foreground-tty': 'false',
    'log.prefix': 'PREFIX',
    'log.thread': 'false',
    'log.thread-name': 'false',
    'log.role': 'true',
    'log.line-number': 'true',
    'log.ids': 'true',
    'log.hostname': 'auto',
    'log.process': 'true',
    'log.in-memory': 'true',
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
        let tries = 0;
        let filtered = [];
        while (tries++ < 60) {
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
          assertTrue(filtered[i].match(/testmann: testi\d+/));
          let parsedRes = JSON.parse(filtered[i]);
          assertTrue(parsedRes.hasOwnProperty("prefix"), parsedRes);
          assertEqual(parsedRes.prefix, "PREFIX");
          assertTrue(parsedRes.hasOwnProperty("time"), parsedRes);
          assertTrue(parsedRes.hasOwnProperty("pid"), parsedRes);
          assertMatch(/\d+/, parsedRes.pid);
          assertTrue(parsedRes.hasOwnProperty("level"), parsedRes);
          assertTrue(parsedRes.hasOwnProperty("topic"), parsedRes);
          assertTrue(parsedRes.hasOwnProperty("id"), parsedRes);
          assertTrue(parsedRes.hasOwnProperty("hostname"), parsedRes);
          assertEqual(typeof parsedRes.hostname, "string");
          assertNotEqual(parsedRes.hostname, "");
          assertTrue(parsedRes.hasOwnProperty("role"), parsedRes);
          assertFalse(parsedRes.hasOwnProperty("tid"), parsedRes);
          assertFalse(parsedRes.hasOwnProperty("thread"));
          assertTrue(parsedRes.hasOwnProperty("line"));
          assertMatch(/^\d+$/, parsedRes.line);
          assertMatch(/^[a-f0-9]{5}$/, parsedRes.id, parsedRes);
          assertTrue(parsedRes.hasOwnProperty("message"), parsedRes);
          assertEqual("testmann: testi" + (i - 1), parsedRes.message, parsedRes);
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
