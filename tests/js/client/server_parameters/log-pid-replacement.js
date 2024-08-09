/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, arango, assertMatch */

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
    'log.output': 'file://' + fs.getTempFile() + '.testi.$PID',
    'log.foreground-tty': 'false',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  return {
    
    testLogEntries: function() {
      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
return require('internal').options()["log.output"];
`);

      assertTrue(Array.isArray(res));
      res = res.filter((x) => x.startsWith('file://'));
      assertTrue(res.length > 0);

      let logfile = res[0].replace(/^file:\/\//, '');
      // logfile name must end with dot and then process id
      assertMatch(/\.testi\.[0-9]+$/, logfile);
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
