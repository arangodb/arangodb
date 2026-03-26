/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, arango */

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

if (getOptions === true) {
  return {
    'log.in-memory': 'true',
    'log.in-memory-level' : 'fatal',
  };
}

const jsunity = require('jsunity');
const { logServer } = require('@arangodb/test-helper');

function testSuite() {
  let checkEmpty = function() {
    // check that the in-memory logger does not return them (min log level is FATAL)
    let res = arango.GET("/_admin/log/entries?upto=trace");
    assertEqual(0, res.total);
    assertEqual(0, res.messages.length);
  };
  
  let checkPresent = function(level) {
    let res = arango.GET("/_admin/log/entries?upto=trace");
    assertEqual(50, res.total);
    assertEqual(50, res.messages.length);
    res.messages.forEach((message) => {
      assertTrue(message.hasOwnProperty("id"));
      assertTrue(message.hasOwnProperty("topic"));
      assertTrue(message.hasOwnProperty("level"));
      assertTrue(message.hasOwnProperty("date"));
      assertTrue(message.hasOwnProperty("message"));
      assertEqual(level, message.level);
      assertMatch(/testi/, message.message);
    });
  };
      
  let log = function(level) {
    for (let i = 1; i <= 50; ++i) {
      logServer('testi', level);
    }
  };

  return {
    setUp : function() {
      arango.DELETE("/_admin/log/entries");
    },

    testApiTrace : function() {
      log("trace");
      checkEmpty();
    },

    testApiDebug : function() {
      log("debug");
      checkEmpty();
    },

    testApiInfo : function() {
      log("info");
      checkEmpty();
    },
    
    testApiWarn : function() {
      log("warn");
      checkEmpty();
    },
    
    testApiErr : function() {
      log("error");
      checkEmpty();
    },
    
    testApiFatal : function() {
      log("fatal");
      // /_admin/log/entries returns string literals e.g. "FATAL"
      checkPresent("FATAL");
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
