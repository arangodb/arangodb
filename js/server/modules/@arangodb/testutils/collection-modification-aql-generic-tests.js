/*jshint globalstrict:true, strict:true, esnext: true */
/*global arango */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {CollectionWrapper} = require("@arangodb/testutils/collection-wrapper-util");
const {assertEqual, assertTrue, assertNotEqual}
  = jsunity.jsUnity.assertions;
const {db} = require("@arangodb");

// The post fix is used to make testName unique accross suites in the same file
// just to make testing-js happy.
const generateTestSuite = (collectionWrapper, testNamePostfix = "") => {
  assertTrue(collectionWrapper instanceof CollectionWrapper);
  const runAllSingleDocumentOperationsOnDocuments = (generator) => {
    const isEnterpriseGraphEdge = collectionWrapper._isEnterpriseGraphEdge;
    const collection = collectionWrapper.rawCollection();
    const cn = collection.name();
    const insertAql = `INSERT
    @doc INTO
    ${cn}
    RETURN
    NEW`;

    const readAql = `FOR c IN ${cn} FILTER c._key == @key RETURN c`;
    const generateReadByDocumentAql = (cn, key) => {
      const id = cn + '/' + key;
      return `RETURN DOCUMENT("${id}")`;
    };
    const updateAql = `UPDATE @key WITH @patch IN ${cn} RETURN NEW`;
    const replaceAql = `REPLACE @key WITH @updatedDoc IN ${cn} RETURN NEW`;
    const removeAql = `REMOVE @key IN ${cn}`;
    for (const doc of generator) {
      let key;

      if (!isEnterpriseGraphEdge) {
        assertTrue(doc.hasOwnProperty("_key"));
        key = doc._key;
      }

      // create document
      let result = db._query(insertAql, {doc}).toArray();
      if (isEnterpriseGraphEdge) {
        assertTrue(result[0].hasOwnProperty("_key"));
        key = result[0]._key;
      }
      assertEqual(1, result.length, `Creating document with key ${key}`);
      let rev = result[0]._rev;

      // read document via document function by key (string)
      result = db._query(generateReadByDocumentAql(cn, key)).toArray();
      assertEqual(1, result.length, `Reading document with key ${key} via DOCUMENT function.`);

      // read document back (collection scan + filter)
      result = db._query(readAql, {key}).toArray();
      assertEqual(1, result.length, `Reading document with key ${key}`);

      // Get cannot modify the revision
      assertEqual(result[0]._rev, rev);

      // PATCH request
      assertNotEqual(doc.test, "testmann");

      // Only send the part to patch
      const patch = {test: "testmann"};
      for (const [key, value] of Object.entries(patch)) {
        // Just make sure we are actually updating something here.
        // Existing entries should differ from the target
        assertNotEqual(doc[key], value);
      }

      result = db._query(updateAql, {key, patch}).toArray();
      assertEqual(1, result.length, `Patching document with key ${key}`);
      // Patch needs to update the revision
      assertNotEqual(result[0]._rev, rev);
      {
        // Test patch was applied
        // read document back
        result = db._query(readAql, {key}).toArray();
        assertEqual(1, result.length, `Reading document with key ${key}`);

        for (const [key, value] of Object.entries(patch)) {
          // All values in Patch need to be updated
          assertEqual(result[0][key], value);
        }

        // The new revision needs to be persisted
        assertNotEqual(result[0]._rev, rev);

        // Retain the updated revision for later tests
        rev = result[0]._rev;
      }
      // Locally apply the patch from before
      let updatedDoc = {...doc, ...patch};

      if (isEnterpriseGraphEdge) {
        // As in EnterpriseGraphs, the _key cannot be given during creation,
        // we have to insert it here into our original doc for the replace operation.
        updatedDoc._key = key;
      }

      const replace = {test2: "testmann2"};
      for (const [key, value] of Object.entries(replace)) {
        // Just make sure we are actually updating something here.
        // Existing entries should differ from the target
        assertNotEqual(updatedDoc[key], value);
      }
      // Locally apply the replace
      updatedDoc = {...updatedDoc, ...replace};

      result = db._query(replaceAql, {key, updatedDoc}).toArray();
      assertEqual(1, result.length, `Replacing document with key ${key}`);
      // Replace needs to update the revision
      assertNotEqual(result[0]._rev, rev);
      {
        // Local Document and DBState now need to be identical
        // read document back
        result = db._query(readAql, {key}).toArray();
        assertEqual(1, result.length, `Reading document with key ${key}`);

        for (const [key, value] of Object.entries(updatedDoc)) {
          // All values in Patch need to be updated
          assertEqual(result[0][key], value);
        }

        // The new revision needs to be persisted
        assertNotEqual(result[0]._rev, rev);

        // Retain the updated revision for later tests
        rev = result[0]._rev;
      }

      // Check if count is now 1, we have only inserted Once, afterwards only read
      assertEqual(1, collection.count());

      // remove document
      result = db._query(removeAql, {key}).toArray();

      // No document in collection anymore
      assertEqual(0, collection.count());
    }
  };

  return {
    setUpAll: function () {
      collectionWrapper.tearDown();
      collectionWrapper.setUp();
    },

    setUp: function () {
      collectionWrapper.clear();
    },
    tearDownAll: function () {
      collectionWrapper.tearDown();
    },

    [`testValidKeys${testNamePostfix}`]: function () {
      // This is some basic tests for easy characters in URLs
      runAllSingleDocumentOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.validKeyGenerator()));
    },

    [`testSpecialKeysInUrls${testNamePostfix}`]: function () {
      // This is supposed to test special characters in URLs
      runAllSingleDocumentOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.specialKeyGenerator()));
    },
  };
};

exports.generateTestSuite = generateTestSuite;
