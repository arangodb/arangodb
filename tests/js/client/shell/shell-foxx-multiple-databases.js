/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const path = require('path');
const db = arangodb.db;
const FoxxManager = require('@arangodb/foxx/manager');
const basePath1 = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'perdb1');
const basePath2 = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'perdb2');

require("@arangodb/test-helper").waitForFoxxInitialized();

function multipleDatabasesSuite () {
  'use strict';
  const mount1 = '/test1';
  const mount2 = '/test2';

  return {
    setUp: function () {
      db._useDatabase("_system");
      db._createDatabase("UnitTestsFoxx");
    },

    tearDown: function () {
      db._useDatabase("_system");
      try {
        db._dropDatabase("UnitTestsFoxx");
      } catch (err) {}
    },

    testSystemDatabaseMultipleApps: function () {
      assertEqual("_system", db._name());

      FoxxManager.install(basePath1, mount1);
      try {
        FoxxManager.install(basePath2, mount2);
        try {
          let res = arango.GET(`/_db/_system/${mount1}/echo`);
          assertEqual("_system", res.db);
          
          res = arango.GET(`/_db/_system/${mount1}/echo-piff`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/_system/${mount1}/echo-nada`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/_system/${mount2}/echo`);
          assertTrue(res.echo);
          
          res = arango.GET(`/_db/_system/${mount2}/echo-piff`);
          assertTrue(res.piff);
          
          res = arango.GET(`/_db/_system/${mount2}/echo-nada`);
          assertEqual({}, res);
        } finally {
          FoxxManager.uninstall(mount2, {force: true});
        } 
      } finally {
        FoxxManager.uninstall(mount1, {force: true});
      }
    },
    
    testCustomDatabaseMultipleApps: function () {
      assertEqual("_system", db._name());
      db._useDatabase("UnitTestsFoxx");

      FoxxManager.install(basePath1, mount1);
      try {
        FoxxManager.install(basePath2, mount2); // issue
        try {
          let res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo`);
          assertEqual("UnitTestsFoxx", res.db);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo-piff`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo-nada`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount2}/echo`);
          assertTrue(res.echo); // issue
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount2}/echo-piff`);
          assertTrue(res.piff);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount2}/echo-nada`);
          assertEqual({}, res);
        } finally {
          FoxxManager.uninstall(mount2, {force: true});
        } 
      } finally {
        FoxxManager.uninstall(mount1, {force: true});
      }
    },
    
    testCustomDatabaseAndSystemDifferentAppsSameMount: function () {
      assertEqual("_system", db._name());

      FoxxManager.install(basePath1, mount1);
      try {
        db._useDatabase("UnitTestsFoxx");
        FoxxManager.install(basePath2, mount1);
        try {
          let res = arango.GET(`/_db/_system/${mount1}/echo`);
          assertEqual("_system", res.db);
          
          res = arango.GET(`/_db/_system/${mount1}/echo-piff`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/_system/${mount1}/echo-nada`);
          assertTrue(res.error);
          assertEqual(404, res.code);
          assertEqual(404, res.errorNum);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo`);
          assertTrue(res.echo);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo-piff`);
          assertTrue(res.piff);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo-nada`);
          assertEqual({}, res);
        } finally {
          FoxxManager.uninstall(mount2, {force: true});
        } 
      } finally {
        db._useDatabase("_system");
        FoxxManager.uninstall(mount1, {force: true});
      }
    },
    
    testCustomDatabaseAndSystemSameApp: function () {
      assertEqual("_system", db._name());

      FoxxManager.install(basePath1, mount1);
      try {
        db._useDatabase("UnitTestsFoxx");
        FoxxManager.install(basePath1, mount1);
        try {
          let res = arango.GET(`/_db/_system/${mount1}/echo`);
          assertEqual("_system", res.db);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo`);
          assertEqual("UnitTestsFoxx", res.db);
          
          res = arango.GET(`/_db/_system/${mount1}/echo`);
          assertEqual("_system", res.db);
          
          res = arango.GET(`/_db/UnitTestsFoxx/${mount1}/echo`);
          assertEqual("UnitTestsFoxx", res.db);
        } finally {
          FoxxManager.uninstall(mount2, {force: true});
        } 
      } finally {
        db._useDatabase("_system");
        FoxxManager.uninstall(mount1, {force: true});
      }
    },
  };
}

jsunity.run(multipleDatabasesSuite);

return jsunity.done();
