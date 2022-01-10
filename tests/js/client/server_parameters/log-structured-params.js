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
    'log.structured-param': ['database=true', 'url', 'username', "pregelId=false"],
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  return {

    testStartingParameters: function () {
      let res = arango.GET("/_admin/log/structured");
      assertTrue(res.hasOwnProperty("database"));
      assertTrue(res.hasOwnProperty("username"));
      assertTrue(res.hasOwnProperty("url"));
      assertFalse(res.hasOwnProperty("pregelId"));
    },

    testLogEntries: function() {
      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("testmann: start"); 
for (let i = 0; i < 10; ++i) {
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

        if (filtered.length === 12) {
          break;
        }

        require("internal").sleep(0.5);
      }
      assertEqual(12, filtered.length);

      assertTrue(filtered[0].match(/testmann: start/));
      for (let i = 1; i < 11; ++i) {
        assertTrue(filtered[i].match(/testmann: testi\d+/));
        assertTrue(filtered[i].match("[database: _system]"));
        assertTrue(filtered[i].match("[username: root]"));
        assertTrue(filtered[i].match(`[url: /_admin/execute?returnBodyAsJSON=true]`));
      }
      assertTrue(filtered[11].match(/testmann: done/));
    },

    testLogNewEntries: function() {
      let res = arango.PUT("/_admin/log/structured", {"dog": true, "queryId": true, "database": false});

      assertTrue(res.hasOwnProperty("queryId"));
      assertTrue(res.hasOwnProperty("url"));
      assertTrue(res.hasOwnProperty("username"));
      assertFalse(res.hasOwnProperty("dog"));
      assertFalse(res.hasOwnProperty("database"));
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
