/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

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
  };
}

exports.WalArchiveSizeLimitSuite = WalArchiveSizeLimitSuite;
exports.sendLargeServerOperation = (maxWaitTime) => {
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
} while (time() - start < ${maxWaitTime});
  
require('console').log("testmann: done"); 
return getLogLines(logfile);
`;

  // insert larger amounts of data on the server
  let res = arango.POST("/_admin/execute?returnBodyAsJSON=true", body);

  return res;
};
