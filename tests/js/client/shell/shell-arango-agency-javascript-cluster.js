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
        // this executes all the calls from inside Foxx
        let res = arango.GET(`/_db/_system/${mount}/runInsideFoxx`);
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
    
    testAccessFromFoxxTransaction : function() {
      const mount = '/test';

      FoxxManager.install(basePath, mount);
      try {
        // this executes a server-side transaction running inside Foxx
        let res = arango.GET(`/_db/_system/${mount}/runInsideFoxxTransaction`);
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
    
    testAccessFromTransaction : function() {
      // this executes a server-side transaction with all the operations,
      // not using Foxx
      let results = db._executeTransaction({
        collections: {},
        action: function() {
          const ERRORS = require('@arangodb').errors;
          let agencyCall = function(f) {
            let result = false;
            try {
              f();
            } catch (err) {
              result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
            }
            return result;
          };

          let testCases = {};
          // ArangoClusterInfo
          testCases["AgencyClusterInfoUniqid"] = function() {
            let testee = global.ArangoClusterInfo;
            let result = false;
            try {
              testee.uniqid();
            } catch (err) {
              result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
            }
            return result;
          };

          // ArangoAgency
          ["agency", "read", "write", "transact", "transient", "cas", "get", "createDirectory",
           "increaseVersion", "remove", "endpoints", "set", "uniqid"].forEach((func) => {
            testCases["ArangoAgency" + func] = function() {
              let testee = global.ArangoAgency;
              return agencyCall(testee[func]);
            };
          });
          // ArangoAgent
          ["enabled", "leading", "read", "write", "state"].forEach((func) => {
            testCases["ArangoAgent" + func] = function() {
              let testee = global.ArangoAgent;
              return agencyCall(testee[func]);
            };
          });
          let results = {};
          Object.keys(testCases).forEach((tc) => {
            results[tc] = testCases[tc]();
          });
          return results;
        }
      });
      let cases = Object.keys(results);
      assertEqual(19, cases.length);
      cases.forEach((c) => {
        assertTrue(results[c], results);
      });
    },
    
    testAccessFromTask : function() {
      // this executes all the operations server-side,
      // inside a JavaScript task
      const cn = "UnitTestsTaskResult";

      db._drop(cn);
      db._create(cn);

      try {
        let tasks = require("@arangodb/tasks");
        tasks.register({
          command: function() {
            const db = require('@arangodb').db;

            const ERRORS = require('@arangodb').errors;
            let agencyCall = function(f) {
              let result = false;
              try {
                f();
              } catch (err) {
                result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
              }
              return result;
            };

            let testCases = {};
            // ArangoClusterInfo
            testCases["AgencyClusterInfoUniqid"] = function() {
              let testee = global.ArangoClusterInfo;
              let result = false;
              try {
                testee.uniqid();
              } catch (err) {
                result = (err.errorNum === ERRORS.ERROR_FORBIDDEN.code);
              }
              return result;
            };

            // ArangoAgency
            ["agency", "read", "write", "transact", "transient", "cas", "get", "createDirectory",
             "increaseVersion", "remove", "endpoints", "set", "uniqid"].forEach((func) => {
              testCases["ArangoAgency" + func] = function() {
                let testee = global.ArangoAgency;
                return agencyCall(testee[func]);
              };
            });
            // ArangoAgent
            ["enabled", "leading", "read", "write", "state"].forEach((func) => {
              testCases["ArangoAgent" + func] = function() {
                let testee = global.ArangoAgent;
                return agencyCall(testee[func]);
              };
            });
            let results = [];
            Object.keys(testCases).forEach((tc) => {
              results.push({ name: tc, result: testCases[tc]() });
            });
            
            db.UnitTestsTaskResult.insert(results);
          }
        });

        let tries = 0;
        while (++tries < 60) {
          if (db[cn].count() === 19) {
            break;
          }
          internal.sleep(0.5);
        }

        assertEqual(19, db[cn].count());
        db[cn].toArray().forEach((doc) => {
          assertTrue(doc.result, doc);
        });
      } finally {
        db._drop(cn);
      }
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
