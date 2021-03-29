/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');

if (getOptions === true) {
  return {
    'log.use-json-format': 'true',
    'log.role': 'false',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.foreground-tty': 'false',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  return {
    
    testLogEntries: function() {
      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("testmann: start"); 
for (let i = 0; i < 50; ++i) {
  require('console').log("testmann: testi" + i);
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

        if (filtered.length === 52) {
          break;
        }

        require("internal").sleep(0.5);
      }
      assertEqual(52, filtered.length);
          
      assertTrue(filtered[0].match(/testmann: start/));
      for (let i = 1; i < 51; ++i) {
        assertTrue(filtered[i].match(/testmann: testi\d+/));
        let msg = JSON.parse(filtered[i]);
        assertTrue(msg.hasOwnProperty("time"), msg);
        assertTrue(msg.hasOwnProperty("pid"), msg);
        assertTrue(msg.hasOwnProperty("level"), msg);
        assertTrue(msg.hasOwnProperty("topic"), msg);
        assertTrue(msg.hasOwnProperty("id"), msg);
        assertFalse(msg.hasOwnProperty("hostname"), msg);
        assertFalse(msg.hasOwnProperty("role"), msg);
        assertFalse(msg.hasOwnProperty("tid"), msg);
        assertMatch(/^[a-f0-9]{5}/, msg.id, msg);
        assertTrue(msg.hasOwnProperty("message"), msg);
        assertEqual("testmann: testi" + (i - 1), msg.message, msg);
      }
      assertTrue(filtered[51].match(/testmann: done/));
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
