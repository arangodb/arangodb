/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

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
    'query.require-with': 'true'
  };
}

const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('@arangodb').errors;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const isCluster = require('internal').isCluster;
const isEnterprise = require('internal').isEnterprise;
  
const vn1 = "UnitTestsVertex1";
const vn2 = "UnitTestsVertex2";
const en = "UnitTestsEdge";

let createData = function() {
  let v1 = db._create(vn1, { numberOfShards: 3 });
  let v2 = db._create(vn2, { numberOfShards: 3 });
  let e = db._createEdgeCollection(en, { numberOfShards: 3 });

  const n = 10;
  let docs = [];
  for (let i = 0; i < n; ++i) {
    docs.push({ _key: "test" + i });
  }
  v1.insert(docs);
  v2.insert(docs);

  docs = [];
  for (let i = 0; i < n; ++i) {
    docs.push({ _from: vn1 + "/test" + i, _to: vn1 + "/test" + (n - i - 1) });
    docs.push({ _from: vn2 + "/test" + i, _to: vn2 + "/test" + (n - i - 1) });
  }
 
  // insert 2 vertices that cross the tracks
  e.insert({ _from: vn1 + "/test99", _to: vn2 + "/test0" });
  e.insert({ _from: vn2 + "/test99", _to: vn1 + "/test0" });
  e.insert(docs);
};

function BaseTestConfig () {
  return {
    testTraversalDisallowed : function() {
      // no WITH
      try {
        db._query("FOR v,e,p IN 1..1 OUTBOUND '" + vn1 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // false WITH
      try {
        db._query("WITH " + vn2 + " FOR v,e,p IN 1..1 OUTBOUND '" + vn1 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (1)
      try {
        db._query("WITH " + vn1 + " FOR v,e,p IN 0..1 OUTBOUND '" + vn1 + "/test99' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (2)
      try {
        db._query("WITH " + vn1 + " FOR v,e,p IN 0..1 OUTBOUND '" + vn2 + "/test99' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (3)
      try {
        db._query("WITH " + vn2 + " FOR v,e,p IN 0..1 OUTBOUND '" + vn1 + "/test99' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (4)
      try {
        db._query("WITH " + vn2 + " FOR v,e,p IN 0..1 OUTBOUND '" + vn2 + "/test99' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
    },
    
    testShortestPathDisallowed : function() {
      // no WITH
      try {
        db._query("FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // false WITH
      try {
        db._query("WITH " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (1)
      try {
        db._query("WITH " + vn1 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (2)
      try {
        db._query("WITH " + vn1 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (3)
      try {
        db._query("WITH " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (4)
      try {
        db._query("WITH " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN v").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
    },
    
    testKPathsDisallowed : function() {
      // no WITH
      try {
        db._query("FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // false WITH
      try {
        db._query("WITH " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (1)
      try {
        db._query("WITH " + vn1 + " FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (2)
      try {
        db._query("WITH " + vn1 + " FOR p IN OUTBOUND K_PATHS '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }

      // incomplete WITH (3)
      try {
        db._query("WITH " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
      
      // incomplete WITH (4)
      try {
        db._query("WITH " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN p").toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
      }
    },
        
    testTraversalWithWith : function() {
      let result = db._query("WITH " + vn1 + " FOR v,e,p IN 1..3 OUTBOUND '" + vn1 + "/test0' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
      
      result = db._query("WITH " + vn2 + " FOR v,e,p IN 1..3 OUTBOUND '" + vn2 + "/test0' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR v,e,p IN 1..1 OUTBOUND '" + vn1 + "/test99' " + en + " RETURN v").toArray();
      assertEqual(1, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR v,e,p IN 1..1 OUTBOUND '" + vn2 + "/test99' " + en + " RETURN v").toArray();
      assertEqual(1, result.length);
    },
    
    testShortestPathWithWith : function() {
      let result = db._query("WITH " + vn1 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
      
      result = db._query("WITH " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn2 + "/test0' TO '" + vn2 + "/test9' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR v,e IN OUTBOUND SHORTEST_PATH '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN v").toArray();
      assertEqual(2, result.length);
    },
    
    testKPathsWithWith : function() {
      let result = db._query("WITH " + vn1 + " FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test0' TO '" + vn1 + "/test9' " + en + " RETURN p").toArray();
      assertEqual(1, result.length);
      
      result = db._query("WITH " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn2 + "/test0' TO '" + vn2 + "/test9' " + en + " RETURN p").toArray();
      assertEqual(1, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn1 + "/test99' TO '" + vn2 + "/test0' " + en + " RETURN p").toArray();
      assertEqual(1, result.length);
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR p IN OUTBOUND K_PATHS '" + vn2 + "/test99' TO '" + vn1 + "/test0' " + en + " RETURN p").toArray();
      assertEqual(1, result.length);
    },
    
    testDocument : function() {
      // DOCUMENT(...) does not require using WITH, as it is always running on a coordinator
      let result = db._query("FOR doc IN ['" + vn1 + "/test0', '" + vn1 + "/test1'] RETURN DOCUMENT(doc)").toArray();
      assertEqual(2, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty("_key"));
      });
      
      result = db._query("FOR doc IN ['" + vn1 + "/test0', '" + vn2 + "/test1'] RETURN DOCUMENT(doc)").toArray();
      assertEqual(2, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty("_key"));
      });
      
      // using WITH should do not harm, however
      result = db._query("WITH " + vn1 + " FOR doc IN ['" + vn1 + "/test0', '" + vn1 + "/test1'] RETURN DOCUMENT(doc)").toArray();
      assertEqual(2, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty("_key"));
      });
      
      result = db._query("WITH " + vn1 + ", " + vn2 + " FOR doc IN ['" + vn1 + "/test0', '" + vn2 + "/test1'] RETURN DOCUMENT(doc)").toArray();
      assertEqual(2, result.length);
      result.forEach((doc) => {
        assertTrue(doc.hasOwnProperty("_key"));
      });
    },
  };
}

function GenericSuite() {
  'use strict';

  let suite = {
    setUpAll: function() {
      db._drop(vn1);
      db._drop(vn2);
      db._drop(en);

      createData();
    },
    
    tearDownAll: function() {
      db._drop(vn1);
      db._drop(vn2);
      db._drop(en);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_Generic');
  return suite;
}

function OneShardSuite() {
  'use strict';

  let suite = {
    setUpAll: function() {
      try {
        db._dropDatabase("UnitTestsDatabase");
      } catch (err) {}

      db._createDatabase("UnitTestsDatabase", { sharding: "single" });
      db._useDatabase("UnitTestsDatabase");

      createData();
    },
    
    tearDownAll: function() {
      db._useDatabase("_system");
      db._dropDatabase("UnitTestsDatabase");
    },

    testOneShard: function() {
      assertEqual("single", db._properties().sharding);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OneShard');
  return suite;
}

jsunity.run(GenericSuite);
if (isCluster() && isEnterprise()) {
  jsunity.run(OneShardSuite);
}
return jsunity.done();
