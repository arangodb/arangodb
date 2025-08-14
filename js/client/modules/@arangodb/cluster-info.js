/* jshint strict: false */
/* global arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// / @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
const arangosh = require('@arangodb/arangosh');


function doesDatabaseExist () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-doesDatabaseExist`);
  arangosh.checkRequestResult(res);
}
function flush () {
  let res = arango.PUT_RAW(`_api/cluster/cluster-info-flush`);
  arangosh.checkRequestResult(res);
}
function databases () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-databases`);
  arangosh.checkRequestResult(res);
}
function getCollectionInfo () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getCollectionInfo`);
  arangosh.checkRequestResult(res);
}
function getCollectionInfoCurrent () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getCollectionInfoCurrent`);
  arangosh.checkRequestResult(res);
}
function getResponsibleServer () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getResponsibleServer`);
  arangosh.checkRequestResult(res);
}
function getResponsibleServers () {
  let res = arango.POST_RAW(`_api/cluster/cluster-info-getResponsibleServers`);
  arangosh.checkRequestResult(res);
}
function getResponsibleShard () {
  let res = arango.POST_RAW(`_api/cluster/cluster-info-getResponsibleShard`);
  arangosh.checkRequestResult(res);
}
function getServerEndpoint (serverID) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getServerEndpoint/serverID/${serverID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.endpoint;
}
function getServerName () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getServerName`);
  arangosh.checkRequestResult(res);
}
function getDBServers () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getDBServers`);
  arangosh.checkRequestResult(res);
}
function getCoordinators () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getCoordinators`);
  arangosh.checkRequestResult(res);
}
function uniqid () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-uniqid`);
  arangosh.checkRequestResult(res);
}
function getAnalyzersRevision () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getAnalyzersRevision`);
  arangosh.checkRequestResult(res);
}
function waitForPlanVersion () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-waitForPlanVersion`);
  arangosh.checkRequestResult(res);
}



exports.doesDatabaseExist = doesDatabaseExist;
exports.databases = databases;
exports.getCollectionInfo = getCollectionInfo;
exports.getCollectionInfoCurrent = getCollectionInfoCurrent;
exports.getResponsibleServer = getResponsibleServer;
exports.getResponsibleServers = getResponsibleServers;
exports.getResponsibleShard = getResponsibleShard;
exports.getServerEndpoint = getServerEndpoint;
exports.getServerName = getServerName;
exports.getDBServers = getDBServers;
exports.getCoordinators = getCoordinators;
exports.uniqid = uniqid;
exports.getAnalyzersRevision = getAnalyzersRevision;
exports.waitForPlanVersion = waitForPlanVersion;



