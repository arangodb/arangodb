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


const {testHelperFunctions, readAgencyValueAt} = require("@arangodb/testutils/replicated-logs-helper");
const database = 'CollectionGroupsTest';

const {setUpAll, tearDownAll, setUp, tearDown} = testHelperFunctions(database, {replicationVersion: "2"});

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

    const getCollectionGroups = () => readAgencyCollectionGroups(database);
    return {
        setUpAll, tearDownAll, setUp, tearDown,

        testCreateOneNewCollection: function() {
            const collectionName = "UnitTestCollection";
            try {
                const groupsBefore = getCollectionGroups();
                // TODO: To activate this the create database needs to properly instanciate the System Collections Group
                // assertTrue(_.isObject(groupsBefore), `CollectionGroups is not an object ${groupsBefore}`);
                db._create(collectionName, {numberOfShards: 3});
                const groupsAfter = getCollectionGroups();
                require("internal").print(groupsBefore, groupsAfter);
                assertTrue(_.isObject(groupsAfter), `CollectionGroups is not an object ${groupsAfter}`);
                assertNoCollectionIsInTwoGroups(groupsAfter);
                // assertEqual(Object.keys(groupsBefore).length + 1, Object.keys(groupsAfter), `It has to create exactly one new group`);
                const myGroup = getGroupWithCollection(groupsAfter, collectionName);
                require("internal").print(myGroup);
            } finally {
                db._drop(collectionName);
            }
        }
    };
};

jsunity.run(createCollectionGroupsSuite);
return jsunity.done();