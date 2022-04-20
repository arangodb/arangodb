/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertEqual */

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

if (getOptions === true) {
  return {
    'log.structured-param': ['database=true', 'url', 'username', "pregelId=false"],
    'log.use-json-format': 'true',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  let generateFilteredLog = function(fieldName) {
    const res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("${fieldName}: start"); 
for (let i = 0; i < 10; ++i) {
  require('console').log("${fieldName}: test" + i);
}
require('console').log("${fieldName}: done"); 
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
        return line.match(new RegExp(`${fieldName}: `));
      });

      if (filtered.length === 12) {
        break;
      }

      require("internal").sleep(0.5);
    }
    return filtered;
  };

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

    testStartingParameters: function () {
      const res = arango.GET("/_admin/log/structured");
      assertTrue(res.hasOwnProperty("database"));
      assertTrue(res.hasOwnProperty("username"));
      assertTrue(res.hasOwnProperty("url"));
      assertFalse(res.hasOwnProperty("pregelId"));
    },

    testLogEntries: function () {
      const formerSettings = arango.GET("/_admin/log/structured");
      try {
        const resChangeParams = arango.PUT("/_admin/log/structured", {
          "collection": true,
          "database": true,
          "username": false,
          "url": false
        });
        assertFalse(resChangeParams.hasOwnProperty("collection"));
        assertFalse(resChangeParams.hasOwnProperty("url"));
        assertFalse(resChangeParams.hasOwnProperty("username"));
        assertFalse(resChangeParams.hasOwnProperty("dog"));
        assertTrue(resChangeParams.hasOwnProperty("database"));
        assertFalse(resChangeParams.hasOwnProperty("url"));

        const filtered = generateFilteredLog("testmann");

        assertEqual(12, filtered.length);

        assertTrue(filtered[0].match(/testmann: start/));
        for (let i = 1; i < 11; ++i) {
          const filteredObj = JSON.parse(filtered[i]);
          assertFalse(filteredObj.hasOwnProperty("collection"));
          assertTrue(filteredObj.hasOwnProperty("database"));
          assertFalse(filteredObj.hasOwnProperty("username"));
          assertFalse(filteredObj.hasOwnProperty("url"));

          assertFalse(filtered[i].match(/\[collection: /));
          assertTrue(filtered[i].match(/testmann: test\d+/));
          assertTrue(filtered[i].match("[database: _system]"));
          assertFalse(filtered[i].match(/\[username: root\]/));
          assertFalse(filtered[i].match(/\[url: \/_admin\/execute\?returnBodyAsJSON=true\]/));
        }
        assertTrue(filtered[11].match(/testmann: done/));
      } finally {
        arango.PUT("/_admin/log/structured", formerSettings);
      }
    },

    testLogNewEntries: function () {
      const formerSettings = arango.GET("/_admin/log/structured");
      try {
        const resChangeParams = arango.PUT("/_admin/log/structured", {
          "dog": true,
          "database": false,
          "username": true,
          "url": true
        });
        assertTrue(resChangeParams.hasOwnProperty("url"));
        assertTrue(resChangeParams.hasOwnProperty("username"));
        assertFalse(resChangeParams.hasOwnProperty("dog"));
        assertFalse(resChangeParams.hasOwnProperty("database"));

        const filtered = generateFilteredLog("testParams");
        assertEqual(12, filtered.length);

        assertTrue(filtered[0].match(/testParams: start/));
        for (let i = 1; i < 11; ++i) {
          const filteredObj = JSON.parse(filtered[i]);
          assertFalse(filteredObj.hasOwnProperty("dog"));
          assertFalse(filteredObj.hasOwnProperty("database"));
          assertTrue(filteredObj.hasOwnProperty("username"));
          assertTrue(filteredObj.hasOwnProperty("url"));

          assertTrue(filtered[i].match(/testParams: test\d+/));
          assertFalse(filtered[i].match(/\[dog: /));
          assertFalse(filtered[i].match(/\[database: _system\]/));
          assertTrue(filtered[i].match("[username: root]"));
          assertTrue(filtered[i].match(`[url: /_admin/execute?returnBodyAsJSON=true]`));
        }
        assertTrue(filtered[11].match(/testParams: done/));
      } finally {
        arango.PUT("/_admin/log/structured", formerSettings);
      }
    },
  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
