/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotNull, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const analyzers = require("@arangodb/analyzers");
const { deriveTestSuite, getMetric, versionHas } = require('@arangodb/test-helper');
const db = arangodb.db;
const _ = require('lodash');

const replication = require('@arangodb/replication');
const internal = require('internal');

const cn = 'UnitTestsReplication';
const sysCn = '_UnitTestsReplication';
const userManager = require("@arangodb/users");

const {
  BaseTestConfig,
  connectToLeader,
  connectToFollower,
  clearFailurePoints } = require(fs.join('tests', 'js', 'client', 'replication', 'sync', 'replication-sync.inc'));

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite on _system
// //////////////////////////////////////////////////////////////////////////////

function ReplicationSuite () {
  'use strict';

  let suite = {
    setUp: function () {
      connectToLeader();
      arango.DELETE_RAW("/_admin/debug/failat", "");
      try {
        db._dropView(cn + 'View');
      } catch (ignored) {}
      try {
        db._dropView('analyzersView');
      } catch (ignored) {}
      db._drop(cn);
      db._drop(sysCn, {isSystem: true});
    },

    tearDown: function () {
      connectToLeader();
      arango.DELETE_RAW("/_admin/debug/failat", "");
      try {
        db._dropView(cn + 'View');
      } catch (ignored) {}
      try {
        db._dropView('analyzersView');
      } catch (ignored) {}
      db._drop(cn);
      db._drop(sysCn, {isSystem: true});
      try {
        analyzers.remove('custom');
      } catch (e) { }
      try {
        analyzers.remove('smartCustom');
      } catch (e) { }
      db._flushCache();
      userManager.all().forEach(user => {
        if (user.user !== "root") {
          userManager.remove(user.user);
        }
      });

      connectToFollower();
      arango.DELETE_RAW("/_admin/debug/failat", "");
      try {
        db._dropView(cn + 'View');
      } catch (ignored) {}
      try {
        db._dropView('analyzersView');
      } catch (ignored) {}
      db._drop(cn);
      db._drop(sysCn, {isSystem: true});
      try {
        analyzers.remove('custom');
      } catch (e) { }
      try {
        analyzers.remove('smartCustom');
      } catch (e) { }
      db._flushCache();
      userManager.all().forEach(user => {
        if (user.user !== "root") {
          userManager.remove(user.user);
        }
      });
    }
  };
  
  deriveTestSuite(BaseTestConfig(), suite, '_Repl');
  return suite;
}

jsunity.run(ReplicationSuite);

return jsunity.done();
