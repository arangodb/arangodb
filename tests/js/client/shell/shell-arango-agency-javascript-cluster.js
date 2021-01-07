/*jshint globalstrict:false, strict:false */
/* global assertEqual, assertTrue, assertFalse, arango */

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

const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const db = require('internal').db;
const FoxxManager = require('@arangodb/foxx/manager');
const path = require('path');
const internal = require('internal');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'arango-agency');

require("@arangodb/test-helper").waitForFoxxInitialized();

function testSuite() {
  const cn = "UnitTestsTransaction";

  return {
    testAccessFromFoxx : function() {
      const mount = '/test';

      FoxxManager.install(basePath, mount);
      try { 
        let res = arango.GET(`/_db/_system/${mount}/run`);
        let results = res.results;
        let cases = Object.keys(results);
        assertEqual(19, cases.length);
        cases.forEach((c) => {
          assertTrue(results[c], results);
        });
      } finally {
        FoxxManager.uninstall(mount, {force: true});
      } 
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
