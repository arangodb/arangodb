/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, VPACK_TO_V8 */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for fuerte responses
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');

function fuerteResponseTestSuite () {
  return {
    testGet : function () {
      let response = arango.GET("/_api/version");
      assertEqual("arango", response.server);
    },
    
    testGetRaw : function () {
      let response = arango.GET_RAW("/_api/version");
      assertFalse(response.error);
      assertTrue(response.hasOwnProperty("parsedBody"));
      assertEqual("arango", response.parsedBody.server);
      
      assertTrue(response.hasOwnProperty("body"));
      assertEqual("arango", VPACK_TO_V8(response.body).server);
      
      assertTrue(response.hasOwnProperty("headers"));
      assertEqual("application/x-velocypack", response.headers['content-type']);
      assertTrue(response.headers.hasOwnProperty('content-length'));

      if (arango.protocol() === "http") {
        // http/1 provides the full status text in a seperate header
        // http/2 does not provide such header, neither does VST
        assertEqual("200 OK", response.headers['http/1.1']);
      }
    },
    
  };
}

jsunity.run(fuerteResponseTestSuite);
return jsunity.done();
