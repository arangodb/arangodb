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


function flush () {
  let res = arango.PUT_RAW(`_api/cluster/cluster-info/flush`, "");
  arangosh.checkRequestResult(res);
  return res.parsedBody.OK;
}
function getCollectionInfo (databaseName, collectionName) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/get_collection_info/${databaseName}/${collectionName}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getCollectionInfoCurrent (databaseName, collectionName, shardID) {
  let res = arango.GET_RAW(
    `_api/cluster/cluster-info/get_collection_info_current/${databaseName}/${collectionName}/${shardID}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getResponsibleServers (shardIDs) {
  let res = arango.POST_RAW(`_api/cluster/cluster-info/get_responsible_servers`, shardIDs);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getResponsibleShard (collectionName, document, documentIsComplete) {
  let res = arango.POST_RAW(`_api/cluster/cluster-info/get_responsible_shard/${arango.getDatabaseName()}/${collectionName}/${documentIsComplete}`, document);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function getAnalyzersRevision (database) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/get_analyzers_revision/${database}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}
function waitForPlanVersion (versionToWaitFor) {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/wait_for_plan_version/${versionToWaitFor}`);
  arangosh.checkRequestResult(res);
  return res.parsedBody;
}

function getMaxNumberOfShards () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/get_max_number_of_shards`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.maxNumberOfShards;
}

function getMaxReplicationFactor () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/get_max_replication_factor`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.maxReplicationfactor;
}

function getMinReplicationFactor () {
  let res = arango.GET_RAW(`_api/cluster/cluster-info/get_min_replication_factor`);
  arangosh.checkRequestResult(res);
  return res.parsedBody.minReplicationfactor;
}

exports.flush = flush;
exports.getCollectionInfo = getCollectionInfo;
exports.getCollectionInfoCurrent = getCollectionInfoCurrent;
exports.getResponsibleServers = getResponsibleServers;
exports.getResponsibleShard = getResponsibleShard;
exports.getAnalyzersRevision = getAnalyzersRevision;
exports.waitForPlanVersion = waitForPlanVersion;
exports.getMaxNumberOfShards = getMaxNumberOfShards;
exports.getMaxReplicationFactor = getMaxReplicationFactor;
exports.getMinReplicationFactor = getMinReplicationFactor;
