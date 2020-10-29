/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

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
  const fs = require('fs');
  const tu = require('@arangodb/test-utils');
  const keyFile = fs.join(tu.pathForTesting('.'), '..', '..', 'UnitTests', 'server.pem');
  return {
    'http.keep-alive-timeout': '5',
    'ssl.keyfile': keyFile,
    'protocol': 'ssl', // not an arangod option, but picked up by the test framework
  };
}
let jsunity = require('jsunity');
let internal = require('internal');
let db = internal.db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const originalEndpoint = arango.getEndpoint();

function BaseTestConfig(protocol) {
  let connectWith = function(protocol) {
    let endpoint = arango.getEndpoint().replace(/^[a-zA-Z0-9\+]+:/, protocol + ':');
    arango.reconnect(endpoint, db._name(), arango.connectedUser(), "");
  };

  let checkReconnect = function(timeout) {
    // make a request so we are reconnecting if necessary
    db._databases();
    let statsBefore = arango.connectionStatistics();
    internal.sleep(timeout);

    // make a request so we are reconnecting if necessary
    db._databases();
    let statsAfter = arango.connectionStatistics();
    assertTrue(statsAfter.connects > statsBefore.connects, { statsBefore, statsAfter });
  };

  return {
    tearDown : function () {
      // restore original connection type
      arango.reconnect(originalEndpoint, db._name(), arango.connectedUser(), "");
    },

    testKeepAliveTimeout : function() {
      connectWith(protocol);
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let result = db._query("FOR i IN 1..10 RETURN SLEEP(1)").toArray();
      assertEqual(10, result.length);
    },
    
    testKeepAliveTimeoutLargeInputValue : function() {
      connectWith(protocol);
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let value = Array(1024 * 1024).join("test");
      let result = db._query("LET x = NOOPT(SLEEP(10)) RETURN @value", { value }).toArray();
      assertEqual(1, result.length);
    },
    
    testKeepAliveTimeoutLargeReturnValue : function() {
      connectWith(protocol);
      // the query should succeed despite it running longer than the configured 
      // connection timeout
      let result = db._query("LET x = NOOPT(SLEEP(10)) RETURN (FOR i IN 1..100000 RETURN i)").toArray();
      assertEqual(1, result.length);
    },
    
    testIdleTimeout : function() {
      connectWith(protocol);
      checkReconnect(7);
    },
    
  };
}

function http1TestSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(BaseTestConfig("ssl"), suite, '_ssl_http1');
  return suite;
};
function vstTestSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(BaseTestConfig("vst+ssl"), suite, '_ssl_vst');
  return suite;
};

function http2TestSuite() {
  'use strict';
  let suite = {};
  deriveTestSuite(BaseTestConfig("h2+ssl"), suite, '_ssl_http2');
  return suite;
};

jsunity.run(http1TestSuite);
jsunity.run(vstTestSuite);
jsunity.run(http2TestSuite);
return jsunity.done();
