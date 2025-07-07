/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

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
/// @author Valery Mironov
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const isCluster = require('internal').isCluster();
const db = arangodb.db;

function UpdateConsistencyArangoSearchView() {
  const collection = "testUpdate";
  const view = "testUpdateView";
  return {
    setUpAll: function () {
      let collectionProperties = {};
      if (isCluster) {
        collectionProperties = {numberOfShards: 9, replicationFactor: 3};
      }
      let c = db._create(collection, collectionProperties);
      c.save({_key: "279974", value: 0, flag: true});
    },

    tearDownAll: function () {
      db._dropView(view);
      db._drop(collection);
    },
    tearDown: function () {
      let c = db._collection(collection);
      c.update("279974", {value: 0, flag: true});
      db._dropView(view);
    },

    testSingleUpdateIncludeAllFields: function () {
      let c = db._collection(collection);
      db._createView(view, "arangosearch", {links: {testUpdate: {includeAllFields: true}}});

      // Run waitForSync query for view index initialization
      db._query("FOR d IN " + view + " SEARCH d.value >= 0 OPTIONS { waitForSync: true } RETURN d");
      
      for (let j = 1; j < 1000; j++) {
        // print("Iteration " + j);
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        // print(db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray());
        assertEqual(oldLen, 1);
        c.update("279974", {value: j, flag: true});
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        assertEqual(newLen, 1);
        c.update("279974", {value: j, flag: false});
      }
    },

    testSingleUpdateOneLinkedField: function () {
      let c = db._collection(collection);
      db._createView(view, "arangosearch", {links: {testUpdate: {"fields": {"value": {"analyzers": ["identity"]}}}}});

      c.update("279974", {value: 345, flag: true});
      // Run waitForSync query for view index initialization
      db._query("FOR d IN " + view + " SEARCH d.value >= 0 OPTIONS { waitForSync: true } RETURN d");      

      for (let j = 0; j < 100; j++) {
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(oldLen, 1);
        c.update("279974", {value: 345, flag: true});
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(newLen, 1);
        c.update("279974", {value: 345, flag: false});
      }
    },

    testSingleleUpdateUnlinkedField: function () {
      let c = db._collection(collection);
      c.update("279974", {value: 345, unlinkedField: 456, flag: true});

      db._createView(view, "arangosearch", {links: {testUpdate: {"fields": {"value": {"analyzers": ["identity"]}}}}});

      // Run waitForSync query for view index initialization
      db._query("FOR d IN " + view + " SEARCH d.value >= 0 OPTIONS { waitForSync: true } RETURN d");      
      
      for (let j = 0; j < 100; j++) {
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(oldLen, 1);
        c.update("279974", {unlinkedField: j, flag: true});
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(newLen, 1);
        c.update("279974", {unlinkedField: j, flag: false});
      }
    },

    testMultiUpdatesIncludeAllFields: function () {
      let collectionProperties = {};
      if (isCluster) {
        collectionProperties = {numberOfShards: 9, replicationFactor: 3};
      }

      const amountOfCollections = 3;
      const amountOfDocuments = 10;
      const documents = [];

      for (let i = 0; i < amountOfDocuments; i++) {
        documents.push({_key: `doc${i}`, value: i});
      }

      let v = db._createView(view, "arangosearch", {});
      const insertedDocs = [];
      for (let i = 0; i < amountOfCollections; i++) {
        db._create(`testUpdate${i}`, collectionProperties);
        insertedDocs.push(db[`testUpdate${i}`].insert(documents));
        v.properties({links: {[`testUpdate${i}`]: {includeAllFields: true}}});
      }

      // Run waitForSync query for view index initialization
      db._query("FOR d IN " + view + " SEARCH d.value >= 0 OPTIONS { waitForSync: true } RETURN d");

      for (let j = 1; j < 100; j++) {
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        assertEqual(oldLen, amountOfCollections * amountOfDocuments);
        for (let i = 0; i < amountOfCollections; i++) {
          for (let j = 0; j < amountOfDocuments; j++) {
            db[`testUpdate${i}`].update(`doc${j}`, {value: j, flag: true});
          }
        }
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        assertEqual(newLen, amountOfCollections * amountOfDocuments);
        for (let i = 0; i < amountOfCollections; i++) {
          for (let j = 0; j < amountOfDocuments; j++) {
            db[`testUpdate${i}`].update(`doc${j}`, {value: j, flag: false});
          }
        }
      }

      for (let i = 0; i < amountOfCollections; i++) {
        db._drop(`testUpdate${i}`, collectionProperties);
      }
    }
  };
}

jsunity.run(UpdateConsistencyArangoSearchView);

return jsunity.done();
