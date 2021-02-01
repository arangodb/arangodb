/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail, arango */

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
    'javascript.tasks': "false",
    'javascript.allow-admin-execute': "false"
  };
}

const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;
const FoxxManager = require('@arangodb/foxx/manager');
const path = require('path');
const internal = require('internal');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'execute-task');

require("@arangodb/test-helper").waitForFoxxInitialized();

function testSuite() {
  return {
    testJavaScriptTask : function() {
      // JavaScript tasks should not work
      try {
        require('@arangodb/tasks').register({
          command: function() {
            require("console").log("this task must not run!");
            throw "peng!";
          }
        });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },
    
    testJavaScriptTaskFromFoxx : function() {
      const mount = '/test';

      FoxxManager.install(basePath, mount);
      try { 
        let res = arango.GET(`/_db/_system/${mount}/execute`);
        assertEqual(403, res.code);
        assertTrue(res.error);
      } finally {
        FoxxManager.uninstall(mount, {force: true});
      } 
    },
    
    testJavaScriptTaskViaAdminExecute : function() {
      let body = `require('@arangodb/tasks').register({ id: 'le-task-123', command: function() { require("console").log("hello from task!"); } }); return "ok!"; `;

      // this will fail because /_admin/execute is turned off
      let res = arango.POST('/_db/_system/_admin/execute?returnBodyAsJSON=true', body);
      // /_admin/execute API is turned off
      assertEqual(404, res.code);
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
