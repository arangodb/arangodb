/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, arango */

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
    'http.keep-alive-timeout': '5'
  };
}
let jsunity = require('jsunity');
let db = require('internal').db;
const originalEndpoint = arango.getEndpoint();

function testSuite() {
  let connectWith = function(protocol) {
    let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
    arango.reconnect(endpoint, db._name(), arango.connectedUser(), "");
  };

  return {
    tearDown: function() {
      // restore original connection type
      arango.reconnect(originalEndpoint, db._name(), arango.connectedUser(), "");
    },

    testKeepAliveTimeoutHttp1 : function() {
      connectWith("tcp");
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let result = db._query("FOR i IN 1..10 RETURN SLEEP(1)").toArray();
      assertEqual(10, result.length);
    },
    
    testKeepAliveTimeoutVst : function() {
      connectWith("vst");
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let result = db._query("FOR i IN 1..10 RETURN SLEEP(1)").toArray();
      assertEqual(10, result.length);
    },
    
    testKeepAliveTimeoutHttp2 : function() {
      connectWith("h2");
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let result = db._query("FOR i IN 1..10 RETURN SLEEP(1)").toArray();
      assertEqual(10, result.length);
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
