/*jshint globalstrict:false, strict:false */
/* global GLOBAL, print, getOptions, assertTrue, arango, assertEqual, assertMatch */

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
    'log.prefix': 'PREFIX',
    'log.thread': 'true',
    'log.thread-name': 'true',
    'log.role': 'true',
    'log.line-number': 'true',
    'log.ids': 'true',
    'log.hostname': 'HOSTNAME',
    'log.process': 'true',
    'log.in-memory': 'true',
    'log.level': 'info',
    'log.escape-unicode-chars': 'true',
    'database.extended-names': 'true',
    'server.authentication': 'false',
  };
}


const fs = require('fs');
const db = require('internal').db;
const jsunity = require('jsunity');
const inst = require('@arangodb/testutils/instance');
const { logServer } = require('@arangodb/test-helper');
const IM = GLOBAL.instanceManager;
const cn = "testCollection";
const dbName = "maçã";

function LoggerSuite() {
  'use strict';

  return {

    setUp: function() {
      IM.rememberConnection();
      db._useDatabase("_system");
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      const c = db._create(cn, {numberOfShards: 3});
      let props = c.properties();
      assertEqual(3, props.numberOfShards);
    },

    tearDown: function() {
      IM.reconnectMe();
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testLogClusterInfoInFile: function() {
      let coordinator = IM.arangods.filter(arangod => {return arangod.isFrontend();})[0];

      coordinator.connect();
      db._useDatabase(dbName);

      let res1 = arango.GET("/_api/cluster/cluster-info");

      logServer("testmann: start"); 
      logServer("testmann: testi" + `${JSON.stringify(res1)}`);
      logServer("testmann: done", "error");

      // log is buffered, so give it a few tries until the log messages appear
      let tries = 0;
      let filtered = [];
      while (++tries < 60) {
        let content = fs.readFileSync(coordinator.logFile, 'ascii');
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
      assertMatch(/testmann: start/, filtered[0]);
      const parsedRes = JSON.parse(filtered[1]);
      const parsedMsg = JSON.parse(parsedRes.message.substring(parsedRes.message.indexOf("{")));

      assertEqual(res1, parsedMsg);

      assertMatch(/testmann: done/, filtered[2]);
    },
  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
