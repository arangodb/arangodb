/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertMatch, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
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

if (getOptions === true) {
  return {
    'log.in-memory': 'true',
    'log.in-memory-level' : 'error',
  };
}

const jsunity = require('jsunity');

function testSuite() {
  let checkEmpty = function() {
    // check that the in-memory logger does not return them (min log level is FATAL)
    let res = arango.GET("/_admin/log?upto=trace");
    assertEqual(0, res.totalAmount);
    assertEqual([], res.lid);
    assertEqual([], res.topic);
    assertEqual([], res.level);
    assertEqual([], res.timestamp);
    assertEqual([], res.text);
  };
  
  let checkPresent = function(level) {
    let res = arango.GET("/_admin/log?upto=trace");
    assertEqual(50, res.totalAmount, res);
    assertEqual(50, res.lid.length, res);
    assertEqual(50, res.topic.length, res);
    assertEqual(50, res.level.length, res);
    res.level.forEach((l) => assertEqual(level, l, res));
    assertEqual(50, res.timestamp.length, res);
    assertEqual(50, res.text.length, res);
    res.text.forEach((t) => assertMatch(/testi/, t, res));
  };
      
  let log = function(level) {
    arango.POST("/_admin/execute", `for (let i = 0; i < 50; ++i) require('console')._log('general=${level}', 'testi');`);
  };

  return {
    setUp : function() {
      arango.DELETE("/_admin/log");
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
      checkPresent(1);
    },
    
    testApiFatal : function() {
      log("fatal");
      checkPresent(0);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
