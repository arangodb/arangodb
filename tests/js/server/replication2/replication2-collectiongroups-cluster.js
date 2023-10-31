/*jshint strict: true */
/*global assertEqual, assertTrue, assertFalse */
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {db} = require("@arangodb");
const _ = require("lodash");


const {testHelperFunctions, readAgencyValueAt, waitFor} = require("@arangodb/testutils/replicated-logs-helper");
const database = 'CollectionGroupsTest';

const {setUpAll, tearDownAll, setUp, tearDownAnd} = testHelperFunctions(database, {replicationVersion: "2"});

const readAgencyCollectionGroups = (databaseName) => {
  return readAgencyValueAt(`Target/CollectionGroups/${databaseName}/`);
};

const getGroupWithCollection = (groups, collection) => {
  return Object.values(groups).find(g => g.collections.hasOwnProperty(collection));
};

const assertNoCollectionIsInTwoGroups = (groups) => {
  const uniqueCollectionList = new Set();
  Object.values(groups).map(g => {
    assertTrue(g.hasOwnProperty("collections"));
    Object.keys(g.collections).map(colName => {
      assertFalse(uniqueCollectionList.has(colName), `Duplicate collection ${colName} in groups ${groups}`);
      uniqueCollectionList.add(colName);
    });
  });
};

const createCollectionGroupsSuite = function () {

  const getCollectionGroups = () => {
    const groups = readAgencyCollectionGroups(database);
    assertTrue(_.isObject(groups), `CollectionGroups is not an object ${groups}`);
    assertNoCollectionIsInTwoGroups(groups);

    return groups;
  };

  const emptyCollectionGroupsGone = () => {
    const groups = readAgencyCollectionGroups(database);
    for (const [id, group] of Object.entries(groups)) {
      if (Object.keys(group.collections).length === 0) {
        return Error(`Collection group ${id} not removed but empty.`);
      }
    }
    return true;
  };

  return {
    setUpAll, tearDownAll, setUp, tearDown: tearDownAnd(function () {
      waitFor(emptyCollectionGroupsGone);
    }),

    testNewDatabaseHasOnlyOneGroupForSystemCollections: function () {
      const groups = getCollectionGroups();
      const systemCollections = db._collections().filter(c => c.properties().isSystem);
      assertTrue(systemCollections.length > 0, `We do not have any system collections`);
      const {collections, attributes} = getGroupWithCollection(groups, systemCollections[0].planId());
      for (const s of systemCollections) {
        assertTrue(collections.hasOwnProperty(s.planId()), `System collection ${s.name()} with id ${s.planId()} is not part of the group ${JSON.stringify(collections)}`);
      }
      assertEqual(Object.keys(collections).length, systemCollections.length);
    },

    testCreateOneNewCollection: function () {
      const collectionName = "UnitTestCollection";
      try {
        const groupsBefore = getCollectionGroups();
        assertTrue(_.isObject(groupsBefore), `CollectionGroups is not an object ${groupsBefore}`);
        const c = db._create(collectionName, {numberOfShards: 3});
        const groupsAfter = getCollectionGroups();
        assertEqual(Object.keys(groupsBefore).length + 1, Object.keys(groupsAfter).length, `It has to create exactly one new group`);
        const {collections, attributes} = getGroupWithCollection(groupsAfter, c.planId());
        assertEqual(Object.keys(collections).length, 1, `Got more than 1 collection in the group ${Object.keys(collections)}`);
        assertEqual(c.planId(), Object.keys(collections)[0]);
      } finally {
        db._drop(collectionName);
      }
    },

    testCreateAGroupOfCollections: function () {
      const collectionName = "UnitTestCollection";
      const followerName = "UnitTestCollectionFollower";
      try {
        const groupsBefore = getCollectionGroups();
        assertTrue(_.isObject(groupsBefore), `CollectionGroups is not an object ${groupsBefore}`);
        const c1 = db._create(collectionName, {numberOfShards: 3});
        const c2 = db._create(followerName, {numberOfShards: 3, distributeShardsLike: collectionName});
        const groupsAfter = getCollectionGroups();
        assertTrue(_.isObject(groupsAfter), `CollectionGroups is not an object ${groupsAfter}`);
        assertNoCollectionIsInTwoGroups(groupsAfter);
        assertEqual(Object.keys(groupsBefore).length + 1, Object.keys(groupsAfter).length, `It has to create exactly one new group\nBefore ${JSON.stringify(groupsBefore)}\nAfter ${JSON.stringify(groupsAfter)}`);
        // The following is currently disabled because each collection has its own group
        const {collections, attributes} = getGroupWithCollection(groupsAfter, c2.planId());
        assertEqual(Object.keys(collections).length, 2, `Did not get 2 collections in the group ${Object.keys(collections)}`);
        assertTrue(collections.hasOwnProperty(c1.planId()));
        assertTrue(collections.hasOwnProperty(c2.planId()));
      } finally {
        db._drop(followerName);
        db._drop(collectionName);
      }
    }
  };
};

jsunity.run(createCollectionGroupsSuite);
return jsunity.done();
