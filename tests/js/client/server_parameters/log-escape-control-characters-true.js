/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertEqual */

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
    'log.hostname': 'delorean',
    'log.process': 'false',
    'log.ids': 'false',
    'log.role': 'true',
    'log.thread': 'true',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.foreground-tty': 'false',
    'log.level': 'debug',
    'log.escape-control-chars': 'true',
  };
}

const jsunity = require('jsunity');

function EscapeControlSuite() {
  'use strict';

  return {
    testEscapeControlTrue: function() {
      const visibleControlChars = ['\n', '\r', '\b', '\t'];
      const escapeCharsLength = 31;
      const res = arango.POST("/_admin/execute", `
     
    require('console').log("testmann: start");
    for (let i = 1; i <= 31; ++i) {
        const hexCode = i.toString(16);
        const padding = hexCode.length === 1 ? "0" : "";
        const charCode = "0x" + padding + hexCode;
        require('console').log("testmann: testi" + String.fromCharCode(charCode));
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
      print(filtered);

      assertTrue(filtered[0].match(/testmann: start/));
      for (let i = 1; i < escapeCharsLength + 1; ++i) {
        let msg = JSON.parse(filtered[i]);
        assertTrue(msg.hasOwnProperty("time"), msg);
        assertFalse(msg.hasOwnProperty("pid"), msg);
        assertTrue(msg.hasOwnProperty("level"), msg);
        assertTrue(msg.hasOwnProperty("topic"), msg);
        assertFalse(msg.hasOwnProperty("id"), msg);
        assertTrue(msg.hasOwnProperty("hostname"), msg);
        assertEqual("delorean", msg.hostname, msg);
        assertTrue(msg.hasOwnProperty("role"), msg);
        assertTrue(msg.hasOwnProperty("tid"), msg);
        assertTrue(msg.hasOwnProperty("message"), msg);
        const hexCode = i.toString(16);
        const padding = hexCode.length === 1 ? "0" : "";
        const charCode = "0x" + padding + hexCode;
        const foundIdx = visibleControlChars.indexOf(String.fromCharCode(charCode));
        if (foundIdx !== -1) {
          assertEqual("testmann: testi" + visibleControlChars[foundIdx], msg.message, msg);
        }
        assertEqual("testmann: testi" + String.fromCharCode(charCode), msg.message, msg);
      }
      assertTrue(filtered[escapeCharsLength + 1].match(/testmann: done/));

    },

  };
}


jsunity.run(EscapeControlSuite);
return jsunity.done();
