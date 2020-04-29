/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertMatch, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the common database interface
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: database methods
////////////////////////////////////////////////////////////////////////////////

function DatabaseSuite () {
  'use strict';
  return {

    setUp : function () {
      internal.db._useDatabase("_system");
    },

    tearDown : function () {
      // always go back to system database
      internal.db._useDatabase("_system");

      try {
        // drop this database if it exists
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err) {
        // ignore
      }

      // trigger GC to remove databases physically
      internal.wait(0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabase : function () {
      // run this early in the test setup for maximal stress.
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      try {
        internal.db._dropDatabase("UnitTestsDatabase1");
      } catch (err2) {
      }

      assertTrue(internal.db._createDatabase("UnitTestsDatabase0"));
      assertTrue(internal.db._createDatabase("UnitTestsDatabase1"));

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
      assertTrue(internal.db._dropDatabase("UnitTestsDatabase1"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test create with too long names function
////////////////////////////////////////////////////////////////////////////////

    testLongName : function () {
      const prefix = "UnitTestsDatabase";
      let name = prefix + Array(64 + 1 - prefix.length).join("x");
      assertEqual(64, name.length);
          
      assertTrue(internal.db._createDatabase(name));
      assertTrue(internal.db._dropDatabase(name));

      name += 'x';
      assertEqual(65, name.length);

      try {
        internal.db._createDatabase(name);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _name function
////////////////////////////////////////////////////////////////////////////////

    testName : function () {
      assertEqual("_system", internal.db._name());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _isSystem function
////////////////////////////////////////////////////////////////////////////////

    testIsSystem : function () {
      assertTrue(typeof internal.db._isSystem() === "boolean");
      assertTrue(internal.db._isSystem());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function
////////////////////////////////////////////////////////////////////////////////

    testQueryMemoryLimit : function () {
      try {
        internal.db._query("FOR i IN 1..100000 SORT i RETURN i", {}, { memoryLimit: 100000 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function
////////////////////////////////////////////////////////////////////////////////

    testQueryMemoryLimitSufficient : function () {
      assertEqual(10000, internal.db._query("FOR i IN 1..10000 RETURN i", {}, { memoryLimit: 100000 + 4096 }).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function
////////////////////////////////////////////////////////////////////////////////

    testQueryMemoryLimitUnbounded : function () {
      assertEqual(10000, internal.db._query("FOR i IN 1..10000 SORT i RETURN i", {}, { memoryLimit: 0 }).toArray().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function
////////////////////////////////////////////////////////////////////////////////

    testQuery : function () {
      assertEqual([ 1 ], internal.db._query("return 1").toArray());
      assertEqual([ [ 1, 2, 9, "foo" ] ], internal.db._query("return [ 1, 2, 9, \"foo\" ]").toArray());
      assertEqual([ [ 1, 454 ] ], internal.db._query("return [ @low, @high ]", { low : 1, high : 454 }).toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function
////////////////////////////////////////////////////////////////////////////////

    testQueryObject : function () {
      assertEqual([ 1 ], internal.db._query({ query: "return 1" }).toArray());
      assertEqual([ [ 1, 2, 9, "foo" ] ], internal.db._query({ query: "return [ 1, 2, 9, \"foo\" ]" }).toArray());
      var obj = { query: "return [ @low, @high ]", bindVars: { low : 1, high : 454 } };
      assertEqual([ [ 1, 454 ] ], internal.db._query(obj).toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function with AQB object
////////////////////////////////////////////////////////////////////////////////

    testQueryAQB : function () {
      var qb = require("aqb");
      assertEqual([ 1 ], internal.db._query(qb.return(1)).toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _query function with AQB
////////////////////////////////////////////////////////////////////////////////

    testQueryAQBBindParameter : function () {
      var qb = require("aqb");
      assertEqual([ 1 ], internal.db._query(qb.return("@x"), { x: 1 }).toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test query helper
////////////////////////////////////////////////////////////////////////////////

    testQueryHelper : function () {
      var query = require("@arangodb").query;
      assertEqual([ 1 ], query`RETURN 1`.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test query helper as function
////////////////////////////////////////////////////////////////////////////////

    testQueryHelperAsFunction : function () {
      var query = require("@arangodb").query;
      assertEqual([ 1 ], query()`RETURN 1`.toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test query helper with options
////////////////////////////////////////////////////////////////////////////////

    testQueryHelperWithOptions : function () {
      var query = require("@arangodb").query;
      var result = query({fullCount: true})`RETURN 1`;
      assertEqual([ 1 ], result.toArray());
      assertEqual(1, result.getExtra().stats.fullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL literal
////////////////////////////////////////////////////////////////////////////////

    testAQLWithLiteral : function () {
      var aql = require("@arangodb").aql;
      assertEqual([ 1 ], internal.db._query(aql`${aql.literal('RETURN 1')}`).toArray());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test AQL literal and options
////////////////////////////////////////////////////////////////////////////////

    testAQLWithLiteralAndOptions : function () {
      var aql = require("@arangodb").aql;
      var result = internal.db._query(
        aql`${aql.literal('RETURN 1')}`,
        {fullCount: true}
      );
      assertEqual([ 1 ], result.toArray());
      assertEqual(1, result.getExtra().stats.fullCount);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _executeTransaction
////////////////////////////////////////////////////////////////////////////////

    testExecuteTransaction1 : function () {
      var result = internal.db._executeTransaction({
        collections: { },
        action: function (params) {
          return params.v1 + params.v2;
        },
        params: {
          "v1": 1,
          "v2": 2
        }
      });

      assertEqual(3, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _executeTransaction
////////////////////////////////////////////////////////////////////////////////

    testExecuteTransaction2 : function () {
      var result = internal.db._executeTransaction({
        collections: { },
        action: "function () { return params.v1[0] - params.v1[1]; }",
        params: {
          "v1": [ 10, 4 ],
        }
      });

      assertEqual(6, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _databases function
////////////////////////////////////////////////////////////////////////////////

    testListDatabases : function () {
      var actual, n;

      assertEqual("_system", internal.db._name());

      actual = internal.db._databases();
      assertTrue(Array.isArray(actual));
      n = actual.length;
      assertTrue(n > 0);
      assertTrue( ( function () {
        for (var i = 0; i < actual.length; ++i) {
          if (actual[i] === "_system") {
            return true;
          }
        }
        return false;
      }() ) );

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err) {
      }

      internal.db._createDatabase("UnitTestsDatabase0");

      actual = internal.db._databases();
      assertTrue(Array.isArray(actual));
      assertEqual(n + 1, actual.length);
      assertTrue( ( function () {
        for (var i = 0; i < actual.length; ++i) {
          if (actual[i] === "UnitTestsDatabase0") {
            return true;
          }
        }
        return false;
      }() ) );

      internal.db._dropDatabase("UnitTestsDatabase0");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseWithUsers1 : function () {
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      // empty users
      assertTrue(internal.db._createDatabase("UnitTestsDatabase0", { }, [ ]));

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseWithUsers2 : function () {
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      var users = [
        { username: "admin", passwd: "secret", extra: { gender: "m" } },
        { username: "foo", active: false, extra: { gender: "f" } }
      ];

      assertTrue(internal.db._createDatabase("UnitTestsDatabase0", { }, users));

      var userManager = require("@arangodb/users");
      var user = userManager.document("admin");

      assertEqual("admin", user.user);
      assertTrue(user.active);
      assertEqual("m", user.extra.gender);

      user = userManager.document("foo");
      assertEqual("foo", user.user);
      assertFalse(user.active);
      assertEqual("f", user.extra.gender);

      assertEqual("rw", userManager.permission("admin")["UnitTestsDatabase0"]);
      assertEqual("rw", userManager.permission("foo")["UnitTestsDatabase0"]);

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseWithUsers3 : function () {
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      var users = [
        { username: "admin", passwd: "secret", active: true, extra: { gender: "m" } },
        { username: "admin", passwd: "", active: false, extra: { gender: "m" } },
      ];
      assertTrue(internal.db._createDatabase("UnitTestsDatabase0", { }, users));

      var userManager = require("@arangodb/users");
      var user = userManager.document("admin");
      assertEqual("admin", user.user);
      assertTrue(user.active);
      assertEqual("m", user.extra.gender);
      
      assertEqual("rw", userManager.permission("admin")["UnitTestsDatabase0"]);

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseInvalidName : function () {
      assertEqual("_system", internal.db._name());

      [ "", " ", "-", "0", "99999", ":::", "fox::", "test!" ].forEach (function (d) {
        try {
          internal.db._createDatabase(d);
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseNonSystem : function () {
      assertEqual("_system", internal.db._name());
      assertTrue(internal.db._isSystem());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      internal.db._createDatabase("UnitTestsDatabase0");
      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());
      assertFalse(internal.db._isSystem());

      // creation of new databases should fail here
      try {
        internal.db._createDatabase("UnitTestsDatabase1");
        fail();
      } catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err2.errorNum);
      }

      // removing a database should fail here
      try {
        internal.db._dropDatabase("UnitTestsDatabase1");
        fail();
      } catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err3.errorNum);
      }

      internal.db._useDatabase("_system");
      assertTrue(internal.db._isSystem());
      internal.db._dropDatabase("UnitTestsDatabase0");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseDuplicate : function () {
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      internal.db._createDatabase("UnitTestsDatabase0");

      try {
        internal.db._createDatabase("UnitTestsDatabase0");
        fail();
      } catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err2.errorNum);
      }

      internal.db._dropDatabase("UnitTestsDatabase0");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _createDatabase function
////////////////////////////////////////////////////////////////////////////////

    testCreateDatabaseCaseSensitivity : function () {
      assertEqual("_system", internal.db._name());

      var getCollections = function () {
        var result = [ ];

        internal.db._collections().forEach(function (c) {
          if (c.name()[0] !== "_") {
            result.push(c.name());
          }
        });
        result.sort();

        return result;
      };

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      try {
        internal.db._dropDatabase("UNITTESTSDATABASE0");
      } catch (err2) {
      }

      assertTrue(internal.db._createDatabase("UnitTestsDatabase0"));
      assertTrue(internal.db._createDatabase("UNITTESTSDATABASE0"));

      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());
      var c1 = internal.db._create("test1");
      assertNotEqual(-1, getCollections().indexOf("test1"));
      c1.save({ "_key": "foo" });
      assertEqual(1, internal.db._collection("test1").count());
      assertEqual(1, c1.count());


      internal.db._useDatabase("UNITTESTSDATABASE0");
      assertEqual("UNITTESTSDATABASE0", internal.db._name());
      var c2 = internal.db._create("test1");
      assertNotEqual(-1, getCollections().indexOf("test1"));
      c2.save({ "_key": "foo" });
      c2.save({ "_key": "bar" });
      c2.save({ "_key": "baz" });
      assertEqual(3, internal.db._collection("test1").count());
      assertEqual(3, c2.count());

      c1.remove("foo");
      assertEqual(0, c1.count());
      assertEqual(3, c2.count());
      assertEqual(3, internal.db._collection("test1").count());

      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual(0, c1.count());
      assertEqual(3, c2.count());
      assertEqual(0, internal.db._collection("test1").count());
      c1.drop();
      c1 = null;

      internal.db._useDatabase("UNITTESTSDATABASE0");
      assertEqual(3, internal.db._collection("test1").count());
      assertEqual(3, c2.count());
      c2.remove("foo");
      assertEqual(2, internal.db._collection("test1").count());
      assertEqual(2, c2.count());

      internal.db._useDatabase("_system");
      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
      assertTrue(internal.db._dropDatabase("UNITTESTSDATABASE0"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _useDatabase function
////////////////////////////////////////////////////////////////////////////////

    testUseDatabase : function () {
      assertEqual("_system", internal.db._name());
      assertTrue(internal.db._isSystem());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      internal.db._createDatabase("UnitTestsDatabase0");
      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());
      assertFalse(internal.db._isSystem());

      internal.db._useDatabase("UnitTestsDatabase0");
      assertEqual("UnitTestsDatabase0", internal.db._name());
      assertFalse(internal.db._isSystem());

      internal.db._useDatabase("_system");
      assertEqual("_system", internal.db._name());
      assertTrue(internal.db._isSystem());

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));

      try {
        internal.db._useDatabase("UnitTestsDatabase0");
        fail();
      } catch (err2) {
        assertTrue(err2.errorNum === ERRORS.ERROR_ARANGO_DATABASE_NOT_FOUND.code ||
                   err2.errorNum === ERRORS.ERROR_HTTP_NOT_FOUND.code);
      }

      assertEqual("_system", internal.db._name());

      try {
        internal.db._useDatabase("THISDATABASEDOESNOTEXIST");
        fail();
      } catch (err3) {
        assertTrue(err3.errorNum === ERRORS.ERROR_ARANGO_DATABASE_NOT_FOUND.code ||
                   err3.errorNum === ERRORS.ERROR_HTTP_NOT_FOUND.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _dropDatabase function
////////////////////////////////////////////////////////////////////////////////

    testDropDatabase : function () {
      assertEqual("_system", internal.db._name());

      try {
        internal.db._dropDatabase("UnitTestsDatabase0");
      } catch (err1) {
      }

      var isContained = function (name) {
        var l = internal.db._databases();
        for (var i = 0; i < l.length; ++i) {
          if (l[i] === name) {
            return true;
          }
        }
        return false;
      };

      internal.db._createDatabase("UnitTestsDatabase0");
      assertTrue(isContained("UnitTestsDatabase0"));
      assertTrue(isContained("_system"));

      assertTrue(internal.db._dropDatabase("UnitTestsDatabase0"));
      assertFalse(isContained("UnitTestsDatabase0"));
      assertTrue(isContained("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection cache in the face of collection deletion
////////////////////////////////////////////////////////////////////////////////

    testCollectionsCache1 : function () {
      var db = internal.db;
      var name = "UnitTestsCollectionCache";

      db._drop(name);

      db._create(name);

      db[name].save({ _key: "test", value: 1 });
      var cid = db[name]._id;
      assertEqual(name, db[name].name());
      assertEqual(1, db[name].count());
      assertEqual(1, db[name].document("test").value);

      // remove the collection and re-create a new one with the same name
      db._drop(name);

      db._create(name);
      db[name].save({ _key: "foo", value: 1 });
      db[name].save({ _key: "test", value: 2 });

      assertNotEqual(cid, db[name]._id);
      assertEqual(name, db[name].name());
      assertEqual(2, db[name].count());
      assertEqual(1, db[name].document("foo").value);
      assertEqual(2, db[name].document("test").value);

      db._drop(name);

      try {
        db[name].save({ _key: "foo", value: 1 });
        fail();
      } catch (err) {
        // cannot call method ... of undefined
        assertMatch(/^Cannot read property 'save' of undefined/, err.message);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection cache in the face of database changes
////////////////////////////////////////////////////////////////////////////////

    testCollectionsCache2 : function () {
      var db = internal.db;
      var name = "UnitTestsCollectionCache";

      assertTrue("_system", db._name());

      db._drop(name);
      db._create(name);

      db[name].save({ _key: "foo", value: 1 });
      var cid = db[name]._id;
      assertEqual(name, db[name].name());
      assertEqual(1, db[name].count());

      // switch the database
      db._createDatabase("UnitTestsDatabase0");
      db._useDatabase("UnitTestsDatabase0");

      // collection should not yet exist in other database
      try {
        db[name].save({ _key: "foo", value: 1 });
        fail();
      } catch (err) {
        // cannot call method ... of undefined
        assertMatch(/^Cannot read property 'save' of undefined/, err.message);
      }

      db._create(name);
      assertNotEqual(cid, db[name]._id);
      assertEqual(name, db[name].name());
      assertEqual(0, db[name].count());

      db[name].save({ _key: "foo", value: 1 });
      db[name].save({ _key: "bar", value: 1 });
      assertEqual(2, db[name].count());

      db._useDatabase("_system");
      assertEqual(1, db[name].count());
      db._drop(name);

      db._dropDatabase("UnitTestsDatabase0");
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(DatabaseSuite);

return jsunity.done();
