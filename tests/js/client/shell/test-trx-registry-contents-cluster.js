/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, arango, BigInt, assertEqual, assertFalse, assertTrue, assertMatch */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;

const start = (new Date()).toISOString();

function RegistrySuite() { 
  const cn = "UnitTests";

  const incrementId = (id, value = 1) => {
    assertEqual("string", typeof id, id);
    assertTrue(id.length > 0, id);
    return (BigInt(id) + BigInt(value)).toString();
  };

  const runInTrx = (options, cb) => {
    let trx = db._createTransaction(options);
    try {
      cb(trx);
    } finally {
      trx.abort();
    }
  };

  const validateCoordinatorPart = (id, tx) => {
    assertEqual(id, tx.id);
    assertEqual("running", tx.state);
    assertFalse(tx.expired);
    assertEqual("managed", tx.type);
    assertEqual("coordinator id", tx.idType);
    assertFalse(tx.hasPerformedIntermediateCommits);
    assertEqual(0, tx.sideUsers);
    assertEqual("undefined", tx.finalStatus);
    assertEqual(60, tx.timeToLive);
    assertTrue(tx.expiryTime > start, {tx, start});
    assertEqual("root", tx.user);
    assertEqual(db._name(), tx.database);
    assertMatch(/^CRDN-/, tx.server);
    assertMatch(/trx type: managed/, tx.context);
    assertMatch(/multi op/, tx.context);
  };

  const validateDBServerPart = (id, tx) => {
    const isFollower = BigInt(id) % BigInt(4) === BigInt(2);

    assertEqual(id, tx.id);
    assertEqual("running", tx.state);
    assertFalse(tx.expired);
    assertEqual("managed", tx.type);
    assertEqual(isFollower ? "follower id" : "leader id", tx.idType);
    // round down to coordinator id
    assertEqual(BigInt(id) / BigInt(4) * BigInt(4), tx.coordinatorId);
    assertFalse(tx.hasPerformedIntermediateCommits);
    assertEqual(0, tx.sideUsers);
    assertEqual("undefined", tx.finalStatus);
    assertEqual(300, tx.timeToLive);
    assertTrue(tx.expiryTime > start, {tx, start});
    assertEqual(db._name(), tx.database);
    assertMatch(/^PRMR-/, tx.server);
  };

  return {
    setUpAll: function () {
      db._create(cn, {numberOfShards: 10, replicationFactor: 2});
    },

    tearDownAll: function () {
      db._drop(cn);
    },
    
    testTrxRegistryNoDetails: function () {
      runInTrx({ collections: { write: cn } }, (trx) => {
        let result = arango.GET("/_api/transaction").transactions;
        assertTrue(Array.isArray(result));

        let tx = result.filter((t) => t.id === trx.id());
        assertEqual(1, tx.length);
        assertEqual(trx.id(), tx[0].id);
        assertEqual("running", tx[0].state);
      });
    },
    
    testTrxRegistryWithDetailsOnlyLeader: function () {
      runInTrx({ collections: { write: cn } }, (trx) => {
        let result = arango.GET("/_api/transaction?details=true").transactions;
        assertTrue(Array.isArray(result));

        // coordinator part
        {
          const id = trx.id();
          let tx = result.filter((t) => t.id === id);
          assertEqual(1, tx.length);

          validateCoordinatorPart(id, tx[0]);
          assertMatch(/collections: \[.*name: UnitTests, access: write.*\]/, tx[0].context);
        }
        
        // leader part(s)
        {
          const id = incrementId(trx.id());
          let tx = result.filter((t) => t.id === id);
          assertTrue(tx.length > 0, tx);

          tx.forEach((tx) => {
            validateDBServerPart(id, tx);
          });
        }
        
        // follower part(s)
        {
          const id = incrementId(trx.id(), 2);
          let tx = result.filter((t) => t.id === id);
          assertEqual(0, tx.length, tx);
        }
      });
    },
    
    testTrxRegistryWithDetailsLeaderAndFollower: function () {
      runInTrx({ collections: { write: cn } }, (trx) => {
        // write documents into collection. this also starts the transaction
        // on the follower(s)
        let docs = [];
        for (let i = 0; i < 5000; ++i) {
          docs.push({});
        }
        trx.collection(cn).insert(docs);

        let result = arango.GET("/_api/transaction?details=true").transactions;
        assertTrue(Array.isArray(result));

        // leader part(s)
        {
          const id = incrementId(trx.id());
          let tx = result.filter((t) => t.id === id);
          assertTrue(tx.length > 0, tx);

          tx.forEach((tx) => {
            validateDBServerPart(id, tx);
          });
        }
        
        // follower part(s)
        {
          const id = incrementId(trx.id(), 2);
          let tx = result.filter((t) => t.id === id);
          assertTrue(tx.length > 0, tx);

          tx.forEach((tx) => {
            validateDBServerPart(id, tx);
          });
        }
      });
    },
    
    testTrxRegistryWithDetailsLeaderAndFollowerOtherDB: function () {
      db._createDatabase(cn);

      try {
        db._useDatabase(cn);
      
        db._create(cn, {numberOfShards: 10, replicationFactor: 2});

        runInTrx({ collections: { write: cn } }, (trx) => {
          // write documents into collection. this also starts the transaction
          // on the follower(s)
          let docs = [];
          for (let i = 0; i < 5000; ++i) {
            docs.push({});
          }
          trx.collection(cn).insert(docs);

          let result = arango.GET("/_api/transaction?details=true").transactions;
          assertTrue(Array.isArray(result));
          
          // coordinator part
          {
            const id = trx.id();
            let tx = result.filter((t) => t.id === id);
            assertEqual(1, tx.length);

            validateCoordinatorPart(id, tx[0]);
            assertMatch(/collections: \[.*name: UnitTests, access: write.*\]/, tx[0].context);
          }

          // leader part(s)
          {
            const id = incrementId(trx.id());
            let tx = result.filter((t) => t.id === id);
            assertTrue(tx.length > 0, tx);

            tx.forEach((tx) => {
              validateDBServerPart(id, tx);
            });
          }
          
          // follower part(s)
          {
            const id = incrementId(trx.id(), 2);
            let tx = result.filter((t) => t.id === id);
            assertTrue(tx.length > 0, tx);

            tx.forEach((tx) => {
              validateDBServerPart(id, tx);
            });
          }
        });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },
    
    testTrxRegistryWithExclusiveTrx: function () {
      db._createDatabase(cn);

      try {
        db._useDatabase(cn);
      
        db._create(cn, {numberOfShards: 1, replicationFactor: 1});

        runInTrx({ collections: { exclusive: cn } }, (trx) => {
          // write documents into collection
          let docs = [];
          for (let i = 0; i < 10; ++i) {
            docs.push({});
          }
          trx.collection(cn).insert(docs);

          let result = arango.GET("/_api/transaction?details=true").transactions;
          assertTrue(Array.isArray(result));
          
          // coordinator part
          {
            const id = trx.id();
            let tx = result.filter((t) => t.id === id);
            assertEqual(1, tx.length);

            validateCoordinatorPart(id, tx[0]);
            assertMatch(/collections: \[.*name: UnitTests, access: exclusive.*\]/, tx[0].context);
          }
              
          const shards = db._collection(cn).shards();
          assertEqual(1, shards.length, shards);

          // leader part(s)
          {
            const id = incrementId(trx.id());
            let tx = result.filter((t) => t.id === id);
            assertEqual(1, tx.length, tx);

            tx.forEach((tx) => {
              validateDBServerPart(id, tx);
              assertMatch(/collections: \[.*, access: exclusive.*\]/, tx.context);

              assertMatch(new RegExp(shards[0]), tx.context);
            });
          }
          
          // follower part(s) - we don't have followers because of replicationFactor 1
          {
            const id = incrementId(trx.id(), 2);
            let tx = result.filter((t) => t.id === id);
            assertEqual(0, tx.length, tx);
          }
        });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(cn);
      }
    },

  };
}

jsunity.run(RegistrySuite);
return jsunity.done();
