/*jshint strict: true */
/*global assertTrue, assertEqual, print*/
"use strict";
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const _ = require("lodash");
const lh = require("@arangodb/testutils/replicated-logs-helper");

const serverHelper = require('@arangodb/test-helper');

const database = 'CollectionGroupsSupervisionTest';

const readCollectionGroupAgency = function (database, groupId) {
  let target = lh.readAgencyValueAt(`Target/CollectionGroups/${database}/${groupId}`);
  let plan = lh.readAgencyValueAt(`Plan/CollectionGroups/${database}/${groupId}`);
  let current = lh.readAgencyValueAt(`Current/CollectionGroups/${database}/${groupId}`);
  return {target, plan, current};
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

const createCollectionGroupTarget = function (database, config) {
  const gid = lh.nextUniqueLogId();
  const cid = lh.nextUniqueLogId();

  const numberOfShards = config.numberOfShards || 1;

  const target = {
    id: gid,
    collections: {[`${cid}`]: {}},
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

  const collectionTarget = {
    groupId: gid,
    mutableProperties: {
      schema: null,
      computedValues: null,
    },
    indexes: {
      indexes: []
    },
    immutableProperties: {
      name: `TestCollection${cid}`,
      id: `${cid}`,
      isSystem: false,
      isSmart: false,
      isDisjoint: false,
      cacheEnabled: false,
      type: 0,
      keyOptions: {
        allowUserKeys: true,
        type: "traditional",
      },
    },
  };
  print(target);

  serverHelper.agency.write([[{
    [`arango/Target/CollectionGroups/${database}/${gid}`]: target,
    [`arango/Target/Collections/${database}/${cid}`]: collectionTarget,
    [`arango/Target/CollectionNames/${database}/${collectionTarget.immutableProperties.name}`]: {},
  }]]);

  return {gid, cid};
};

const {setUpAll, tearDownAll, setUp, tearDown} = lh.testHelperFunctions(database, {replicationVersion: "2"});

const collectionGroupsSupervisionSuite = function () {


  return {
    setUpAll, tearDownAll, setUp, tearDown,

    testCreateCollectionGroup: function () {
      const {gid} = createCollectionGroupTarget(database, {
        replicationFactor: 3,
        numberOfShards: 3,
      });

      lh.waitFor(collectionGroupIsReady(database, gid));
    }
  };
};

jsunity.run(collectionGroupsSupervisionSuite);
return jsunity.done();
