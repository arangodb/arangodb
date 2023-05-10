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
const db = require('internal').db;
const getCoordinatorEndpoints = require('@arangodb/test-helper').getCoordinatorEndpoints;
const cn = "testCollection";
const dbName = "maçã";
const originalEndpoint = arango.getEndpoint();

if (getOptions === true) {
  return {
    'log.use-json-format': 'true',
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
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


const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  return {

    setUp: function() {
      db._useDatabase("_system");
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      const c = db._create(cn, {numberOfShards: 3});
      let props = c.properties();
      assertEqual(3, props.numberOfShards);
    },

    tearDown: function() {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
      arango.reconnect(originalEndpoint, db._name(), "root", "");
    },

    testLogClusterInfoInFile: function() {
      let coordinatorEndpoint = getCoordinatorEndpoints()[0];
      arango.reconnect(coordinatorEndpoint, db._name(), "root", "");

      let res1 = arango.GET("/_api/cluster/cluster-info?returnBodyAsJSON=true");

      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
        require('console').log("testmann: start"); 
        require('console').log("testmann: testi" + '${JSON.stringify(res1)}');
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
