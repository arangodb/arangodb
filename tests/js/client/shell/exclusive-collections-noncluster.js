/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, print */

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
/// @author Jan Christoph Uhde
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const ct = require('@arangodb/testutils/client-tools');
var jsunity = require("jsunity");
const sleep = require('internal').sleep;

var arangodb = require("@arangodb");
var db = arangodb.db;
var IM = global.instanceManager;
var ERRORS = arangodb.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ExclusiveSuite () {
  var cn1 = "UnitTestsExclusiveCollection1"; // used for test data
  var cn2 = "UnitTestsExclusiveCollection2"; // used for communication
  var c1, c2;
  let joinShells = function(shells) {
    let count = 0;
    while (ct.run.joinFinishedBGShells(IM.options, shells) !== 0){
      if (count > 60) {
        ct.run.joinForceBGShells(IM.options, shells);
        throw new Error("sub-shells did not get ready on time");
      }
      count += 1;
      sleep(1);
      print('.');
    }
  };      

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    testExclusiveExpectConflict : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let fn1 = function() {
        let db = require('internal').db;
        let c2 = db[args.cn2];
        c2.insert({ _key: "runner1", value: false });
              
        while (!c2.exists("runner2")) {
          require("internal").sleep(0.02);
        }
        
        let trx = db._createTransaction({
          collections: {
            write: [args.cn1, args.cn2],
            exclusive: [ ]
          }
        });
        let ct1 = trx.collection(args.cn1);
        let ct2 = trx.collection(args.cn2);
        for (let i = 0; i < 10000; ++i) {
          ct1.update("XXX", { name : "runner1" });
        }
        ct2.update("runner1", { value: true });
        print('committing spawned');
        trx.commit();
      };
      let shells = [];
      ct.run.spawnStressArangoshInBG(shells, fn1, 'xx', 1, {cn1, cn2});
      db.UnitTestsExclusiveCollection2.insert({ _key: "runner2", value: false });
      while (!c2.exists("runner1")) {
        sleep(0.02);
      }
      try {
        let trx = db._createTransaction({
          collections: {
            write: [cn1, cn2],
            exclusive: [ ]
          }
        });
        let ct1 = trx.collection(cn1);
        let ct2 = trx.collection(cn2);
        for (let i = 0; i < 10000; ++i) {
          ct1.update("XXX", { name : "runner2" });
        }
        ct2.update("runner2", { value: true });
        print('commiting local');
        trx.commit();
      } catch (err) {
        print('error with local commit');
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      joinShells(shells);

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },

    testExclusiveExpectNoConflict : function () {
      assertEqual(0, c2.count());
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let fn1 = function() {
        let db = require("internal").db;
        let c2 = db[args.cn2];
        c2.insert({ _key: "runner1", value: false });
        while (!c2.exists("runner2")) {
          require("internal").sleep(0.02);
        }

        let trx = db._createTransaction({
          collections: {
            exclusive: [args.cn1, args.cn2],
          }
        });
        let ct1 = trx.collection(args.cn1);
        let ct2 = trx.collection(args.cn2);
        for (let i = 0; i < 10000; ++i) {
          ct1.update("XXX", { name : "runner1" });
        }
        ct2.update("runner1", { value: true });
        trx.commit();
      };
      let shells = [];
      ct.run.spawnStressArangoshInBG(shells, fn1, 'xx', 1, {cn1, cn2});

      c2.insert({ _key: "runner2", value: false });
      while (!c2.exists("runner1")) {
        require("internal").sleep(0.02);
      }

      let trx = db._createTransaction({
        collections: {
          exclusive: [cn1, cn2],
        }
      });
      let ct1 = trx.collection(cn1);
      let ct2 = trx.collection(cn2);
      for (let i = 0; i < 10000; ++i) {
        ct1.update("XXX", { name : "runner2" });
      }
      ct2.update("runner2", { value: true });
      trx.commit();

      joinShells(shells);
      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    },

    testExclusiveExpectConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let fn1 = function() {
        var arangodb = require("@arangodb");
        var ERRORS = arangodb.errors;
        let db = require("internal").db;
        let c2 = db[args.cn2];
        c2.insert({ _key: "runner1", value: false });
        while (!c2.exists("runner2")) {
          require("internal").sleep(0.02);
        }
        try {
          for (let i = 0; i < 1000; ++i) {
            db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN @@cn", {'@cn': args.cn1});
          }
          c2.update("runner1", { value: true });
        } catch (err) {
          print('error with spawned queries');
          if (ERRORS.ERROR_ARANGO_CONFLICT.code != err.errorNum) {
            throw(err);
          }
        }
      };
      
      let shells = [];
      ct.run.spawnStressArangoshInBG(shells, fn1, 'xx', 1, {cn1, cn2});

      try {
        c2.insert({ _key: "runner2", value: false });
        while (!c2.exists("runner1")) {
          require("internal").sleep(0.02);
        }
        let trx = db._createTransaction({
          collections: {
            write: [cn1, cn2],
            exclusive: [ ]
          }
        });
        for (let i = 0; i < 10000; i++) {
          db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN @@cn", {'@cn': cn1});
        }
        c2.update("runner2", { value: true });
      } catch (err) {
        print('error with local queries');
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      joinShells(shells);

      // only one transaction should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertNotEqual(docs[0].value, docs[1].value);
    },
    
    testExclusiveExpectNoConflictAQL : function () {
      c1.insert({ "_key" : "XXX" , "name" : "initial" });
      let fn1 = function() {
        let db = require("internal").db;
        let c2 = db[args.cn2];
        c2.insert({ _key: "runner1", value: false });
        while (!c2.exists("runner2")) {
          require("internal").sleep(0.02);
        }
        print('spawned start');
        for (let i = 0; i < 1000; ++i) {
          db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner1' } UPDATE { name: 'runner1' } IN @@col OPTIONS { exclusive: true }", {"@col": args.cn1});
        }
        print('spawned done');
        c2.update("runner1", { value: true });
      };
      
      let shells = [];
      ct.run.spawnStressArangoshInBG(shells, fn1, 'xx', 1, {cn1, cn2});

      c2.insert({ _key: "runner2", value: false });
      while (!c2.exists("runner1")) {
        require("internal").sleep(0.02);
      }
      print('local start');
      for (let i = 0; i < 10000; i++) {
        db._query("UPSERT { _key: 'XXX' } INSERT { name: 'runner2' } UPDATE { name: 'runner2' } IN @@col OPTIONS { exclusive: true }", {"@col": cn1});
      }
      print('local done');
      c2.update("runner2", { value: true });

      joinShells(shells);

      // both transactions should have succeeded
      assertEqual(2, c2.count());
      let docs = c2.toArray().sort(function(l, r) { return l._key < r._key; });
      assertTrue(docs[0].value);
      assertTrue(docs[1].value);
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ExclusiveSuite);

return jsunity.done();
