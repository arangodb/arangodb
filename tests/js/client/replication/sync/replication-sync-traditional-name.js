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
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const db = arangodb.db;
const _ = require('lodash');

const replication = require('@arangodb/replication');
const internal = require('internal');

const cn = 'UnitTestsReplication';
const sysCn = '_UnitTestsReplication';
const userManager = require("@arangodb/users");
let IM = global.instanceManager;
const leaderEndpoint = IM.arangods[0].endpoint;
const followerEndpoint = IM.arangods[1].endpoint;

const {
  ReplicationOtherDBSuiteBase,
  BaseTestConfig,
  connectToLeader,
  connectToFollower,
  clearFailurePoints } = require(fs.join('tests', 'js', 'client', 'replication', 'sync', 'replication-sync.inc'));

function ReplicationOtherDBTraditionalNameSuite () {
  return ReplicationOtherDBSuiteBase('UnitTestDB');
}

jsunity.run(ReplicationOtherDBTraditionalNameSuite);
return jsunity.done();
