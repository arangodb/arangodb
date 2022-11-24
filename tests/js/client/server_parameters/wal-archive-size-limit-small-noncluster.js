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
      const body = `
let fs = require('fs');
let getLogLines = (logfile) => {
  let content = fs.readFileSync(logfile, 'ascii');
  let lines = content.split('\\n');

  return lines.filter((line) => {
    // logId "d9793" from RocksDBEngine.cpp
    return line.match(/(testmann: |d9793)/);
  });
};
      
let logfiles = require('internal').options()["log.output"];
const logfile = logfiles[logfiles.length - 1].replace(/^file:\\/\\//, '');

require('console').log("testmann: start"); 
let docs = [];
for (let i = 0; i < 2000; ++i) {
  docs.push({ value1: "test" + i, payload: Array(512).join("x") });
}
let db = require("internal").db;
let time = require("internal").time;
let i = 0;
let start = time();
do {
  db.UnitTestsCollection.insert(docs);
  if (i++ % 10 === 0) {
    let lines = getLogLines(logfile);
    if (lines.length >= 2) {
      break;
    }
  }
} while (time() - start < 300);
  
require('console').log("testmann: done"); 
return getLogLines(logfile);
`;

      let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", body);

      assertTrue(Array.isArray(res));

      let found = false;
      assertTrue(res[0].match(/testmann: start/));
      for (let i = 0; i < res.length; ++i) {
        if (res[i].match(/d9793/)) {
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
