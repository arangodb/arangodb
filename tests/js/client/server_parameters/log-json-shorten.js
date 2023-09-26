/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertNotMatch, assertEqual */

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
    'log.line-number': 'true',
    'log.shorten-filenames': 'true',
    'log.role': 'false',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.foreground-tty': 'false',
    'log.thread': 'false',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  let oldLogLevel;

  return {
    setUpAll: function() {
      oldLogLevel = arango.GET("/_admin/log/level").general;
      arango.PUT("/_admin/log/level", {general: "info"});
    },

    tearDownAll: function() {
      // restore previous log level for "general" topic;
      arango.PUT("/_admin/log/level", {general: oldLogLevel});
    },

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
        let parsedRes = JSON.parse(filtered[i]);
        assertTrue(parsedRes.hasOwnProperty("time"), parsedRes);
        assertTrue(parsedRes.hasOwnProperty("pid"), parsedRes);
        assertTrue(parsedRes.hasOwnProperty("level"), parsedRes);
        assertTrue(parsedRes.hasOwnProperty("topic"), parsedRes);
        assertTrue(parsedRes.hasOwnProperty("id"), parsedRes);
        assertFalse(parsedRes.hasOwnProperty("hostname"), parsedRes);
        assertFalse(parsedRes.hasOwnProperty("role"), parsedRes);
        assertFalse(parsedRes.hasOwnProperty("tid"), parsedRes);
        assertTrue(parsedRes.hasOwnProperty("file"), parsedRes);
        assertFalse(parsedRes.file.includes("/") || parsedRes.file.includes("\\"), parsedRes.file);
        assertMatch(/^[a-f0-9]{5}/, parsedRes.id, parsedRes);
        assertTrue(parsedRes.hasOwnProperty("message"), parsedRes);
        assertEqual("testmann: testi" + (i - 1), parsedRes.message, parsedRes);
      }
      assertTrue(filtered[51].match(/testmann: done/));
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
