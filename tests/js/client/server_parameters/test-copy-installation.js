/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertFalse, arango */

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
    'javascript.copy-installation': 'true',
    'javascript.allow-admin-execute': 'true',
  };
}

const db = require("@arangodb").db;
const fs = require("fs");
const jsunity = require('jsunity');

function testSuite() {
  const errors = require('@arangodb').errors;

  return {
    setUp: function() {},
    tearDown: function() {},

    testCanExecuteAction : function() {
      // fetch server-side database directory name
      let data = "return require('@arangodb').db._path();";
      let dbPath = arango.POST("/_admin/execute", data);
      let jsPath = fs.join(dbPath, "js");
      assertTrue(fs.exists(jsPath));
      assertTrue(fs.exists(fs.join(jsPath, "node")));
      assertTrue(fs.exists(fs.join(jsPath, "node", "node_modules")));
      assertTrue(fs.exists(fs.join(jsPath, "node", "node_modules", "lodash")));
      assertFalse(fs.exists(fs.join(jsPath, "node", "eslint")));
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
