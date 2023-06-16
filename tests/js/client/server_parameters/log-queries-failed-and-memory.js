/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertEqual */

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
    'query.log-failed': 'true',
    'query.log-memory-usage-threshold': '3145728',
    'query.max-artifact-log-length': '1024',
  };
}

const jsunity = require('jsunity');

function LoggerSuite() {
  'use strict';

  let getLogLines = function () {
    const res = arango.POST("/_admin/execute?returnBodyAsJSON=true", `
require('console').log("testmann: start"); 
let db = require('internal').db;
db._query("/*LOG TEST ok query*/ RETURN CONCAT('a', 'b', 'c')");
try {
  db._query("/*LOG TEST failed query*/ RETURN PIFF()");
  throw "failed!";
} catch (err) {}

db._query("/*LOG TEST low memory usage*/ FOR i IN 1..1024 RETURN i");
db._query("/*LOG TEST high memory usage*/ FOR i IN 1..1048576 RETURN i");

let values = [];
for (let i = 0; i < 1024; ++i) {
  values.push(i);
}
db._query("/*LOG TEST large bind1*/ FOR i IN 1..1024 FILTER i IN @values RETURN i", { values });

try {
  db._query("/*LOG TEST large bind2*/ FOR i IN 1..1024 FILTER i IN @values RETURN PIFF()", { values });
  throw "failed!";
} catch (err) {}

require('console').log("testmann: done"); 
return require('internal').options()["log.output"];
`);

    assertTrue(Array.isArray(res));
    assertTrue(res.length > 0);

    let logfile = res[res.length - 1].replace(/^file:\/\//, '');

    // log is buffered, so give it a few tries until the log messages appear
    let tries = 0;
    let lines;
    while (++tries < 60) {
      let content = fs.readFileSync(logfile, 'ascii');

      lines = content.split('\n');

      let filtered = lines.filter((line) => {
        return line.match(/testmann: /);
      });

      if (filtered.length === 2) {
        break;
      }

      require("internal").sleep(0.5);
    }
    return lines.filter((line) => {
      return line.match(/LOG TEST/);
    });
  };

  let oldLogLevels;

  return {
    setUpAll : function() {
      oldLogLevels = arango.GET("/_admin/log/level");
      // must ramp up log levels because otherwise everything is hidden by
      // default during testing
      arango.PUT("/_admin/log/level", { general: "info", queries: "warn" });
    },

    tearDownAll : function () {
      // restore previous log level for "general" topic;
      arango.PUT("/_admin/log/level", oldLogLevels);
    },

    testLoggedQueries: function () {
      const lines = getLogLines();

      // should not be logged:
      //   /*LOG TEST ok query*/ RETURN CONCAT('a', 'b', 'c')
      assertEqual(0, lines.filter((line) => line.match(/LOG TEST ok query/)).length);

      // should be logged:
      //   /*LOG TEST failed query*/ RETURN PIFF()
      assertEqual(1, lines.filter((line) => line.match(/LOG TEST failed query/)).length);

      // should not be logged:
      //   /*LOG TEST low memory usage*/ FOR i IN 1..1024 RETURN i
      assertEqual(0, lines.filter((line) => line.match(/LOG TEST low memory usage/)).length);

      // should not be logged:
      //   /*LOG TEST large bind1*/ FOR i IN 1..1024 FILTER i IN @values RETURN i
      assertEqual(0, lines.filter((line) => line.match(/LOG TEST large bind1/)).length);

      // should be logged:
      //   /*LOG TEST large bind1*/ FOR i IN 1..1024 FILTER i IN @values RETURN PIFF()
      assertEqual(1, lines.filter((line) => line.match(/LOG TEST large bind2/)).length);

      // test truncation after 1024 chars:
      assertEqual(1, lines.filter((line) => line.includes("/*LOG TEST large bind2*/ FOR i IN 1..1024 FILTER i IN @values RETURN PIFF()', bind vars: {\"values\":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,... (2985)")).length);
    },
  };
}

jsunity.run(LoggerSuite);
return jsunity.done();
