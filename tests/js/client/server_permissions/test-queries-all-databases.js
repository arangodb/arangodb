/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertTrue, assertFalse, assertEqual, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'runSetup': true
  };
}

if (runSetup === true) {
  let users = require("@arangodb/users");

  users.save("test_rw", "testi");
  users.grantDatabase("test_rw", "_system", "rw");

  return true;
}

let jsunity = require('jsunity');
let queries = require('@arangodb/aql/queries');

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;
  const errors = require('@arangodb').errors;
  const cn = "UnitTestsDatabase";

  return {
    setUp: function() {
      db._useDatabase('_system');
      try {
        db._dropDatabase(cn);
      } catch (err) {}

      db._createDatabase(cn);
    },

    tearDown: function() {
      db._useDatabase('_system');
      db._dropDatabase(cn);
    },
    
    testQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      let result = queries.current();
      // must succeed
      assertTrue(Array.isArray(result));
    },

    testAllQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      try {
        queries.current(true);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
      }
    },
    
    testAllQueriesFromSystemDatabase : function() {
      let result = queries.current(true);
      // must succeed
      assertTrue(Array.isArray(result));
    },
    
    testAllQueriesWithUnprivilegedUser : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      try {
        db._useDatabase(cn);
        try {
          queries.current(true);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
        }
      } catch (err) {
        arango.reconnect(endpoint, db._name(), "root", "");
      }
    },
    
    testSlowQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      let result = queries.slow();
      // must succeed
      assertTrue(Array.isArray(result));
    },

    testAllSlowQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      try {
        queries.slow(true);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
      }
    },
    
    testAllSlowQueriesFromSystemDatabase : function() {
      let result = queries.slow(true);
      // must succeed
      assertTrue(Array.isArray(result));
    },
    
    testAllSlowQueriesWithUnprivilegedUser : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      try {
        db._useDatabase(cn);
        try {
          queries.slow(true);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
        }
      } catch (err) {
        arango.reconnect(endpoint, db._name(), "root", "");
      }
    },
    
    testClearSlowQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      let result = queries.clearSlow();
      assertFalse(result.error);
      assertEqual(200, result.code);
    },
    
    testClearAllSlowQueriesFromNonSystemDatabase : function() {
      db._useDatabase(cn);
      try {
        queries.clearSlow(true);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
      }
    },
    
    testClearAllSlowQueriesFromSystemDatabase : function() {
      let result = queries.clearSlow(true);
      // must succeed
      assertFalse(result.error);
      assertEqual(200, result.code);
    },
    
    testClearAllSlowQueriesWithUnprivilegedUser : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      try {
        db._useDatabase(cn);
        try {
          queries.clearSlow(true);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code, err.errorNum);
        }
      } catch (err) {
        arango.reconnect(endpoint, db._name(), "root", "");
      }
    },

  };
}
jsunity.run(testSuite);
return jsunity.done();
