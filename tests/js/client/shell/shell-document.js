/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global arango, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the document interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;

function CollectionDocumentKeysSuite () {
  'use strict';

  var cn = "UnitTestsCollection";
  var collection = null;
      
  var keys = [ 
    ":", "-", "_", "@", ".", "..", "...", "a@b", "a@b.c", "a-b-c", "_a", "@a", "@a-b", ":80", ":_", "@:_",
    "0", "1", "123456", "0123456", "true", "false", "a", "A", "a1", "A1", "01ab01", "01AB01",
    "abcd-efgh", "abcd_efgh", "Abcd_Efgh", "@@", "abc@foo.bar", "@..abc-@-foo__bar", 
    ".foobar", "-foobar", "_foobar", "@foobar", "(valid)", "%valid", "$valid",
    "$$bill,y'all", "'valid", "'a-key-is-a-key-is-a-key'", "m+ller", ";valid", ",valid", "!valid!",
    ":::", ":-:-:", ";", ";;;;;;;;;;", "(", ")", "()xoxo()", "%", ".::.", "::::::::........",
    "%-%-%-%", ":-)", "!", "!!!!", "'", "''''", "this-key's-valid.", "=",
    "==================================================", "-=-=-=___xoxox-",
    "*", "(*)", "****", "--", "__" 
  ];

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testSaveSpecialKeysInUrls : function () {
      keys.forEach(function(key, index) {
        var doc = { _key: key, value: "test" };

        // create document
        var result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), doc);
        assertEqual(202, result.code);

        // read document back
        result = arango.GET_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key));
        assertEqual(200, result.code);

        assertEqual(index + 1, collection.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testGetSpecialKeysInUrls : function () {
      keys.forEach(function(key, index) {
        var doc = { _key: key, value: "test" };

        // create document
        var result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), doc);
        assertEqual(202, result.code);

        // read document back
        result = arango.HEAD_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key));
        assertEqual(200, result.code);

        assertEqual(index + 1, collection.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testUpdateSpecialKeysInUrls : function () {
      keys.forEach(function(key, index) {
        var doc = { _key: key, value: "test" };

        // create document
        var result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), doc);
        assertEqual(202, result.code);

        // update document
        doc.test = "testmann";
        doc.value = 12345;

        result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key), doc);
        assertEqual(202, result.code);
        
        assertEqual(index + 1, collection.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testReplaceSpecialKeysInUrls : function () {
      keys.forEach(function(key, index) {
        var doc = { _key: key, value: "test" };

        // create document
        var result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), doc);
        assertEqual(202, result.code);

        // update document
        doc.test = "testmann";
        doc.value = 12345;

        result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key), doc);
        assertEqual(202, result.code);
        
        assertEqual(index + 1, collection.count());
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove document w/ special keys
////////////////////////////////////////////////////////////////////////////////

    testRemoveSpecialKeysInUrls : function () {
      keys.forEach(function(key, index) {
        var doc = { _key: key, value: "test" };

        // create document
        var result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), doc);
        assertEqual(202, result.code);

        // remove document
        result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key));
        assertEqual(202, result.code);
        
        assertEqual(0, collection.count());
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentKeysSuite);

return jsunity.done();

