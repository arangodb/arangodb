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


function doesDatabaseExist (database) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-doesDatabaseExist/database/${database}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.exists;
}
function flush () {
  let res = arango.PUT_RAW(`_api/cluster/cluster-info-flush`, "");
  arangosh.checkRequestResult(res);
  return res.parsedBody.OK;
}
function databases () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-databases`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.databases;
}
function getCollectionInfo (databaseID, collectionID) {
  print(`_api/cluster/cluster-info-getCollectionInfo/databaseID/${databaseID}/collectionID/${collectionID}`);
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getCollectionInfo/databaseID/${databaseID}/collectionID/${collectionID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.shardShorts;
}
function getCollectionInfoCurrent (databaseID, collectionID, shardID) {
  let res = arango.GET_RAW(
    `_api/cluster/cluster-info-getCollectionInfoCurrent/databaseID/${databaseID}/collectionID/${collectionID}/shardID/${shardID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getResponsibleServer (shardID) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getResponsibleServer/shardID/${shardID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getResponsibleServers (shardIDs) {
  let res = arango.POST_RAW(`_api/cluster/cluster-info-getResponsibleServers`, shardIDs);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getResponsibleShard (collectionID, documentKey, parseSuccess) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getResponsibleShard/collectionID/${collectionID}/documentKey/${documentKey}/parseSuccess/${parseSuccess}`);
  arangosh.checkRequestResult(res);
}
function getServerEndpoint (serverID) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getServerEndpoint/serverID/${serverID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.endpoint;
}
function getServerName (endpoint) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getServerName/${encodeURIComponent(endpoint)}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.serverName;
}
function getDBServers () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getDBServers`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getCoordinators () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getCoordinators`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function uniqid (count) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-uniqid/${count}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getAnalyzersRevision (database) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-getAnalyzersRevision/${database}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function waitForPlanVersion (versionToWaitFor) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info-waitForPlanVersion/${versionToWaitFor}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}



exports.doesDatabaseExist = doesDatabaseExist;
exports.flush = flush;
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



