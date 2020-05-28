/*jshint globalstrict:false, strict:false */
/*global arango, assertEqual, assertTrue, assertEqual, assertMatch, fail, ArangoClusterComm */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the traditional key generators
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var db = arangodb.db;
var ERRORS = arangodb.errors;
let cluster = require("internal").isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: traditional key gen
////////////////////////////////////////////////////////////////////////////////

function TraditionalSuite () {
  'use strict';
  var cn = "UnitTestsKeyGen";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKeyNonDefaultSharding1 : function () {
      var c = db._create(cn, { shardKeys: ["value"], keyOptions: { type: "traditional", allowUserKeys: false } });

      try {
        c.save({ _key: "1234" }); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
                   err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },
    
    testCreateInvalidKeyNonDefaultSharding2 : function () {
      if (!cluster) {
        return;
      }

      var c = db._create(cn, { shardKeys: ["value"], keyOptions: { type: "traditional", allowUserKeys: true } });

      try {
        c.save({ _key: "1234" }); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
                   err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },
    
    testCreateKeyNonDefaultSharding : function () {
      if (!cluster) {
        return;
      }

      var c = db._create(cn, { shardKeys: ["value"], keyOptions: { type: "traditional", allowUserKeys: true } });

      let key = c.save({ value: "1" }); // no user keys allowed
      let doc = c.document(key);
      assertEqual("1", doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey1 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: false } });

      try {
        c.save({ _key: "1234" }); // no user keys allowed
        fail();
      } catch (err) {
        assertTrue(err.errorNum === ERRORS.ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED.code ||
                   err.errorNum === ERRORS.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with key
////////////////////////////////////////////////////////////////////////////////

    testCreateInvalidKey2 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: true } });

      try {
        c.save({ _key: "öä .mds 3 -6" }); // invalid key
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk1 : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional" } });

      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk2 : function () {
      var c = db._create(cn, { keyOptions: { } });

      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with valid properties
////////////////////////////////////////////////////////////////////////////////

    testCreateOk3 : function () {
      var c = db._create(cn, { keyOptions: { allowUserKeys: false } });

      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(false, options.allowUserKeys);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create with user key
////////////////////////////////////////////////////////////////////////////////

    testCheckUserKey : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional", allowUserKeys: true } });

      var options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertEqual(true, options.allowUserKeys);

      var d1 = c.save({ _key: "1234" });
      assertEqual("1234", d1._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check auto values
////////////////////////////////////////////////////////////////////////////////

    testCheckAutoValues : function () {
      var c = db._create(cn, { keyOptions: { type: "traditional" } });

      var d1 = parseFloat(c.save({ })._key);
      var d2 = parseFloat(c.save({ })._key);
      var d3 = parseFloat(c.save({ })._key);
      var d4 = parseFloat(c.save({ })._key);
      var d5 = parseFloat(c.save({ })._key);

      assertTrue(d1 < d2);
      assertTrue(d2 < d3);
      assertTrue(d3 < d4);
      assertTrue(d4 < d5);
    },
    
    testInvalidKeyGenerator : function () {
      try {
        db._create(cn, { keyOptions: { type: "der-fuchs" } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_INVALID_KEY_GENERATOR.code, err.errorNum);
      }
    },

    testAutoincrementGeneratorInCluster : function () {
      if (!cluster) {
        return;
      }

      try {
        db._create(cn, { keyOptions: { type: "autoincrement" } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_UNSUPPORTED.code, err.errorNum);
      }
    },

    testUuid : function () {
      let c = db._create(cn, { keyOptions: { type: "uuid" } });

      let options = c.properties().keyOptions;
      assertEqual("uuid", options.type);
      assertTrue(options.allowUserKeys);

      for (let i = 0; i < 100; ++i) {
        let doc = c.insert({});
        assertMatch(/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/, doc._key);
      }
    },

    testPadded : function () {
      let c = db._create(cn, { keyOptions: { type: "padded" } });

      let options = c.properties().keyOptions;
      assertEqual("padded", options.type);
      assertTrue(options.allowUserKeys);

      for (let i = 0; i < 100; ++i) {
        let doc = c.insert({});
        assertEqual(16, doc._key.length);
        assertMatch(/^[0-9a-f]{16}$/, doc._key);
      }
    },
    
    testTraditionalTracking : function () {
      let c = db._create(cn, { keyOptions: { type: "traditional" } });

      let options = c.properties().keyOptions;
      assertEqual("traditional", options.type);
      assertTrue(options.allowUserKeys);

      let lastKey = null;
      for (let i = 0; i < 100; ++i) {
        let key = c.insert({})._key;
        if (lastKey !== null) {
          assertTrue(Number(key) > Number(lastKey));
        }
        lastKey = key;
      }

      if (!cluster) {
        assertEqual(lastKey, c.properties().keyOptions.lastValue);
      }
    },

    testPaddedTracking : function () {
      let c = db._create(cn, { keyOptions: { type: "padded" } });

      let options = c.properties().keyOptions;
      assertEqual("padded", options.type);
      assertTrue(options.allowUserKeys);

      let lastKey = null;
      for (let i = 0; i < 100; ++i) {
        let key = c.insert({})._key;
        assertEqual(16, key.length);
        if (lastKey !== null) {
          assertTrue(key > lastKey);
        }
        lastKey = key;
      }

      if (!cluster) {
        let hex = c.properties().keyOptions.lastValue.toString(16);
        assertEqual(lastKey, Array(16 + 1 - hex.length).join("0") + hex);
      }
    }

  };
}

jsunity.run(TraditionalSuite);

return jsunity.done();
