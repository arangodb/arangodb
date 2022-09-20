/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, arango, assertEqual, assertMatch */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');

if (getOptions === true) {
  return {
    'log.hostname': 'delorean',
    'log.process': 'false',
    'log.ids': 'false',
    'log.role': 'true',
    'log.thread': 'true',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.foreground-tty': 'false',
    'log.level': 'debug',
    'log.escape-unicode-chars': 'true',
    'log.use-json-format': 'true',
  };
}

const jsunity = require('jsunity');

function EscapeUnicodeTrueSuite() {
  'use strict';


  return {
    testEscapeUnicodeTrue: function() {
      const testValuesLength = 4;
      const expectedValues = ['\\u00B0', 'm\\u00F6t\\u00F6r', 'ma\\u00E7\\u00E3', '\\u72AC'];

      const res = arango.POST("/_admin/execute", `
        require('console').log("testmann: start");
        const testValues = ["°", "mötör", "maçã", "犬"];
        testValues.forEach(testValue => {
          require('console').log("testmann: testi "  + testValue + " abc123");
        });
        require('console').log("testmann: done");
        return require('internal').options()["log.output"];
      `);

      assertTrue(Array.isArray(res));
      assertTrue(res.length > 0);

      let logfile = res[res.length - 1].replace(/^file:\/\//, '');

      // log is buffered, so give it a few tries until the log messages appear
      let tries = 0;
      let filtered = [];
      while (++tries < 60) {
        let content = fs.readFileSync(logfile, 'utf-8');
        let lines = content.split('\n');

        filtered = lines.filter((line) => {
          return line.match(/testmann: /);
        });

        if (filtered.length === testValuesLength + 2) {
          break;
        }

        require("internal").sleep(0.5);
      }
      assertEqual(testValuesLength + 2, filtered.length);

      assertMatch(/testmann: start/, filtered[0]);
      for (let i = 1; i < testValuesLength + 1; ++i) {
        const msg = JSON.parse(filtered[i]);
        assertTrue(msg.hasOwnProperty("message"));
        assertTrue(msg.message, "testmann: testi " + expectedValues[i - 1] + " abc123");
      }
      assertMatch(/testmann: done/, filtered[testValuesLength + 1]);
    },
  };
}


jsunity.run(EscapeUnicodeTrueSuite);
return jsunity.done();
