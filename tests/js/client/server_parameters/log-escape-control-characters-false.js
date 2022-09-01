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
    'log.escape-control-chars': 'false',
  };
}

const jsunity = require('jsunity');

function EscapeControlFalseSuite() {
  'use strict';

  return {
    testEscapeControlFalse: function() {
      const escapeCharsLength = 31;
      const res = arango.POST("/_admin/execute", `
        require('console').log("testmann: start");
        for (let i = 1; i <= 31; ++i) {
          require('console').log("testmann: testi" + String.fromCharCode(i) + " abc123");
        }
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
        let content = fs.readFileSync(logfile, 'ascii');
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

      assertTrue(filtered[0].match(/testmann: start/));
      for (let i = 1; i < escapeCharsLength + 1; ++i) {
        const msg = filtered[i];
        assertTrue(msg.endsWith("testmann: testi  abc123"));
      }
      assertTrue(filtered[escapeCharsLength + 1].match(/testmann: done/));

    },

  };
}


jsunity.run(EscapeControlFalseSuite);
return jsunity.done();
