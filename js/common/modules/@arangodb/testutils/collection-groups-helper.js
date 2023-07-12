/*jshint strict: true */
/*global print*/
'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alexandru Petenchea
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

const lh = require("@arangodb/testutils/replicated-logs-helper");
const serverHelper = require('@arangodb/test-helper');

const readCollectionGroupAgency = function (database, groupId) {
  let target = lh.readAgencyValueAt(`Target/CollectionGroups/${database}/${groupId}`);
  let plan = lh.readAgencyValueAt(`Plan/CollectionGroups/${database}/${groupId}`);
  let current = lh.readAgencyValueAt(`Current/CollectionGroups/${database}/${groupId}`);
  return {target, plan, current};
};

const createCollectionTarget = function (gid, cid) {
  return {
    groupId: gid,
    schema: null,
    computedValues: null,
    indexes: [],
    name: `TestCollection${cid}`,
    id: `${cid}`,
    isSystem: false,
    isSmart: false,
    isDisjoint: false,
    cacheEnabled: false,
    shardKeys: ["_key"],
    type: 2,
    keyOptions: {
      allowUserKeys: true,
      type: "traditional",
    },
  };
};

const collectionGroupIsReady = function (database, groupId) {
  return function () {
    const {target, current} = readCollectionGroupAgency(database, groupId);
    if (target.version === undefined) {
      return true; // nothing to check
    }

    if (current === undefined || current.supervision === undefined) {
      return Error("No supervision in current");
    }

    if (current.supervision.targetVersion !== target.version) {
      return Error(`Collection group has not yet converged to version ${target.value},` +
        ` found ${current.supervision.targetVersion}`);
    }

    return true;
  };
};

const getCollectionGroupServers = function (database, gid) {
  const {plan} = readCollectionGroup(database, gid);
  const logs = plan.shardSheaves.map((x) => x.replicatedLog);

  const servers = logs.map(function (logId) {
    const {target} = lh.readReplicatedLogAgency(database, logId);
    return Object.keys(target.participants);
  });

  const leaders = logs.map(function (logId) {
    const {plan} = lh.readReplicatedLogAgency(database, logId);
    return plan.currentTerm.leader.serverId;
  });

  return {servers, leaders, logs};
};

const createCollectionGroupTarget = function (database, config) {
  const gid = lh.nextUniqueLogId();

  const numberOfShards = config.numberOfShards || 1;
  const numberOfCollections = config.numberOfCollections || 1;
  const cids = new Array(numberOfCollections).fill(undefined).map(() => lh.nextUniqueLogId());
  const target = {
    id: gid,
    collections: {},
    attributes: {
      mutable: {
        writeConcern: config.writeConcern || 1,
        replicationFactor: config.replicationFactor || 2,
        waitForSync: false,
      },
      immutable: {
        numberOfShards: numberOfShards,
      }
    },
    version: 1,
  };

  const agencyTrx = {
    [`arango/Target/CollectionGroups/${database}/${gid}`]: target,
  };

  for (const cid of cids) {
    const collectionTarget = createCollectionTarget(gid, cid);
    target.collections[cid] = {};
    agencyTrx[`arango/Target/Collections/${database}/${cid}`] = collectionTarget;
    agencyTrx[`arango/Target/CollectionNames/${database}/${collectionTarget.name}`] = {};
  }

  serverHelper.agency.write([[agencyTrx]]);
  lh.waitFor(collectionGroupIsReady(database, gid));

  const {servers, leaders, logs} = getCollectionGroupServers(database, gid);
  return {gid, cid: cids[0], cids, logs, servers, leaders};
};

const addCollectionToGroup = function (database, gid) {
  const cid = lh.nextUniqueLogId();
  const collectionTarget = createCollectionTarget(gid, cid);

  serverHelper.agency.write([[{
    [`arango/Target/CollectionGroups/${database}/${gid}/collections/${cid}`]: {},
    [`arango/Target/CollectionGroups/${database}/${gid}/version`]: {op: "increment"},
    [`arango/Target/Collections/${database}/${cid}`]: collectionTarget,
    [`arango/Target/CollectionNames/${database}/${collectionTarget.name}`]: {},
  }]]);

  return {cid};
};

const dropCollectionFromGroup = function (database, gid, cid) {
  serverHelper.agency.write([[{
    [`arango/Target/CollectionGroups/${database}/${gid}/collections/${cid}`]: {op: "delete"},
    [`arango/Target/CollectionGroups/${database}/${gid}/version`]: {op: "increment"},
    [`arango/Target/Collections/${database}/${cid}`]: {op: "delete"},
  }]]);

  return {cid};
};

const readCollectionGroup = function (database, gid) {
  const target = lh.readAgencyValueAt(`Target/CollectionGroups/${database}/${gid}`);
  const plan = lh.readAgencyValueAt(`Plan/CollectionGroups/${database}/${gid}`);
  const current = lh.readAgencyValueAt(`Current/CollectionGroups/${database}/${gid}`);
  return {target, plan, current};
};

const readCollection = function (database, cid) {
  const target = lh.readAgencyValueAt(`Target/Collections/${database}/${cid}`);
  const plan = lh.readAgencyValueAt(`Plan/Collections/${database}/${cid}`);
  const current = lh.readAgencyValueAt(`Current/Collections/${database}/${cid}`);
  return {target, plan, current};
};

const modifyCollectionGroupTarget = function (database, gid, cb) {
  const {target} = readCollectionGroup(database, gid);
  const res = cb(target) || target;
  serverHelper.agency.set(`Target/CollectionGroups/${database}/${gid}`, res);
};

exports.readCollectionGroupAgency = readCollectionGroupAgency;
exports.createCollectionTarget = createCollectionTarget;
exports.collectionGroupIsReady = collectionGroupIsReady;
exports.getCollectionGroupServers = getCollectionGroupServers;
exports.createCollectionGroupTarget = createCollectionGroupTarget;
exports.addCollectionToGroup = addCollectionToGroup;
exports.dropCollectionFromGroup = dropCollectionFromGroup;
exports.readCollectionGroup = readCollectionGroup;
exports.readCollection = readCollection;
exports.modifyCollectionGroupTarget = modifyCollectionGroupTarget;
