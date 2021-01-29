/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'foxx.api': 'true',
    'runSetup': true
  };
}

require("@arangodb/test-helper").waitForFoxxInitialized();

if (runSetup === true) {
  let users = require("@arangodb/users");
  
  users.save("test_rw", "testi");
  users.grantDatabase("test_rw", "_system", "rw");
  
  users.save("test_ro", "testi");
  users.grantDatabase("test_ro", "_system", "ro");
  return true;
}

var jsunity = require('jsunity');

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;
  const errors = require('@arangodb').errors;

  return {
    setUp: function() {},
    tearDown: function() {},
    
    testCanAccessAdminFoxxApi : function() {
      ["test_rw", "test_ro"].forEach(function(user) {
        arango.reconnect(endpoint, db._name(), user, "testi");

        let routes = [
          "setup", "teardown", "install", "uninstall",
          "replace", "upgrade", "configure", "configuration",
          "set-dependencies", "dependencies", "development",
          "tests", "script"
        ];

        routes.forEach(function(route) {
          let result = arango.POST("/_admin/foxx/" + route, {});
          assertTrue(result.error);
          assertNotEqual(403, result.code);
          assertNotEqual(403, result.errorNum);
          assertNotEqual(errors.ERROR_FORBIDDEN.code, result.errorNum);
        });
      });
    },
    
    testCanAccessPutApiFoxxApi : function() {
      ["test_rw", "test_ro"].forEach(function(user) {
        arango.reconnect(endpoint, db._name(), user, "testi");

        let routes = [
          "store", "git", "url", "generate", "zip", "raw" 
        ];

        routes.forEach(function(route) {
          let result = arango.PUT("/_api/foxx/" + route, {});
          assertNotEqual(403, result.code);
          assertNotEqual(403, result.errorNum);
          assertNotEqual(errors.ERROR_FORBIDDEN.code, result.errorNum);
        });
      });
    },
    
    testCanAccessPostApiFoxxApiRo : function() {
      ["test_rw", "test_ro"].forEach(function(user) {
        arango.reconnect(endpoint, db._name(), user, "testi");

        let routes = [
          "tests", "download/nonce"
        ];

        routes.forEach(function(route) {
          let result = arango.POST("/_api/foxx/" + route, {});
          assertNotEqual(403, result.code);
          assertNotEqual(403, result.errorNum);
          assertNotEqual(errors.ERROR_FORBIDDEN.code, result.errorNum);
        });
      });
    },
    
    testCanAccessGetApiFoxxApi : function() {
      ["test_rw", "test_ro"].forEach(function(user) {
        arango.reconnect(endpoint, db._name(), user, "testi");

        let routes = [
          "", "thumbnail", "config", "deps", "fishbowl", "download/zip" 
        ];

        routes.forEach(function(route) {
          let result = arango.GET("/_api/foxx/" + route);
          assertNotEqual(403, result.code);
          assertNotEqual(403, result.errorNum);
          assertNotEqual(errors.ERROR_FORBIDDEN.code, result.errorNum);
        });
      });
    },

  };
}
jsunity.run(testSuite);
return jsunity.done();
