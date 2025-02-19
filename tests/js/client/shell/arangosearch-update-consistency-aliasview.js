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

function UpdateConsistencySearchAliasView() {
  const collection = "testUpdate";
  const view = "testUpdateView";
  return {
    setUpAll: function () {
      let collectionProperties = {};
      if (isCluster) {
        collectionProperties = {numberOfShards: 9, replicationFactor: 3};
      }
      let c = db._create(collection, collectionProperties);
      c.save({_key: "279974", value: 345, flag: true});
      c.ensureIndex({name: "inverted_index", type: "inverted", fields: ["value"]});
      let v = db._createView(view, "search-alias", {});
    },

    tearDownAll: function () {
      db._dropView(view);
      db._drop(collection);
    },

    testInvertedSingleUpdateLinkedField: function () {
      let c = db._collection(collection);
      let v = db._view(view);
      v.properties({indexes: [{collection: "testUpdate", index: "inverted_index"}]});

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


    testMultiUpdates: function () {
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

      let v = db._view(view);
      const insertedDocs = [];
      for (let i = 0; i < amountOfCollections; i++) {
        let c = db._create(`testUpdate${i}`, collectionProperties);
        c.ensureIndex({name: "inverted_index", type: "inverted", fields: ["value"]});
        insertedDocs.push(db[`testUpdate${i}`].insert(documents));
        v.properties({indexes: [{collection: `testUpdate${i}`, index: "inverted_index"}]});
      }

      // Run waitForSync query for view index initialization
      db._query("FOR d IN " + view + " SEARCH d.value >= 0 OPTIONS { waitForSync: true } RETURN d");

      for (let j = 1; j < 100; j++) {
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        // print(db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray());
        assertEqual(oldLen, amountOfCollections * amountOfDocuments + 1);
        for (let i = 0; i < amountOfCollections; i++) {
          for (let j = 0; j < amountOfDocuments; j++) {
            db[`testUpdate${i}`].update(`doc${j}`, {value: j, flag: true});
          }
        }
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value >= 0 RETURN d").toArray().length;
        assertEqual(newLen, amountOfCollections * amountOfDocuments + 1);
        for (let i = 0; i < amountOfCollections; i++) {
          for (let j = 0; j < amountOfDocuments; j++) {
            db[`testUpdate${i}`].update(`doc${j}`, {value: j, flag: false});
          }
        }
      }

      for (let i = 0; i < amountOfCollections; i++) {
        db._drop(`testUpdate${i}`, collectionProperties);
      }
    },

    testInvertedSingleleUpdateUnlinkedField: function () {
      // we need to create our own view, since we can't modify it.
      db._dropView(view);
      let v = db._createView(view, "search-alias", {});
      let c = db._collection(collection);
      v.properties({indexes: [{collection: collection, index: "inverted_index"}]});

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
  };
}

jsunity.run(UpdateConsistencySearchAliasView);

return jsunity.done();
