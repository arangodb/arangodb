/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertTrue, ARGUMENTS, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the sync method of the replication
// /
// / Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / DISCLAIMER
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const db = arangodb.db;
const replication = require('@arangodb/replication');
const leaderEndpoint = arango.getEndpoint();
const followerEndpoint = ARGUMENTS[ARGUMENTS.length - 1];
const errors = require("internal").errors;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

const cn = 'UnitTestsReplication';

const connectToLeader = function () {
  reconnectRetry(leaderEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const connectToFollower = function () {
  reconnectRetry(followerEndpoint, db._name(), 'root', '');
  db._flushCache();
};
  
const setFailurePoint = function(which) {
  let res = arango.PUT("/_admin/debug/failat/" + encodeURIComponent(which), {});
  if (res !== true) {
    throw "unable to set failure point '" + which + "'";
  }
};

const clearFailurePoints = function() {
  arango.DELETE("/_admin/debug/failat");
};

function BaseTestConfig () {
  'use strict';
  
  let checkCountConsistency = function(cn, expected) {
    let check = function() {
      db._flushCache();
      let c = db[cn];
      let figures = c.figures(true).engine;

      assertEqual(expected, c.count());
      assertEqual(expected, c.toArray().length);
      assertEqual(expected, figures.documents);
      figures.indexes.forEach((idx) => {
        assertEqual(expected, idx.count);
      });
    };

    connectToFollower();
    check();
    connectToLeader();
    check();
  };
  
  let runRandomOps = function(cn, n) {
    let state = {};
    let c = db[cn];
    for (let i = 0; i < n; ++i) {
      let key = "testmann" + Math.floor(Math.random() * 10);
      if (state.hasOwnProperty(key)) {
        if (Math.random() >= 0.666) {
          // remove
          c.remove(key);
          delete state[key];
        } else {
          c.replace(key, { value: ++state[key] });
        }
      } else {
        state[key] = Math.floor(Math.random() * 10000);
        c.insert({ _key: key, value: state[key] });
      }
    };

    return state;
  };

  let runRandomOpsTransaction = function(cn, n) {
    return db._executeTransaction({
      collections: { write: cn },
      action: function(params) {
        let db = require("@arangodb").db;
        let state = {};
        let c = db[params.cn];
        for (let i = 0; i < params.n; ++i) {
          let key = "testmann" + Math.floor(Math.random() * 10);
          if (state.hasOwnProperty(key)) {
            if (Math.random() >= 0.666) {
              // remove
              c.remove(key);
              delete state[key];
            } else {
              c.replace(key, { value: ++state[key] });
            }
          } else {
            state[key] = Math.floor(Math.random() * 10000);
            c.insert({ _key: key, value: state[key] });
          }
        };

        return state;
      },
      params: { cn, n }
    });
  };

  return {

    tearDown: function () {
      connectToFollower();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);

      connectToLeader();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);
    },

    // create different state on follower
    testDowngradeManyRevisions: function () {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        docs.push({ value: i });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      db._query("FOR doc IN " + cn + " REPLACE doc WITH { value: doc.value + 1 } IN " + cn);
      
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);

      checkCountConsistency(cn, 100000);
    },
    
    // create different state on follower
    testRewriteEntireFollowerSecondaryUniqueIndexes: function () {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        docs.push({ value: i });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }

      c.ensureIndex({ type: "hash", fields: ["value"], unique: true });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      db._query("FOR doc IN " + cn + " REPLACE doc WITH { value: doc.value + 1000000 } IN " + cn);
      
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);

      checkCountConsistency(cn, 100000);
    },
    
    testUniqueConstraints: function () {
      let c = db._create(cn);
      c.ensureIndex({type: "hash", unique: true, fields: ["x"]});
      c.ensureIndex({type: "hash", unique: true, fields: ["y"]});
      c.ensureIndex({type: "hash", unique: true, fields: ["z"]});

      let docs = [];
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        docs.push({ _key: "K" + i, value: i, x: i, y: i, z: i });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      db._query("FOR doc IN " + cn + " SORT doc.value REPLACE doc WITH { value: doc.value - 1, x: doc.x + 1000000, y: doc.y + 1000000, z: doc.z + 1000000} IN " + cn);
      
      docs = [];
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        docs.push({ _key: "L" + i, value: i, x: i, y: i, z: i });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }

      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);

      checkCountConsistency(cn, 1 * 100 * 1000);

      // Now check that the unique index entries are all in place by
      // provoking violations:
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", x: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", y: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", z: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      // And use the index entries in a query:
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn}
             FILTER doc.x == i 
             RETURN doc._key`).toArray().length);
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn}
             FILTER doc.y == i 
             RETURN doc._key`).toArray().length);
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn} 
             FILTER doc.z == i 
             RETURN doc._key`).toArray().length);
    },
    
    testManyUniqueConstraints: function () {
      let c = db._create(cn);
      c.ensureIndex({type: "hash", unique: true, fields: ["x"]});
      c.ensureIndex({type: "hash", unique: true, fields: ["y"]});
      c.ensureIndex({type: "hash", unique: true, fields: ["z"]});

      let docs = [];
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        docs.push({ _key: "K" + i, value: i, x: i, y: i, z: i });
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      db._query("FOR doc IN " + cn + " SORT doc.value REPLACE doc WITH { value: doc.value - 1, x: doc.x - 1, y: doc.y - 2, z: doc.z - 3 } IN " + cn);
      
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);

      checkCountConsistency(cn, 1 * 100 * 1000);

      // Now check that the unique index entries are all in place by
      // provoking violations:
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", x: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", y: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      for (let i = 0; i < 1 * 100 * 1000; ++i) {
        try {
          c.insert({ _key: "N", z: i });
          fail();
        } catch(e) {
          assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                      e.errorNum);
        }
      }
      // And use the index entries in a query:
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn} 
             FILTER doc.x == i 
             RETURN doc._key`).toArray().length);
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn} 
             FILTER doc.y == i 
             RETURN doc._key`).toArray().length);
      assertEqual(100000, db._query(
        `FOR i IN 1..100000
           FOR doc IN ${cn} 
             FILTER doc.z == i 
             RETURN doc._key`).toArray().length);
    },
    
    testRevisionIdReuse: function () {
      let c = db._create(cn);
      let rev = c.insert({_key: "testi", value: 1 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
    
      // document on follower must be 100% identical
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      c.remove("testi");
      c.insert({_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);
      c.remove("testi");

      setFailurePoint("Insert::useRev");
      c.insert({ _key: "testi", value: 1, _rev: rev });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      // document on follower must be 100% identical
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      checkCountConsistency(cn, 1);
    },
   
    // create different state on follower for the same key
    testDifferentKeyStateOnFollower: function () {
      let c = db._create(cn);
      let rev = c.insert({_key: "testi", value: 1 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      // document on follower must be 100% identical
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      c.remove("testi");
      c.insert({_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);
      c.remove("testi");
      rev = c.insert({ _key: "testi", value: 3 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      checkCountConsistency(cn, 1);
    },
    
    // create different state on follower for the same key, using replace
    testDifferentKeyStateOnFollowerUsingReplace: function () {
      let c = db._create(cn);
      let rev = c.insert({_key: "testi", value: 1 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
     
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      c.replace("testi", {_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);
      rev = c.replace("testi", { _key: "testi", value: 3 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);
      
      checkCountConsistency(cn, 1);
    },
      
    // create large AQL operation on leader using AQL
    testCreateLargeAQLOnLeader: function () {
      let c = db._create(cn);
      let rev = c.insert({_key: "testi", value: 1 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);
      
      c.replace("testi", {_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      setFailurePoint("TransactionState::intermediateCommitCount1000");
      db._query("FOR i IN 1..10000 INSERT { _key: CONCAT('testmann', i) } INTO " + cn);
      
      rev = c.replace("testi", { _key: "testi", value: 3 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      checkCountConsistency(cn, 10001);
    },
    
    // create large AQL operation on follower using AQL
    testCreateLargeAQLOnFollower: function () {
      let c = db._create(cn);
      let rev = c.insert({_key: "testi", value: 1 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);
      
      c.replace("testi", {_key: "testi", value: 2 });
      
      setFailurePoint("TransactionState::intermediateCommitCount1000");
      db._query("FOR i IN 1..10000 INSERT { _key: CONCAT('testmann', i) } INTO " + cn);

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      rev = c.replace("testi", { _key: "testi", value: 3 })._rev;

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);
      assertEqual(rev, c.document("testi")._rev);

      checkCountConsistency(cn, 1);
    },
    
    // create random operations on leader, with many
    // operations affected the same keys
    testCreateRandomOperationsOnLeader: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      let state = runRandomOps(cn, 50000);
      
      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      let expected = 1;
      for (let s in state) {
        ++expected;
        assertEqual(state[s], c.document(s).value);
      }
      
      checkCountConsistency(cn, expected);
    },
    
    // create a transaction with random operations on leader, with many
    // operations affected the same keys
    testCreateRandomOperationsTransactionOnLeader: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      let state = runRandomOpsTransaction(cn, 50000);
      
      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      let expected = 1;
      for (let s in state) {
        ++expected;
        assertEqual(state[s], c.document(s).value);
      }
      
      checkCountConsistency(cn, expected);
    },
    
    // create random operations on follower, with many
    // operations affected the same keys
    testCreateRandomOperationsOnFollower: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });
      
      runRandomOps(cn, 50000);

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      checkCountConsistency(cn, 1);
    },
    
    // create a transaction with random operations on follower, with many
    // operations affected the same keys
    testCreateRandomOperationsTransactionOnFollower: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });
      
      runRandomOpsTransaction(cn, 50000);

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      checkCountConsistency(cn, 1);
    },
    
    // create random operations on leader and follower, with many
    // operations affected the same keys
    testCreateRandomOperationsOnLeaderAndFollower: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });
      
      runRandomOps(cn, 50000);

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      let state = runRandomOps(cn, 50000);
      
      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      let expected = 1;
      for (let s in state) {
        ++expected;
        assertEqual(state[s], c.document(s).value);
      }
      
      checkCountConsistency(cn, expected);
    },
    
    // create a transaction with random operations on leader and follower, with many
    // operations affected the same keys
    testCreateRandomOperationsTransactionOnLeaderAndFollower: function () {
      let c = db._create(cn);
      c.insert({_key: "testi", value: 1 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true
      });
      db._flushCache();
      c = db._collection(cn);
      
      assertEqual(1, c.document("testi").value);
      
      c.replace("testi", {_key: "testi", value: 2 });
      
      runRandomOpsTransaction(cn, 50000);

      connectToLeader();
      db._flushCache();
      c = db._collection(cn);

      let state = runRandomOpsTransaction(cn, 50000);
      
      c.replace("testi", { _key: "testi", value: 3 });

      connectToFollower();
      replication.syncCollection(cn, {
        endpoint: leaderEndpoint,
        verbose: true,
        incremental: true
      });

      db._flushCache();
      c = db._collection(cn);
      assertEqual(3, c.document("testi").value);

      let expected = 1;
      for (let s in state) {
        ++expected;
        assertEqual(state[s], c.document(s).value);
      }
      
      checkCountConsistency(cn, expected);
    }, 
  };
}

function ReplicationIncrementalMalarkeyOldFormat () {
  'use strict';

  let suite = {
    setUp: function () {
      connectToFollower();
      // clear all failure points, but enforce old-style collections
      clearFailurePoints();
      setFailurePoint("disableRevisionsAsDocumentIds");

      connectToLeader();
      // clear all failure points, but enforce old-style collections
      clearFailurePoints();
      setFailurePoint("disableRevisionsAsDocumentIds");
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_OldFormat');
  return suite;
}

function ReplicationIncrementalMalarkeyNewFormat () {
  'use strict';

  let suite = {
    setUp: function () {
      connectToFollower();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);

      connectToLeader();
      // clear all failure points
      clearFailurePoints();
      db._drop(cn);
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_NewFormat');
  return suite;
}

let res = arango.GET("/_admin/debug/failat");
if (res === true) {
  // tests only work when compiled with -DUSE_FAILURE_TESTS
  jsunity.run(ReplicationIncrementalMalarkeyOldFormat);
  jsunity.run(ReplicationIncrementalMalarkeyNewFormat);
}

return jsunity.done();
