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


const generateTestSuite = (collectionWrapper) => {
  assertTrue(collectionWrapper instanceof CollectionWrapper);

  const runAllCrudOperationsOnDocuments = (generator) => {
    const collection = collectionWrapper.rawCollection();
    const cn = collection.name();
    const baseUrl = "/_api/document/" + encodeURIComponent(cn);
    for (const doc of generator) {
      assertTrue(doc.hasOwnProperty("_key"));
      const keyUrl = baseUrl + "/" + encodeURIComponent(doc._key);
      // create document
      let result = arango.POST_RAW(baseUrl, doc);
      assertEqual(202, result.code, `Creating document with key ${doc._key}`);
      let rev = result.parsedBody._rev;

      // read document back
      result = arango.GET_RAW(keyUrl);
      assertEqual(200, result.code, `Reading document with key ${doc._key}`);

      // Get cannot modify the revision
      assertEqual(result.parsedBody._rev, rev);

      // HEAD request
      result = arango.HEAD_RAW(keyUrl);
      assertEqual(200, result.code, `Head document with key ${doc._key}`);

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
      assertEqual(202, result.code, `Patching document with key ${doc._key}`);
      // Patch needs to update the revision
      assertNotEqual(result.parsedBody._rev, rev);
      {
        // Test patch was applied
        // read document back
        let result = arango.GET_RAW(keyUrl);
        assertEqual(200, result.code, `Reading document with key ${doc._key}`);
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

      result = arango.PUT_RAW(keyUrl, updatedDoc);
      assertEqual(202, result.code, `Replacing document with key ${doc._key}`);
      // Replace needs to update the revision
      assertNotEqual(result.parsedBody._rev, rev);

      {
        // Local Document and DBState now need to be identical
        // read document back
        let result = arango.GET_RAW(keyUrl);
        assertEqual(200, result.code, `Reading document with key ${doc._key}`);
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
      assertEqual(202, result.code, `Deleting document with key ${doc._key}`);

      // No document in collection anymore
      assertEqual(0, collection.count());
    }
  };

  function GenericCrudTestSuite() {
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

      testValidKeys: function () {
        // This is some basic tests for easy characters in URLs
        runAllCrudOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.validKeyGenerator()));
      },

      testSpecialKeysInUrls: function () {
        // This is supposed to test special characters in URLs
        runAllCrudOperationsOnDocuments(collectionWrapper.documentGeneratorWithKeys(collectionWrapper.specialKeyGenerator()));
      },
    };
  }

  return GenericCrudTestSuite;
};


exports.generateTestSuite = generateTestSuite;