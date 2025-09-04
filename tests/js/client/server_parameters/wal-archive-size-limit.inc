/*jshint globalstrict:false, strict:false */
/* global GLOBAL, getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

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
const {
  logServer,
  insertManyDocumentsIntoCollection
} = require('@arangodb/test-helper');
const IM = GLOBAL.instanceManager;

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
let getLogLines = function() {
  let content = fs.readFileSync(IM.arangods[0].logFile, 'ascii');
  let lines = content.split('\n');

  return lines.filter((line) => {
    // logId "d9793" from RocksDBEngine.cpp
    return line.match(/(testmann: |d9793)/);
  });
};

exports.WalArchiveSizeLimitSuite = WalArchiveSizeLimitSuite;
exports.sendLargeServerOperation = function(maxWaitTime) {
  logServer("testmann: start");
  let time = require("internal").time;
  let tEnd = time() + maxWaitTime;
  let checkCount = 0;
  insertManyDocumentsIntoCollection(
    '_system', 'UnitTestsCollection',
    (i) => {
      return {i, payload: Array(512).join("x")};},
    1000000, 2000,
    () => {
      let now = time();
      if (now > tEnd) { return true; }
      checkCount += 1;
      if (checkCount % 20 === 0) {
        return (getLogLines().length > 2);
      }
      return false;
    });
  
  logServer("testmann: done", "error"); 
  return getLogLines();
};
