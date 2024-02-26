/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, arango, assertMatch, assertEqual */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');

if (getOptions === true) {
  return {
    'log.max-entry-length': '16384',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.foreground-tty': 'false',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  let oldLogLevel;

  return {
    setUpAll : function() {
      oldLogLevel = arango.GET("/_admin/log/level").general;
      arango.PUT("/_admin/log/level", { general: "info" });
    },

    tearDownAll : function () {
      // restore previous log level for "general" topic;
      arango.PUT("/_admin/log/level", { general: oldLogLevel });
    },

    testLogEntries: function() {
      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("testmann: start"); 
require('console').log("testmann: " + Array(32768).join("x")); 
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

        if (filtered.length === 3) {
          break;
        }

        require("internal").sleep(0.5);
      }
      assertEqual(3, filtered.length);
          
      assertTrue(filtered[0].match(/testmann: start/));
      assertTrue(filtered[1].match(/testmann: xxxxxxxx/));
      assertTrue(filtered[2].match(/testmann: done/));

      assertTrue(filtered[1].length <= 16384 + '...'.length, filtered[1].length);
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
