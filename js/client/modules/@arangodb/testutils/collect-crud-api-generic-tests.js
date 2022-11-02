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
const {assertEqual, assertTrue, assertNotEqual, assertFalse, assertNotUndefined}
  = jsunity.jsUnity.assertions;

// The post fix is used to make testName unique accross suites in the same file
// just to make testing-js happy.
const generateTestSuite = (collectionWrapper, testNamePostfix = "") => {
  assertTrue(collectionWrapper instanceof CollectionWrapper);

  const runAllCrudOperationsOnDocuments = (generator) => {
    const isEnterpriseGraphEdge = collectionWrapper._isEnterpriseGraphEdge;

    const collection = collectionWrapper.rawCollection();
    const cn = collection.name();
    const baseUrl = "/_api/document/" + encodeURIComponent(cn);
    for (const doc of generator) {
      let keyUrl;
      let documentKey;

      if (!isEnterpriseGraphEdge) {
        assertTrue(doc.hasOwnProperty("_key"));
        documentKey = doc._key;
        keyUrl = baseUrl + "/" + encodeURIComponent(doc._key);
      }

      // create document
      let result = arango.POST_RAW(baseUrl, doc);
      if (isEnterpriseGraphEdge) {
        assertTrue(result.parsedBody.hasOwnProperty("_key"));
        documentKey = result.parsedBody._key;
        keyUrl = baseUrl + "/" + encodeURIComponent(result.parsedBody._key);
      }
      assertEqual(202, result.code, `Creating document with key ${documentKey}`);
      let rev = result.parsedBody._rev;

      // read document back
      result = arango.GET_RAW(keyUrl);
      assertEqual(200, result.code, `Reading document with key ${documentKey}`);

      // Get cannot modify the revision
      assertEqual(result.parsedBody._rev, rev);

      // HEAD request
      result = arango.HEAD_RAW(keyUrl);
      assertEqual(200, result.code, `Head document with key ${documentKey}`);

      // PATCH request
      assertNotEqual(doc.test, "testmann");

      // Only send the part to patch
      const patch = {test: "testmann"};
      for (const [key, value] of Object.entries(patch)) {
        // Just make sure we are actually updating something here.
        // Existing entries should differ from the target
        assertNotEqual(doc[key], value);
      }
      result = arango.PATCH_RAW(keyUrl, patch);
      assertEqual(202, result.code, `Patching document with key ${documentKey}`);
      // Patch needs to update the revision
      assertNotEqual(result.parsedBody._rev, rev);
      {
        // Test patch was applied
        // read document back
        let result = arango.GET_RAW(keyUrl);
        assertEqual(200, result.code, `Reading document with key ${documentKey}`);
        for (const [key, value] of Object.entries(patch)) {
          // All values in Patch need to be updated
          assertEqual(result.parsedBody[key], value);
        }
        // Patch needs to update the revision
        assertNotEqual(result.parsedBody._rev, rev);
        // Retain the updated revision for later tests
        rev = result.parsedBody._rev;
      }
      // Locally apply the patch from before
      let updatedDoc = {...doc, ...patch};

      const replace = {test2: "testmann2"};
      for (const [key, value] of Object.entries(replace)) {
        // Just make sure we are actually updating something here.
        // Existing entries should differ from the target
        assertNotEqual(updatedDoc[key], value);
      }
      // Locally apply the replace
      updatedDoc = {...updatedDoc, ...replace};

      if (isEnterpriseGraphEdge) {
        // As in EnterpriseGraphs, the _key cannot be given during creation,
        // we have to insert it here into our original doc for the replace operation.
        updatedDoc._key = documentKey;
      }

      result = arango.PUT_RAW(keyUrl, updatedDoc);
      assertEqual(202, result.code, `Replacing document with key "${documentKey}"`);
      // Replace needs to update the revision
      assertNotEqual(result.parsedBody._rev, rev);

      {
        // Local Document and DBState now need to be identical
        // read document back
        let result = arango.GET_RAW(keyUrl);
        assertEqual(200, result.code, `Reading document with key "${documentKey}"`);
        for (const [key, value] of Object.entries(updatedDoc)) {
          // All values in Patch need to be updated
          assertEqual(result.parsedBody[key], value);
        }
        // Patch needs to update the revision
        assertNotEqual(result.parsedBody._rev, rev);
        // Retain the updated revision for later tests
        rev = result.parsedBody._rev;
      }


      // Check if count is now 1, we have only inserted Once, afterwards only read
      assertEqual(1, collection.count());

      // remove document
      result = arango.DELETE_RAW(keyUrl);
      assertEqual(202, result.code, `Deleting document with key ${documentKey}`);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief create document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    [`testValidKeys${testNamePostfix}`]: function () {
      // This is some basic tests for easy characters in URLs
      runAllCrudOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.validKeyGenerator()));
    },

    [`testSpecialKeysInUrls${testNamePostfix}`]: function () {
      // This is supposed to test special characters in URLs
      runAllCrudOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.specialKeyGenerator()));
    },

    /// @brief this tests the SimpleQueries lookupByKeys and removeByKeys
    [`testLookupByKeys${testNamePostfix}`]: function () {
      const collection = collectionWrapper.rawCollection();
      const cn = collection.name();
      const baseUrl = "/_api/document/" + encodeURIComponent(cn);
      const documents = [];
      const isEnterpriseGraphEdge = collectionWrapper._isEnterpriseGraphEdge;
      for (const doc of collectionWrapper.documentGeneratorWithKeys(collectionWrapper.validKeyGenerator())) {
        documents.push(doc);
      }

      let keys = [];
      {
        // create all documents in one call
        const result = arango.POST_RAW(baseUrl, documents);
        assertEqual(202, result.code);
        assertFalse(
          result.headers.hasOwnProperty("x-arango-error-codes"),
          `Got errors on insert: ${JSON.stringify(result.headers["x-arango-error-codes"])}`
        );
        // Validate all documents are successfully created
        assertEqual(documents.length, collection.count());

        if (isEnterpriseGraphEdge) {
          // Keys are not provided by us, they are auto-generated.
          keys = result.parsedBody.map(d => d._key);
        } else {
          keys = documents.map(d => d._key);
        }
      }
      const sortByKey = (l, r) => {
        if (l._key < r._key) {
          return -1;
        }
        if (l._key > r._key) {
          return 1;
        }
        return 0;
      };
      {
        // Try to find one by key
        // This defaults to a SingleRemoteOperation
        const lookupUrl = "/_api/simple/lookup-by-keys";
        assertNotUndefined(keys.slice(0, 1));
        const body = {
          collection: collection.name(),
          keys: keys.slice(0, 1)
        };
        const result = arango.PUT_RAW(lookupUrl, body);

        assertEqual(200, result.code);
        const response = result.parsedBody.documents;
        assertEqual(1, response.length);

        for (const [key, value] of Object.entries(documents[0])) {
          assertEqual(response[0][key], value,
            `Mismatch user data of ${JSON.stringify(response[0])} does not match the insert ${JSON.stringify(documents[0])}`);
        }
      }
      // The response does not have to be sorted by input keys.
      documents.sort(sortByKey);
      {
        // Try to find them by keys
        // This defaults to an IndexLookup
        const lookupUrl = "/_api/simple/lookup-by-keys";
        const body = {
          collection: collection.name(),
          keys
        };

        const result = arango.PUT_RAW(lookupUrl, body);

        assertEqual(200, result.code);
        const response = result.parsedBody.documents;
        response.sort(sortByKey);
        assertEqual(response.length, documents.length);
        for (let i = 0; i < documents.length; ++i) {
          for (const [key, value] of Object.entries(documents[i])) {
            assertEqual(response[i][key], value,
              `Mismatch at document ${i} user data of ${JSON.stringify(response[i])} does not match the insert ${JSON.stringify(documents[i])}`);
          }
        }
      }

      {
        // Try to remove them by keys
        const removeUrl = "/_api/simple/remove-by-keys";
        const body = {
          collection: collection.name(),
          keys
        };
        const result = arango.PUT_RAW(removeUrl, body);
        if (result.code !== 200) {
          console.warn(`Remove returned with: ${JSON.stringify(result)}`);
        }
        assertEqual(200, result.code);
        // Validate all documents are successfully removed
        assertEqual(0, collection.count());
      }
    },
  };
};

exports.generateTestSuite = generateTestSuite;