/*jshint globalstrict:false, strict:false, unused : false */
/*global fail, assertTrue, assertFalse, assertEqual, assertMatch */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: dropping databases while holding references
////////////////////////////////////////////////////////////////////////////////

let logLevel;
function DatabaseSuite () {
  'use strict';
  return {

    setUp : function () {
      logLevel = require("internal").logLevel();
      internal.db._useDatabase("_system");
    },

    tearDown : function () {
      require("internal").logLevel(logLevel);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether the expected keys are present in db._version(true)
////////////////////////////////////////////////////////////////////////////////

    testVersion : function () {
      let result = internal.db._version(true);

      assertEqual(result.server, "arango");
      assertEqual(result.license, (internal.isEnterprise() ? "enterprise" : "community"));
      assertEqual(result.version, internal.db._version());
      assertTrue(result.hasOwnProperty("details"));
    },
    
    testVersionDetails : function () {
      let result = internal.db._version(true).details;

      let keys = [
        "architecture",
        "asan",
        "assertions",
        "boost-version",
        "build-date",
        "compiler",
        "cplusplus",
        "debug",
        "endianness",
        "failure-tests",
        "full-version-string",
        "icu-version",
        "jemalloc",
        "maintainer-mode",
        "openssl-version-run-time",
        "openssl-version-compile-time",
        "platform",
        "reactor-type",
        "rocksdb-version",
        "server-version",
        "sse42",
        "unaligned-access",
        "v8-version",
        "vpack-version",
        "zlib-version"
      ];

      keys.forEach(function(k) {
        assertTrue(result.hasOwnProperty(k), k);
      });
    },

    testVersionBooleans : function () {
      let result = internal.db._version(true).details;

      let keys = [
        "asan",
        "assertions",
        "debug",
        "failure-tests",
        "jemalloc",
        "maintainer-mode",
        "sse42",
        "unaligned-access"
      ];

      keys.forEach(function(k) {
        assertTrue(result[k] === "true" || result[k] === "false");
      });
    },

    testVersionNumbers : function () {
      let result = internal.db._version(true).details;

      let keys = [
        "boost-version",
        "icu-version",
        "rocksdb-version",
        "server-version",
        "v8-version",
        "vpack-version",
        "zlib-version"
      ];

      keys.forEach(function(k) {
        assertMatch(/^\d+(\.\d+)*([\-]([a-z]|\d)+(\.?\d*)?)?$/, result[k]);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references helt on dropped database collections
////////////////////////////////////////////////////////////////////////////////

    testDropDatabaseCollectionReferences : function () {
      assertEqual("_system", internal.db._name());

      assertTrue(internal.db._createDatabase("UnitTestsDatabase0"));

      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());

      // insert 1000 docs and hold a reference on the data
      let c = internal.db._create("test");
      for (let i = 0; i < 1000; ++i) {
        c.insert({ "_key": "test" + i, "value" : i });
      }
      assertEqual(1000, c.count());

      internal.db._useDatabase("_system");
      assertEqual(1000, c.count());

      let cid = c._id;

      // drop the database
      internal.db._dropDatabase("UnitTestsDatabase0");
      // should be dropped
      assertFalse(internal.db._databases().includes("UnitTestsDatabase0"));

      // collection should not be there anymore for counting and properties calls
      try {
        c.count();
        fail();
      } catch (err) {
        assertTrue(
          internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code === err.errorNum ||
          internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code === err.errorNum, err.errorNum);
      }
      
      try {
        c.properties();
        fail();
      } catch (err) {
        assertTrue(
          internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code === err.errorNum ||
          internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code === err.errorNum, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references helt on documents of dropped databases
////////////////////////////////////////////////////////////////////////////////

    testDropDatabaseDocumentReferences : function () {
      assertEqual("_system", internal.db._name());

      assertTrue(internal.db._createDatabase("UnitTestsDatabase0"));

      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());

      // insert docs and hold a reference on the data
      let c = internal.db._create("test");
      for (let i = 0; i < 10; ++i) {
        c.insert({ "_key": "test" + i, "value" : i });
      }

      let d0 = c.document("test0");
      let d4 = c.document("test4");
      let d9 = c.document("test9");

      c = null;

      internal.db._useDatabase("_system");

      // drop the database
      internal.db._dropDatabase("UnitTestsDatabase0");
      // should be dropped
      assertFalse(internal.db._databases().includes("UnitTestsDatabase0"));

      assertEqual(0, d0.value);
      assertEqual(4, d4.value);
      assertEqual(9, d9.value);
    },

  };
}

jsunity.run(DatabaseSuite);

return jsunity.done();
