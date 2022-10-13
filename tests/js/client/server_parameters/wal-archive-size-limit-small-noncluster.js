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
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'rocksdb.wal-archive-size-limit': '10000000', // roughly 10MB
    'rocksdb.wal-file-timeout-initial': '10',
  };
}

const jsunity = require('jsunity');

function WalArchiveSizeLimitSuite() {
  'use strict';
      
  const db = require("internal").db;
  let oldLogLevel;

  return {
    setUpAll : function() {
      oldLogLevel = arango.GET("/_admin/log/level").general;
      // adjusting log levels is necessary to find the messages in the
      // logs later - otherwise they would be suppressed
      arango.PUT("/_admin/log/level", { general: "info", engines: "warning" });
    },
    
    tearDownAll : function () {
      // restore previous log level for "general" topic;
      arango.PUT("/_admin/log/level", { general: oldLogLevel });
    },
      
    setUp : function () {
      db._create("UnitTestsCollection");
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testDoesNotForceDeleteWalFiles: function() {
      // insert larger amounts of data on the server
      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("testmann: start"); 
let docs = [];
for (let i = 0; i < 1000; ++i) {
  docs.push({ value1: "test" + i, payload: Array(512).join("x") });
}
let db = require("internal").db;
let time = require("internal").time;
let start = time();
do {
  db.UnitTestsCollection.insert(docs);
} while (time() - start < 20);
  
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
          // logId "d9793" from RocksDBEngine.cpp
          return line.match(/(testmann: |d9793)/);
        });

        if (filtered.length >= 2) {
          break;
        }

        require("internal").sleep(0.5);
      }

      // this will fail if warning d9793 was *not* logged
      let found = false;
      assertTrue(filtered[0].match(/testmann: start/));
      for (let i = 0; i < filtered.length; ++i) {
        if (filtered[i].match(/d9793/)) {
          found = true;
          break;
        }
      }

      assertTrue(found);
    },

  };
}

jsunity.run(WalArchiveSizeLimitSuite);
return jsunity.done();
