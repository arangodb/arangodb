/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertEqual, assertTrue, assertNotNull, assertNotEqual, fail*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const dbs = ["testDatabase", "abc123", "maçã", "mötör", "😀", "ﻚﻠﺑ ﻞﻄﻴﻓ", "かわいい犬", "let%2Fus%26test%3Fthis"];

function EvilDatabaseNamesSuite () {
  'use strict';
      
  return {

    setUp : function () {
      dbs.forEach((database) => {
        db._createDatabase(database);
      });
    },

    tearDown : function () {
      db._useDatabase("_system");
      dbs.forEach((database) => {
        db._dropDatabase(database);
      });
    },

    testCollectionCreationDeletion: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        db._create("collection123");
        assertNotNull(db._collection("collection123"));
        assertNotEqual(db._collections().map((collection) => collection.name()).indexOf("collection123"), -1);
        db._drop("collection123");
        assertEqual(db._collections().map((collection) => collection.name()).indexOf("collection123"), -1);
      });
    },

    testDocumentCreationDeletion: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        db._create("collection123");
        assertNotNull(db._collection("collection123"));
        const obj = db["collection123"].insert({"name": "abc123"});
        assertEqual(db["collection123"].count(), 1);
        db["collection123"].update(obj._id, {"test": "123"});
        const documentObj = db["collection123"].document(obj._id);
        assertEqual(documentObj.name, "abc123");
        assertEqual(documentObj.test, "123");
        assertTrue(db["collection123"].exists(obj._id));
        db["collection123"].remove(obj._id);
        try {
          db["collection123"].document(obj._id);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, arangodb.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
        }
      });
    },

    testViewCreationDeletion: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        const view = db._createView("view123", "arangosearch", {});
        const views = db._views().map((view) => view.name());
        assertNotEqual(views.indexOf("view123"), -1);
        view.properties({cleanupIntervalStep: 12});
        assertEqual(view.properties().cleanupIntervalStep, 12);
        db._dropView("view123");
        assertEqual(db._views(), []);
      });
    },

    testAnalyzerCreationDeletion: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        const analyzers = require("@arangodb/analyzers");
        const analyzer = analyzers.save("abc123", "identity", {});
        let  analyzerNames = analyzers.toArray().map((analyzer) => analyzer.name());
        assertNotEqual(analyzerNames.indexOf(database + "::abc123"), -1);
        analyzers.remove("abc123");
        analyzerNames = analyzers.toArray().map((analyzer) => analyzer.name());
        assertEqual(analyzerNames.indexOf(database + "::abc123"), -1);
      });
    },

    testIndexCreationDeletion: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        db._create("collection123");
        assertNotNull(db._collection("collection123"));
        db["collection123"].ensureIndex({type: 'persistent', name: 'test123', fields: ['value']});
        assertEqual(db["collection123"].indexes().length, 2);
        assertEqual(db["collection123"].index("test123").name, "test123");
        db["collection123"].dropIndex("test123");
        assertEqual(db["collection123"].indexes().length, 1);
        try{
          assertEqual(db["collection123"].index("test123"));
          fail();
        } catch(err) {
          assertEqual(err.errorNum, arangodb.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code);
        }

      });
    },

    testAqlQueries: function () {
      dbs.forEach((database) => {
        db._useDatabase(database);
        db._create("collection123");
        assertNotNull(db._collection("collection123"));
        let result = db._query('INSERT {"name": "abc", "id": 123, "_key": "abc123"} INTO collection123 RETURN NEW').toArray();
        assertEqual(db["collection123"].count(), 1);
        assertEqual(result[0]._key, "abc123");
        assertEqual(result[0].id, 123);
        assertEqual(db["collection123"].document("abc123").name, "abc");
        assertEqual(db["collection123"].document("abc123").id, 123);
        result = db._query('REPLACE "abc123" WITH {"name": "abc123", "id": 1} IN collection123 RETURN NEW').toArray();
        assertEqual(result[0].id, 1);
        result = db._query('REMOVE "abc123" IN collection123 RETURN OLD').toArray();
        assertEqual(result[0].id, 1);
        assertEqual(db["collection123"].count(), 0);

        //testing for larger result sets
        result = db._query('FOR n IN 1 .. 2000 INSERT {"name": CONCAT("abc", n), "id": n, "_key": CONCAT("abc", n)} INTO collection123 RETURN NEW').toArray();
        assertEqual(db["collection123"].count(), 2000);
        assertEqual(result.length, 2000);
        result.forEach((documentObj, index) => {
          assertEqual(documentObj._key, "abc" + (index + 1));
          assertEqual(documentObj.id, index + 1);
        });
      });
    }

  };
}

jsunity.run(EvilDatabaseNamesSuite);

return jsunity.done();

