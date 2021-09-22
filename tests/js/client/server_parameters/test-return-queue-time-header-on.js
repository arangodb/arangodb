/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertMatch, arango */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB Inc, Cologne, Germany
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
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'http.return-queue-time-header': 'true'
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;

function testSuite() {  
  const header = "x-arango-queue-time-seconds";

  return {
    testQueueTimeHeaderReturned : function() {
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.headers.hasOwnProperty(header));
      let value = result.headers[header];
      assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
    },
    
    testQueueTimeHeaderNonZero : function() {
      for (let i = 0; i < 40; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(1);', { "x-arango-async": "true" });
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      require("internal").sleep(3);
      
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.headers.hasOwnProperty(header));
      let value = result.headers[header];
      assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      value = parseFloat(value);
      assertTrue(value > 0, value);
    },
    
    testRejectBecauseOfTooHighQueueTime : function() {
      for (let i = 0; i < 40; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(1);', { "x-arango-async": "true" });
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      require("internal").sleep(3);
      
      let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0.00001" });
      assertEqual(412, result.code);
      assertEqual(errors.ERROR_QUEUE_TIME_REQUIREMENT_VIOLATED.code, result.errorNum);
      
      // try with much higher queue time. this should succeed
      result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "5000" });
      assertEqual("arango", result.server);
    },
    
    testNotRejectedBecauseOfQueueTimeZero : function() {
      for (let i = 0; i < 40; ++i) {
        let result = arango.POST_RAW("/_admin/execute", 'require("internal").sleep(1);', { "x-arango-async": "true" });
        assertTrue(result.headers.hasOwnProperty(header));
        let value = result.headers[header];
        assertMatch(/^([0-9]*\.)?[0-9]+$/, value);
      }

      require("internal").sleep(3);
      
      let result = arango.GET("/_api/version", { "x-arango-queue-time-seconds": "0" });
      assertEqual("arango", result.server);
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
